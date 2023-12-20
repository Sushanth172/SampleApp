#include "device.h"
#include <libudev.h>
#include <libusb-1.0/libusb.h>
QStringListModel Device::deviceListModel;
QStringListModel Device::formatListModel;
QStringListModel Device::resolutionListModel;
QStringListModel Device::fpsListModel;

Device::Device()
{
    dev_fd=-1;
    hid_fd = -1;
    flag=true;
    init_capture = true;
    initial_check_index = 0;
    image_buf=NULL;
    buffers = NULL;
    m_renderer = NULL;
    temp_buffer = NULL;
    vendor_id = NULL;
    product_id = NULL;
    devh = NULL;
    ctx = NULL;
    inBuf = NULL;
    outBuf = NULL;
}

Device::~Device()
{
    if(dev_fd>=0 && hid_fd>=0)
    {
        flag=false;
        future.waitForFinished();
        decompress_future.waitForFinished();
        if(streamoff()!=0)                              //for streaming off the device connected
            qDebug() << "Streamoff failed";
        if(unmap()!=0)                                  //for unmapping the memory which was mapped
            qDebug() << "Memory unmapping failed";
        close(hid_fd);                              //closing hid file descriptor
        hid_fd = -1;
        close(dev_fd);                              //closing device file descriptor
        dev_fd = -1;
    }
    if(buffers)
    {
        free(buffers);
        buffers=NULL;
    }
    if(image_buf)
    {
        free(image_buf);
        image_buf=NULL;
    }
    if(temp_buffer)
    {
        free(temp_buffer);
        temp_buffer = NULL;
    }
    if(inBuf)
    {
        free(inBuf);
        inBuf = NULL;
    }
    if(outBuf)
    {
        free(outBuf);
        outBuf = NULL;
    }
}

void Device::rendering_image()
{
    if (!m_renderer)
    {
        m_renderer = new Renderer();
        m_renderer->renderer_width = frame_width;
        m_renderer->renderer_height = frame_height;
        m_renderer->calculateViewport((window()->height()-100),(window()->width()-300));            //calculating viewport size
        connect( window(), &QQuickWindow::afterRendering, this, &Device::paint, Qt::DirectConnection);
    }
    if(MJPEG_flag)
    {
        m_renderer->set_shaders_RGB();
        //        m_renderer->glViewport(m_renderer->x,m_renderer->y,m_renderer->viewport_width/2,m_renderer->viewport_height/2);
    }
    else
    {
        m_renderer->set_shaders_UYVY();
        //        m_renderer->glViewport(m_renderer->x,m_renderer->y,(m_renderer->viewport_width/2),m_renderer->viewport_height/2);
    }
}

void Device::paint()
{
    m_renderer->paint();
    window()->resetOpenGLState();
    window()->update();
}


void Device::device_enumerate()                         //enumeration of devices which are connected
{
    deviceList.clear();                               //clearing the list initially
    deviceList << "Select Device";
    struct udev *udev;
    struct udev_device *device,*parentdevice;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    /* creating udev object */
    udev = udev_new();
    if (!udev)
    {
        perror("Cannot create udev context.\n");
        return ;
    }

    /* creating enumerate object */
    enumerate = udev_enumerate_new(udev);
    if (!enumerate)
    {
        perror("Cannot create enumerate context.\n");
        return ;
    }
    udev_enumerate_add_match_subsystem(enumerate, "video4linux");
    udev_enumerate_scan_devices(enumerate);

    /* fillup device list */
    devices = udev_enumerate_get_list_entry(enumerate);
    if (!devices)
    {
        perror("No device connected");
        deviceListModel.setStringList(deviceList);
        return ;
    }

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path=NULL,*sNo=NULL,*deviceName=NULL;

        path = udev_list_entry_get_name(dev_list_entry);

        device = udev_device_new_from_syspath(udev, path);

        parentdevice = udev_device_get_parent_with_subsystem_devtype(device,"usb","usb_device");
        if(!parentdevice)
            continue;

        product_id = udev_device_get_sysattr_value(parentdevice,"idProduct");

        sNo = udev_device_get_sysattr_value(parentdevice,"serial");
        if(sNo==NULL)
            continue;

        deviceName= udev_device_get_sysattr_value(parentdevice, "product");
        if(deviceName==NULL)
            continue;

        if( check_for_valid_videonode(udev_device_get_devnode(device))==0)            //Checking if the file is valid or not.This is because ubuntu enumerates one device as two files
        {
            serialNo.push_back(sNo);
            deviceList << deviceName;
        }
    }
    deviceListModel.setStringList(deviceList);

    udev_enumerate_unref(enumerate);// free enumerate
    udev_unref(udev);               // free udev
}

