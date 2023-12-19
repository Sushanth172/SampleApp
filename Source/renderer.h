#ifndef FRAME_H
#define FRAME_H

#include <QImage>
#include <QQuickPaintedItem>
#include <QPixmap>
#include <QPainter>
#include <QDebug>
#include <QOpenGLShaderProgram>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLContext>
#include <qquickwindow.h>
#include <QQuickWindow>
#include <qquickitem.h>
#include <QMutex>
#include <turbojpeg.h>


class Renderer: public QObject, public QOpenGLFunctions
{
    Q_OBJECT
public:
    //    QPixmap pixmap;
    Renderer();
    ~Renderer();
    int renderer_width,renderer_height,x,y,viewport_width,viewport_height;
    void set_shaders_UYVY();
    void set_shaders_RGB();
    QMutex mutex;
    int bytesused;
    void mjpeg_decompress(unsigned char *source, unsigned char *destination, long int source_size,int stride);
    void paint();
    void calculateViewport(int windowHeight,int windowWidth);
    void device_lost();

signals:
    void gotbuffer();

public slots:
    //    void set_picture(QImage img);
    void getImageBuffer(unsigned char *buf);
    void drawBuffer();

private:
    bool mjpeg_flag;
    unsigned char *image_buffer;
    QOpenGLShaderProgram *m_shaderProgram;
    QQuickWindow *m_window;
    int mPositionLoc;
    int mTexCoordLoc;
    QSize m_viewportSize;
    GLint samplerLoc;
    GLuint TextureId;
    void clear_shader();
};

#endif // FRAME_H
