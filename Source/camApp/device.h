#ifndef DEVICE_H
#define DEVICE_H

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define HID_INTERFACE_NUMBER    0x02
#define BUFFER_LENGTH           64
#define READFIRMWAREVERSION     0x40
#define OUT                     0x06
#define IN                      0x85

#define V4L2_CID_XU_FW_VERSION  0x07
#define UVC_GET_CUR             0x81
#define UVC_GET_LEN             0x85
#define EXTENSION_UNIT_ID       3

#include <vector>
#include <QStringListModel>
#include <QPainter>
#include <QDebug>
#include <QtConcurrent>
#include <libudev.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <renderer.h>

#include <errno.h>
#include <pthread.h>
#include <libusb-1.0/libusb.h>

#include <linux/uvcvideo.h>

class Device : public QQuickItem
{
    Q_OBJECT
public:
    static QStringListModel deviceListModel,formatListModel,fpsListModel,resolutionListModel;
    Device();
    ~Device();

private:
    Renderer *m_renderer;
    int frame_height,frame_width,stride;
    unsigned char *image_buf,*temp_buffer;
    int formatIndex;
    bool flag,init_capture,MJPEG_flag;
    float framerate;
    void *bufstart;
    int index,comboBox_format_index=0,comboBox_res_index=0,comboBox_fps_index=0,initial_check_index;


    struct v4l2_capability cap;
    struct v4l2_format frmt;
    struct v4l2_streamparm strparm;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;
    struct v4l2_frmivalenum frmival;

    struct buffer{
        void   *start;
        size_t  length;
    };

    struct buffer *buffers;

    int check_for_valid_videonode(const char *dev_node);
    int openFile(const char* serialNo,const char* file_name);
    void preview();
    int queryCap();
    int enumFormat();
    int requestBuf();
    int queryBuf();
    int queueBuf();
    int streamon();
    int xioctl(int fd,int request, void *arg);
    void dequeueBuf();
    int streamoff();
    int unmap();
    void rendering_image();
    void query_control();
        void device_lost();

    QFuture<void> future,decompress_future;
    QImage img;
    int hid_fd,dev_fd;
    QVector<QString> serialNo;
    QVector<__u32> format;
    QVector<__u32> frmheight;
    QVector<__u32> frmwidth;
    QVector<__u32> fps_numerator;
    QVector<__u32> fps_denominator;
    QStringList deviceList,formatList,fpsList,resolutionList;

    struct v4l2_control control;
    struct v4l2_queryctrl queryctrl;

    const char *vendor_id,*product_id;
    libusb_device_handle *devh;
    libusb_context **ctx;
    int init_hid_devices(libusb_context **ctx);
    int find_econ_hiddevices();
    unsigned char *inBuf,*outBuf;
    void readFirmwareVersion();

    int getFirmwareVersionH264();
    struct uvc_xu_control_query xquery;


signals:
    //    void emitsignal(QImage img, int img_height,int img_width);
    void emitformat(int index);
    void emitresolution(int index);
    void emitfps(int index);
    void emitimage(unsigned char* img_buffer);
    void emit_max_brightness(int value);
    void device_disconnected();

public slots:
    void selectDevice(int index);
    void enumResolution(int index);
    void enumFps(int index);
    void selectFps(int index);
    void device_enumerate();
    void changeFormat();

    void set_brightness(int value);
    void paint();


};

#endif