int Device::check_for_valid_videonode(const char *dev_node)
{
    int cam_fd;
    struct v4l2_capability cam_cap;                                 //For getting capability of that device
    if ((cam_fd = open(dev_node, O_RDWR|O_NONBLOCK, 0)) < 0)
    {
        qDebug("Can't open camera device ");
        return -1;
    }
    /* Check if the device is capable of streaming */
    if(ioctl(cam_fd, VIDIOC_QUERYCAP, &cam_cap) < 0)
    {
        qDebug(" VIDIOC_QUERYCAP failure");
        close(cam_fd);
        return -1;
    }
    close(cam_fd);

    if (cam_cap.device_caps & V4L2_CAP_META_CAPTURE)                //If this Flag is enable that means invalid node
        return -1;

    return 0;
}


int Device::openFile(const char* serial_num,const char* file_name)
{
    struct udev *udev;
    struct udev_device *device,*parentdevice;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    int fd=-1;

    /* creating udev object */
    udev = udev_new();
    if (!udev)
    {
        perror("Cannot create udev context.\n");
        return -1;
    }

    /* creating enumerate object */
    enumerate = udev_enumerate_new(udev);
    if (!enumerate)
    {
        perror("Cannot create enumerate context.\n");
        return -1;
    }
    udev_enumerate_add_match_subsystem(enumerate, file_name);
    udev_enumerate_scan_devices(enumerate);

    /* fillup device list */
    devices = udev_enumerate_get_list_entry(enumerate);
    if (!devices)
    {
        perror("Failed to get device list.\n");
        return -1;
    }

    udev_list_entry_foreach(dev_list_entry, devices)
    {
        const char *path=NULL,*sNo=NULL;

        path = udev_list_entry_get_name(dev_list_entry);

        device = udev_device_new_from_syspath(udev, path);

        parentdevice = udev_device_get_parent_with_subsystem_devtype(device,"usb","usb_device");
        if(!parentdevice)
            return -1;

        const char *dev_path = udev_device_get_devnode(device);
        sNo = udev_device_get_sysattr_value(parentdevice,"serial");
        if(sNo==NULL)
            continue;

        vendor_id =  udev_device_get_sysattr_value(parentdevice,"idVendor");

        product_id = udev_device_get_sysattr_value(parentdevice,"idProduct");

        if(strcmp(serial_num,sNo)==0)
        {
            if(!strcmp(file_name,"video4linux") && check_for_valid_videonode(dev_path)<0 )
                continue;
            fd=open(dev_path,O_RDWR|O_NONBLOCK);
            if(fd<0)
                qDebug()<<"Error open";
            else
            {
                qDebug()<<"Opened file" << " " << dev_path;
                break;
            }
        }

    }
    udev_enumerate_unref(enumerate);// free enumerate
    udev_unref(udev);    // free udev
    return fd;

}

int Device:: init_hid_devices(libusb_context **ctx)
{
    if(libusb_init(ctx)<0)
    {
        perror("Failed to initialise libusb\n");
        return -1;
    }
    if(find_econ_hiddevices()<0)
    {
        perror("Failed to find e-con devices");
        return -1;
    }
    if(libusb_kernel_driver_active(devh, HID_INTERFACE_NUMBER))
        libusb_detach_kernel_driver(devh, HID_INTERFACE_NUMBER);

    if(libusb_claim_interface(devh, HID_INTERFACE_NUMBER)<0)
    {
        perror("Failed to claim interface");
        return -1;
    }
    return 0;
}

int Device::find_econ_hiddevices()
{
//    qDebug() <<strtol(vendor_id, NULL, 16) << strtol(product_id, NULL, 16);

    devh = libusb_open_device_with_vid_pid(NULL, strtol(vendor_id, NULL, 16), strtol(product_id, NULL, 16));
    if(devh)
    {
        return 0;
    }
    else
    {
        perror("failed to open device with pid");
        return -1;
    }
}

