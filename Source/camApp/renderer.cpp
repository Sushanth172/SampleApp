#include "renderer.h"

static GLfloat mVerticesDataPosition[] = {
    -1.0f, 1.0f, 0.0f, // Position 0
    -1.0f, -1.0f, 0.0f, // Position 1
    1.0f, -1.0f, 0.0f, // Position 2
    1.0f, 1.0f, 0.0f, // Position 3
};

static GLfloat mVerticesDataTextCord[] = {
    0.0f, 0.0f, // TexCoord 0
    0.0f, 1.0f, // TexCoord 1
    1.0f, 1.0f, // TexCoord 2
    1.0f, 0.0f // TexCoord 3
};

static unsigned short mIndicesData[] = { 0, 1, 2, 0, 2, 3 };

Renderer::Renderer()
{
    image_buffer=NULL;
    m_shaderProgram = NULL;
    mjpeg_flag=false;
}

Renderer::~Renderer()
{
    if(m_shaderProgram)
    {
        m_shaderProgram->removeAllShaders();
        m_shaderProgram->release();
        if(image_buffer)
        {
            free(image_buffer);
            image_buffer = NULL;
        }
        delete m_shaderProgram;
        m_shaderProgram = NULL;
    }
}

void Renderer::device_lost()
{
    if(m_shaderProgram)
    {
        m_shaderProgram->removeAllShaders();
        m_shaderProgram->release();
        if(image_buffer)
        {
            free(image_buffer);
            image_buffer = NULL;
        }
        delete m_shaderProgram;
        m_shaderProgram = NULL;
    }
}

void Renderer::mjpeg_decompress(unsigned char *source,unsigned char *destination, long int source_size,int stride)
{
    if(source_size)
    {
        mutex.lock();
        int subsample=0,frame_height=0,frame_width=0;

        tjhandle handle=tjInitDecompress();
        if(!handle)
            perror("Failed to init decompressor");

        if(source[0] == 0xFF && source[1]==0xD8)
        {
            if(tjDecompressHeader2(handle,source,source_size,&frame_width,&frame_height,&subsample)<0)
            {
                qDebug() << "Error while decompressing header:" << tjGetErrorStr();
            }
            else
            {
                if(tjDecompress2(handle,source,source_size,destination,frame_width,stride,frame_height,TJPF_RGBA,TJFLAG_NOREALLOC)<0)
                {

                    FILE *fp=NULL;
                    fp=fopen("/home/econsys6/yuv.JPG","wb");
                    if(!fp)
                    {
                        qDebug("file not open");
                    }
                    fwrite(source,1,source_size,fp);
                    fclose(fp);
                }
            }
        }
        tjDestroy(handle);
        mutex.unlock();
    }
}


void Renderer::clear_shader(){
    if(m_shaderProgram){
        if (m_shaderProgram->isLinked()) {
            m_shaderProgram->release();
            m_shaderProgram->removeAllShaders();
        }
    }
}

void Renderer::calculateViewport(int windowHeight,int windowWidth)
{
    int original_width = renderer_width;
    int original_height = renderer_height;
    int new_width = original_width;
    int new_height = original_height;
    // first check if we need to scale width
    if (original_width > windowWidth) {
        new_width = windowWidth;
        //scale height to maintain aspect ratio
        new_height = (new_width * original_height) / original_width;
    }

    // then check if we need to scale even with the new height
    if (new_height > windowHeight) {
        new_height = windowHeight;
        //scale width to maintain aspect ratio
        new_width = (new_height * original_width) / original_height;
    }

    viewport_height = new_height;
    viewport_width = new_width;
    x = (((windowWidth - new_width)/2))+285;
    y = (((windowHeight - new_height)/2))+50;
 //   qDebug() << x << y <<viewport_height << viewport_width;
//     glViewport(x,y,viewport_width,viewport_height);
}

void Renderer::paint()
{
    glViewport(x,y,viewport_width,viewport_height);                 //setting viewport value
    drawBuffer();
}

void Renderer::getImageBuffer(unsigned char *buf)
{
    mutex.lock();
    if(buf)
    {
        //           qDebug() <<Q_FUNC_INFO<<bytesused;
        memcpy(image_buffer,buf, bytesused);

        //                FILE *fp=NULL;
        //                fp=fopen("/home/econsys6/yuv.raw","wb");
        //                if(!fp)
        //                {
        //                    qDebug("file not open");
        //                }
        //                fwrite(buf,1,bytesused,fp);
        //                fclose(fp);
    }
    mutex.unlock();
}
void Renderer::set_shaders_RGB()
{
    //    qDebug() << "rgb shader called";
    clear_shader();
    if(m_shaderProgram)
    {
        delete m_shaderProgram;
        m_shaderProgram=NULL;
    }
    image_buffer=(unsigned char*)realloc(image_buffer,(renderer_height*renderer_width*4));
    if(!image_buffer)
    {
        qDebug("RGB memory not allocated");
        return ;
    }
    memset(image_buffer,0,(renderer_height*renderer_width*4));
    if(!m_shaderProgram)
    {
        initializeOpenGLFunctions();

        m_shaderProgram = new QOpenGLShaderProgram();

        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                 "attribute vec4 a_position;\n"
                                                 "attribute vec2 a_texCoord;\n"
                                                 "varying vec2 v_texCoord;\n"
                                                 "void main()\n"
                                                 "{\n"
                                                 "gl_Position = a_position;\n"
                                                 "v_texCoord = a_texCoord;\n"
                                                 "}\n");
        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                 "#ifdef GL_ES\n"
                                                 "precision highp float;\n"
                                                 "#endif\n"

                                                 "varying vec2 v_texCoord;"
                                                 "uniform sampler2D texture;"
                                                 "vec4 color;"
                                                 "void main() {"
                                                 "color = texture2D(texture, v_texCoord);"
                                                 "gl_FragColor = color;"
                                                 "}\n");

        m_shaderProgram->bindAttributeLocation("a_position", 0);
        m_shaderProgram->bindAttributeLocation("a_texCoord", 1);
        m_shaderProgram->link();

        mPositionLoc = m_shaderProgram->attributeLocation("a_position");
        mTexCoordLoc = m_shaderProgram->attributeLocation("a_texCoord");


        glEnable(GL_TEXTURE_2D);
        samplerLoc = m_shaderProgram->uniformLocation("texture");
        glGenTextures (1, &TextureId); // Generate a texture object
        glActiveTexture(GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, TextureId);
        mjpeg_flag=true;
    }
}