void Device::readFirmwareVersion()
{
    int timeout = 0;
    int numBytesWrite = 0,numBytesRead=0;

    inBuf = (unsigned char*)calloc((BUFFER_LENGTH+1),sizeof(unsigned char));
    outBuf = (unsigned char*)calloc((BUFFER_LENGTH+1),sizeof(unsigned char));

    outBuf[0] = READFIRMWAREVERSION;

    while(timeout<3)
    {
        if(libusb_interrupt_transfer(devh,OUT,outBuf,BUFFER_LENGTH,&numBytesWrite,1000) == 0)
        {
            timeout=0;
            while(timeout<5)
            {
                if(libusb_interrupt_transfer(devh,IN,inBuf,BUFFER_LENGTH,&numBytesRead,2000) == 0)
                {
                    if(inBuf[0]==READFIRMWAREVERSION)
                    {
                        qDebug() << "Firmware Version:" << inBuf[1]<<"."<<inBuf[2]<<"."<< (inBuf[3]<<8)+inBuf[4] <<"."<<(inBuf[5]<<8)+inBuf[6];
                    }
                    break;
                }
                else
                {
                    perror("libusb interrupt read failed");
                    timeout++;
                }
            }
            break;
        }
        else
        {
            perror("libusb interrupt write failed");
            timeout++;
        }
    }
}

int Device::getFirmwareVersionH264()
{
    __u8 firmware[4];
    __u16 size=0;
    CLEAR(xquery);                                      //V4L2_CID_XU_FW_VERSION,UVC_GET_CUR,4,output

    xquery.query = UVC_GET_LEN;
    xquery.size = 2;
    xquery.selector = V4L2_CID_XU_FW_VERSION;
    xquery.unit = EXTENSION_UNIT_ID;
    xquery.data = (__u8 *)&size;

    if(ioctl(dev_fd,UVCIOC_CTRL_QUERY, &xquery)<0)
        qDebug() << "UVCIOC_CTRL_QUERY ioctl failed";

    CLEAR(xquery);
    xquery.query = UVC_GET_CUR;
    xquery.size = size;
    xquery.selector = V4L2_CID_XU_FW_VERSION;
    xquery.unit = EXTENSION_UNIT_ID;
    xquery.data = firmware;

    if(ioctl(dev_fd,UVCIOC_CTRL_QUERY, &xquery)<0)
        qDebug() << "UVCIOC_CTRL_QUERY ioctl failed";

    __u8 *val = xquery.data;

    for(uint i=0; i<4; i++){
        firmware[i] = val[i];
    }

    qDebug() << "Firmware version:"<<firmware[0]<<"."<<firmware[1]<<"."<<firmware[2]<<"."<<firmware[3];
    return 0;

}


void Device::selectDevice(int index)
{
    if(index)
    {
        QByteArray bytearray = serialNo[index-1].toLocal8Bit();         //converting Qstring to const char
        const char *serial = bytearray.data();
//        const char *serial = 0;
        dev_fd = openFile(serial,"video4linux");
        hid_fd = openFile(serial,"hidraw");
//        if(init_hid_devices(ctx)<0)
//        {
//            qDebug() << "Init hid devices failed";
//        }
//        readFirmwareVersion();
//        if(getFirmwareVersionH264()<0)
//        {
//            qDebug() << "getFirmwareVersion failed";
//        }
        query_control();
        preview();
    }
}

void Device::preview()
{
    if(queryCap()!=0)
        qDebug() << "Query cap failed";
    if(enumFormat()!=0)
        qDebug() << "Enum format failed";
    if(requestBuf()!=0)
        qDebug() << "Request Buffer failed";
    if(queryBuf()!=0)
        qDebug() << "Query Buffer failed";
    if(queueBuf()!=0)
        qDebug() << "Enqueue failed";
    rendering_image();
    if(streamon()==0)
    {
        qDebug() << "Streamon completed";
        init_capture = false;
    }
    future=QtConcurrent::run(this,&Device::dequeueBuf);

}

int Device::queryCap()
{
    if(ioctl(dev_fd,VIDIOC_QUERYCAP,&cap))                      //querrying capabilities
    {
        perror("Query_cap failed ");
        return -1;
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))            //checking if the device supports video capture
    {
        qDebug()<<"Does not support video capture";
        return -1;
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING))                //checking if the device supports streaming
    {
        qDebug()<<"Does not suppot streaming";
        return -1;
    }
    return 0;
}

int Device::enumFormat()
{
    frmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;                      //setting format type
    if(ioctl(dev_fd,VIDIOC_G_FMT,&frmt)<0)                      //getting format
    {
        qDebug()<<"Failed getting format";
        return -1;
    }
    strparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev_fd,VIDIOC_G_PARM,&strparm)<0)                  //getting parameters
    {
        qDebug()<<"Failed getting parameter";
        return -1;
    }

    frame_height = frmt.fmt.pix.height;
    frame_width = frmt.fmt.pix.width;
    stride = frmt.fmt.pix.bytesperline;

    if(frmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)         //checking for mjpeg format
        MJPEG_flag = true;
    else
        MJPEG_flag = false;



    CLEAR(fmtdesc);                                             //clearing garbage values
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    index=0;
    formatList.clear();
    format.clear();
    formatList << "Select Format";

    /*enumeration of pixel formats supported by device*/
    while (ioctl(dev_fd,VIDIOC_ENUM_FMT,&fmtdesc) == 0)
    {
        format.push_back(fmtdesc.pixelformat);
        index++;
        if(fmtdesc.pixelformat==frmt.fmt.pix.pixelformat)
        {
            comboBox_format_index=index;
        }
        formatList << (char*)fmtdesc.description;
        fmtdesc.index++;
    }
    formatListModel.setStringList(formatList);
    emit emitformat(comboBox_format_index);
    return 0;
}

void Device::query_control()
{
    /* Query for brightness support*/
    CLEAR(queryctrl);
    queryctrl.id = V4L2_CID_BRIGHTNESS;

    if (ioctl(dev_fd, VIDIOC_QUERYCTRL, &queryctrl) < 0)
    {
        perror("Query control failed");
        return;
    }
    else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
    {
        printf("V4L2_CID_BRIGHTNESS is not supported");
        return;
    }
    else
        emit emit_max_brightness( queryctrl.maximum);                //emit signal to slider to set maximum value
}