void Renderer::set_shaders_UYVY()
{
    clear_shader();
    if(m_shaderProgram)
    {
        delete m_shaderProgram;
        m_shaderProgram=NULL;
    }
    image_buffer=(unsigned char*)realloc(image_buffer,(renderer_height*renderer_width*2));
    if(!image_buffer)
    {
        qDebug("UYVY memory not allocated");
        return ;
    }
    if(!m_shaderProgram)
    {
        initializeOpenGLFunctions();
        m_shaderProgram = new QOpenGLShaderProgram();

        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                 "attribute vec4 a_position;\n"
                                                 "attribute vec2 a_texCoord;\n"
                                                 "varying vec2 v_texCoord;\n"
                                                 "void main()\n"
                                                 "{\n"
                                                 "gl_Position = a_position;\n"
                                                 "v_texCoord = a_texCoord;\n"
                                                 "}\n");
        m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                 "#ifdef GL_ES\n"
                                                 "precision highp float;\n"
                                                 "#endif\n"

                                                 "varying vec2 v_texCoord;\n"
                                                 "uniform sampler2D uyvy_texture;\n"

                                                 "uniform float texel_width;"
                                                 "uniform float texture_width;"
                                                 "uniform float texture_height;"


                                                 "void main()\n"
                                                 "{\n"
                                                 "float r, g, b, y, u, v;\n"

                                                 "   vec4 luma_chroma;\n"
                                                 "   float xcoord = floor(v_texCoord.x * texture_width);\n"
                                                 "   float ycoord = floor(v_texCoord.y * texture_height);\n"

                                                 //We had put the Y values of each pixel to the R,G,B components by
                                                 //GL_LUMINANCE, that's why we're pulling it from the R component,
                                                 //we could also use G or B

                                                 "if (mod(xcoord, 2.0) == 0.0) {\n"
                                                 "   luma_chroma = texture2D(uyvy_texture, v_texCoord);\n"
                                                 "   y = luma_chroma.g;\n"
                                                 "} else {\n"
                                                 "   luma_chroma = texture2D(uyvy_texture, vec2(v_texCoord.x - texel_width, v_texCoord.y));\n"
                                                 "   y = luma_chroma.a;\n"
                                                 "}\n"
                                                 "u = luma_chroma.r - 0.5;\n"
                                                 "v = luma_chroma.b - 0.5;\n"

                                                 //We had put the U and V values of each pixel to the A and R,G,B
                                                 //components of the texture respectively using GL_LUMINANCE_ALPHA.
                                                 //Since U,V bytes are interspread in the texture, this is probably
                                                 //the fastest way to use them in the shader

                                                 //The numbers are just YUV to RGB conversion constants
                                                 "r = y + 1.13983*v;\n"
                                                 "g = y - 0.39465*u - 0.58060*v;\n"
                                                 "b = y + 2.03211*u;\n"

                                                 "gl_FragColor = vec4(r,g,b,1.0);\n"
                                                 "}\n"
                                                 );
        m_shaderProgram->bindAttributeLocation("a_position", 0);
        m_shaderProgram->bindAttributeLocation("a_texCoord", 1);
        m_shaderProgram->link();

        mPositionLoc = m_shaderProgram->attributeLocation("a_position");
        mTexCoordLoc = m_shaderProgram->attributeLocation("a_texCoord");

        glEnable(GL_TEXTURE_2D);
        // Get the sampler location
        samplerLoc = m_shaderProgram->uniformLocation("uyvy_texture");

        // Generate a texture object
        glGenTextures (1, &TextureId);
        glActiveTexture (GL_TEXTURE1);

        // Bind the texture object
        glBindTexture (GL_TEXTURE_2D, TextureId);
        mjpeg_flag=false;
    }

}

void Renderer::drawBuffer()
{
    m_shaderProgram->bind();

    glVertexAttribPointer(mPositionLoc, 3, GL_FLOAT, GL_FALSE, 12, mVerticesDataPosition);
    glVertexAttribPointer(mTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 8, mVerticesDataTextCord);

    m_shaderProgram->enableAttributeArray(0);
    m_shaderProgram->enableAttributeArray(1);

    // set active texture and give input y buffer
    glActiveTexture(GL_TEXTURE1);
    glUniform1i(samplerLoc, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    mutex.lock();

    if(mjpeg_flag)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderer_width, renderer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,image_buffer);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderer_width/2, renderer_height, 0, GL_RGBA, GL_UNSIGNED_BYTE,image_buffer);
    mutex.unlock();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, mIndicesData);

    m_shaderProgram->disableAttributeArray(0);
    m_shaderProgram->disableAttributeArray(1);

}

//void Renderer::set_picture(QImage img)
//{
//    pixmap = QPixmap::fromImage(img);
//    if(pixmap.isNull()){
//        qDebug()<<"pixmap null";
//    }
//    update();
//}

//void Renderer::paint(QPainter* painter)
//{
//    painter->drawPixmap(0,0,pixmap);
//}