void Device::enumResolution(int index1)
{
    if(index1)
    {
        QString str;
        resolutionList.clear();
        frmheight.clear();
        frmwidth.clear();
        resolutionList << "Select Resolution";
        formatIndex=index1;
        index=0;
        CLEAR(frmsize);                                                 //clearing garbage values
        frmsize.pixel_format = format[index1-1];
        frmt.fmt.pix.pixelformat = format[index1-1];

        /*enumeration of resolution supported by the selected pixel format*/
        while (ioctl(dev_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
        {
            str.clear();
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                frmheight.push_back(frmsize.discrete.height);
                frmwidth.push_back(frmsize.discrete.width);
                index++;
                if(frmsize.discrete.height==frmt.fmt.pix.height && frmsize.discrete.width==frmt.fmt.pix.width)
                {
                    comboBox_res_index=index;
                }
                str.append(QString::number(frmwidth[index-1]));
                str.append("*");
                str.append(QString::number(frmheight[index-1]));
                resolutionList << str;
            }
            frmsize.index++;
        }
        resolutionListModel.setStringList(resolutionList);
        emit emitresolution(comboBox_res_index);
        if(!init_capture)
        {
            enumFps(comboBox_res_index);
            changeFormat();
        }
    }
}

void Device::enumFps(int index1)
{
    if(index1)
    {
        fpsList.clear();
        fps_denominator.clear();
        fps_numerator.clear();
        fpsList << "Select fps";
        index=0;
        CLEAR(frmival);                                         //clearing garbage values
        frmival.pixel_format = format[formatIndex-1];
        frmival.width = frmwidth[index1-1];
        frmt.fmt.pix.width = frmwidth[index1-1];
        frmival.height = frmheight[index1-1];
        frmt.fmt.pix.height = frmheight[index1-1];

        /*enumeration of framerate for the selected resolution*/
        while (ioctl(dev_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0)
        {
            if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
            {
                fps_numerator.push_back(frmival.discrete.numerator);
                fps_denominator.push_back(frmival.discrete.denominator);
                index++;

                if(frmival.discrete.numerator==strparm.parm.capture.timeperframe.numerator && frmival.discrete.denominator==strparm.parm.capture.timeperframe.denominator)
                {
                    comboBox_fps_index=index;
                }
            }
            fpsList << QString::number((float)(fps_denominator[index-1]/fps_numerator[index-1]));
            frmival.index++;
        }
        fpsListModel.setStringList(fpsList);
        emit emitfps(comboBox_fps_index);
        if(!init_capture)
        {
            emitfps(comboBox_fps_index);
            changeFormat();
        }
    }
}

void Device::selectFps(int index1)
{
    if(index1)
    {
        strparm.parm.capture.timeperframe.numerator = fps_numerator[index1-1];
        strparm.parm.capture.timeperframe.denominator = fps_denominator[index1-1];
        if(!init_capture)
            changeFormat();
    }
}

void Device::changeFormat()
{
    qDebug() << "changeformat called";
    flag=false;
    future.waitForFinished();
    decompress_future.waitForFinished();
    if(streamoff()!=0)
        qDebug() << "Streamoff failed inside changeFormat()";
    if(unmap()!=0)
        qDebug() << "Memory unmapping failed inside changeFormat()";

    frmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;                      //setting format type
    if(ioctl(dev_fd,VIDIOC_S_FMT,&frmt)<0)                      //setting format
    {
        qDebug()<<"Failed setting format inside changeFormat()";
        return ;
    }
    strparm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(dev_fd,VIDIOC_S_PARM,&strparm)<0)                  //setting parameters
    {
        qDebug()<<"Failed setting parameter inside changeFormat()";
        return ;
    }

    if(frmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG)          //check if pixel format is mjpeg
        MJPEG_flag = true;
    else
        MJPEG_flag = false;

    frame_height = frmt.fmt.pix.height;
    frame_width = frmt.fmt.pix.width;
    stride = frmt.fmt.pix.bytesperline;

    if(m_renderer)                                               //deleting renderer object when format changed
    {
        delete m_renderer;
        m_renderer = NULL;
    }

    flag = true;
    if(requestBuf()!=0)
        qDebug() << "Request Buffer failed inside changeFormat()";
    if(queryBuf()!=0)
        qDebug() << "Query Buffer failed inside changeFormat()";
    if(queueBuf()!=0)
        qDebug() << "Enqueue failed inside changeFormat()";
    rendering_image();
    if(streamon()!=0)
        qDebug() << "Streamon failed inside changeFormat()";

    future=QtConcurrent::run(this,&Device::dequeueBuf);
}

int Device::requestBuf()
{
    CLEAR(req);
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.count = 3;
    req.memory =V4L2_MEMORY_MMAP;

    if(ioctl(dev_fd, VIDIOC_REQBUFS, &req)<0)                           //checking if request buffer is success or not
    {
        qDebug()<<"Buffer request failed";
        return -1;
    }

    if (req.count < 2)                                                  //request count less than minimum value
    {
        qDebug()<<"Insufficient buffer count:";
        return -1;
    }
    buffers = (struct buffer*)calloc(req.count, sizeof (*buffers));     //allocating memory for requsted buffer count
    if(!buffers)
    {
        qDebug() << "Memory not allocated";
        return -1;
    }
    return 0;
}

int Device::queryBuf()
{
    CLEAR(buf);
    for(index=0;index<req.count;index++)
    {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = index;

        if(ioctl(dev_fd, VIDIOC_QUERYBUF,&buf)<0)                      //checking if query buffer is successfull or not
        {
            perror("Querybuf failed");
            return -1;
        }

        buffers[index].length = buf.length;

        buffers[index].start=mmap(NULL,buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,dev_fd,buf.m.offset);

        if(buffers[index].start==MAP_FAILED)                            //checking if the mapping failed
        {
            qDebug()<<"Mapping Failed";
            return -1;
        }
    }
    return 0;
}

int Device::queueBuf()
{
    for(index=0;index<req.count;index++)
    {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = index;
        if(ioctl(dev_fd, VIDIOC_QBUF,&buf)<0)                           //checking if queue buffer is successful
        {
            qDebug()<<"Failed enqueuing";
            return -1;
        }
    }
    return 0;
}

int Device::streamon()
{
    if(ioctl(dev_fd,VIDIOC_STREAMON,&buf.type)<0)                       //streaming on the device
    {
        perror("Streaming Failed");
        return -1;
    }

    if(frmt.fmt.pix.pixelformat==V4L2_PIX_FMT_MJPEG)                    //checking for mjpeg format
    {
        image_buf = (unsigned char*)realloc(image_buf,(frame_height*frame_width*4));            //allocating memory
        if(!image_buf)
        {
            qDebug() << "No memory allocated";
            return -1;
        }
    }
    return 0;
}

int Device::xioctl(int fd,int request, void *arg)
{
    int index1;
    do {
        index1 = ioctl(fd, request, arg);
    } while (index1==-1 && EAGAIN == errno);                        //do ioctl until there is no EAGAIN error(resource temporarily unavailable, try again)

    return index1;
}

void Device::dequeueBuf()
{
    while(flag)
    {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if(xioctl(dev_fd, VIDIOC_DQBUF,&buf)<0)                         //checking if dequeue is successful
        {
            perror("Dequeue Failed");
            if(errno == ENODEV)
            {
                flag = false;
                device_lost();
                return;
            }
        }
        //        qDebug() << "Bytes used:" << buf.bytesused;
        if(MJPEG_flag)
        {
            if(buf.bytesused == 0)                                      //check if it is invalid data
            {
                if(ioctl(dev_fd, VIDIOC_QBUF,&buf)<0)                       //checking if queue buffer is successfull in case of invalid data
                {
                    qDebug()<<"Failed enqueuing after dequeueing inside mjpeg";
                }
                continue;
            }
            else
            {
                temp_buffer = (unsigned char *)realloc((unsigned char*)temp_buffer,buf.bytesused);                                       //allocating memory for buffer
                m_renderer->mutex.lock();
                memcpy(temp_buffer,(unsigned char *)buffers[buf.index].start,buf.bytesused);                                             //copying data to the buffer
                m_renderer->mutex.unlock();
                m_renderer->bytesused= (frame_height*frame_width*4);
                decompress_future = QtConcurrent::run(m_renderer,&Renderer::mjpeg_decompress,temp_buffer,image_buf,buf.bytesused,stride);

                m_renderer->getImageBuffer(image_buf);
            }
        }
        else if(frmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY)
        {
            if(buf.bytesused < frame_height*frame_width*2)                 //check if it is invalid data
            {
                if(ioctl(dev_fd, VIDIOC_QBUF,&buf)<0)                       //checking if queue buffer is successful in case of invalid data
                {
                    qDebug()<<"Failed enqueuing after dequeueing inside uyvy";
                }
                continue;
            }
            else
            {
                m_renderer->bytesused= buf.bytesused;
                m_renderer->getImageBuffer((unsigned char*)buffers[buf.index].start);
            }
        }
        if(xioctl(dev_fd, VIDIOC_QBUF,&buf)<0)                            //checking if queue buffer is successful in case of valid data during dequeue
        {
            perror("Failed enque after dequeue");
        }
    }
}

void Device::device_lost()
{
    emit device_disconnected();
    qDebug("Device disconnected");
    future.waitForFinished();
    decompress_future.waitForFinished();
    m_renderer->device_lost();
    if(buffers)
    {
        free(buffers);
        buffers=NULL;
    }
    if(image_buf)
    {
        free(image_buf);
        image_buf=NULL;
    }
    if(temp_buffer)
    {
        free(temp_buffer);
        temp_buffer = NULL;
    }
    if(m_renderer)
    {
        delete m_renderer;
        m_renderer = NULL;
    }
    dev_fd=-1;
    hid_fd = -1;
    flag=true;
    init_capture = true;
    initial_check_index = 0;
    vendor_id = NULL;
    product_id = NULL;
    devh = NULL;
    ctx = NULL;
}

int Device::streamoff()
{
    if(xioctl(dev_fd, VIDIOC_STREAMOFF, &buf.type)<0)                    //streaming off the device
    {
        perror("Streamoff failed");
        return -1;
    }
    return 0;
}

int Device::unmap()
{
    for (index = 0; index < req.count;index++)
    {
        if(munmap(buffers[index].start, buffers[index].length)<0)               //unmapping the data acquired from mmap
        {
            perror("Memory unmapping failed");
            return -1;
        }
    }

    //Memory releasing
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.count = 0;
    req.memory =V4L2_MEMORY_MMAP;
    if(ioctl(dev_fd, VIDIOC_REQBUFS, &req)<0)                                   //releasing memory by requesting buffer with count 0
    {
        qDebug()<<"Memory release failed";
        return -1;
    }
    return 0;
}

void Device::set_brightness(int value)
{
    CLEAR(control);
    //    CLEAR(queryctrl);
    control.id = V4L2_CID_BRIGHTNESS;
    control.value = value;
    if (ioctl(dev_fd, VIDIOC_S_CTRL, &control)<0)                               //checking if set control is successful
        perror("Set brightness failed");

}
