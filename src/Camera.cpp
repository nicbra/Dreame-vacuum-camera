#include "Camera.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fstream>
#include <iostream>
#include <cstring>

Camera::Camera(const int width, const int height)
{
    if (m_initDevice(width, height) && m_startStreaming()) {
        m_ready = true;
    }
}

Camera::~Camera()
{
    m_stopStreaming();
    if (m_cameraFd >= 0) {
        close(m_cameraFd);
    }

    free(m_buffers);
}

bool Camera::m_initDevice(int width, int height)
{
    m_width = width;
    m_height = height;
    m_cameraFd = open(m_devPath, O_RDWR);
    if (m_cameraFd < 0) {
        perror("open");
        return false;
    }
    printf("m_cameraFd: %d\n", m_cameraFd);

    struct v4l2_capability cap;
    if (ioctl(m_cameraFd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return false;
    }

    v4l2_input inp;
    inp.type = V4L2_INPUT_TYPE_CAMERA;
    if (ioctl(m_cameraFd, VIDIOC_S_INPUT, &inp) < 0) {
        perror("VIDIOC_S_INPUT");
        return false;
    }

    v4l2_streamparm parms;
    memset(&parms, 0, sizeof(struct v4l2_streamparm));
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parms.parm.capture.timeperframe.numerator = 1;
    parms.parm.capture.timeperframe.denominator = 30;
    if (ioctl(m_cameraFd, VIDIOC_S_PARM, &parms) < 0) {
        perror(" VIDIOC_S_PARM");
        return false;
    }

    memset(&parms, 0, sizeof(struct v4l2_streamparm));
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    if (ioctl(m_cameraFd, VIDIOC_G_PARM, &parms) < 0) {
        perror("VIDIOC_G_PARM");
        return false;
    }
    else {
        printf(" Camera capture framerate is %u/%u\n",
               parms.parm.capture.timeperframe.denominator,
               parms.parm.capture.timeperframe.numerator);
    }

    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = m_width;
    fmt.fmt.pix_mp.height = m_height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV21;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;

    if (ioctl(m_cameraFd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return false;
    }
    if (ioctl(m_cameraFd, VIDIOC_G_FMT, &fmt) < 0) {
        perror("VIDIOC_G_FMT");
        return false;
    }

    printf("Formats:\n-type: %d\n-width: %d\n-height: %d\n-pixelformat: %d\n-field: %d\n",
           fmt.type,
           fmt.fmt.pix_mp.width,
           fmt.fmt.pix_mp.height,
           fmt.fmt.pix_mp.pixelformat,
           fmt.fmt.pix_mp.field);
    m_numPlanes = fmt.fmt.pix_mp.num_planes;

    if ((fmt.fmt.pix_mp.width != m_width) || (fmt.fmt.pix_mp.height != m_height)) {
        printf("%dx%d format is not supported. Setting to %dx%d\n",
               m_width,
               m_height,
               fmt.fmt.pix_mp.width,
               fmt.fmt.pix_mp.height);

        m_width = fmt.fmt.pix_mp.width;
        m_height = fmt.fmt.pix_mp.height;
    }

    return true;
}

bool Camera::ready()
{
    return m_ready;
}

bool Camera::m_startStreaming()
{

    // TODO: cameraet støtter V4L2_CAP_STREAMING
    // Request driver to allocate buffer
    static v4l2_requestbuffers req = { 0 };
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(m_cameraFd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return false;
    }

    printf("Requested buffers\n");

    m_buffers = (struct buffer*)calloc(req.count, sizeof(struct buffer));
    if (!m_buffers) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    m_numBuffers = req.count;

    v4l2_buffer buffer = { 0 };

    /* Query buffers */
    for (uint8_t buffer_num = 0; buffer_num < m_numBuffers; buffer_num++) {
        memset(&buffer, 0, sizeof(buffer));

        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = m_numPlanes;
        buffer.m.planes = (struct v4l2_plane*)calloc(buffer.length, sizeof(struct v4l2_plane));
        buffer.index = buffer_num;

        if (-1 == ioctl(m_cameraFd, VIDIOC_QUERYBUF, &buffer)) {
            perror("VIDIOC_QUERYBUF");
            exit(errno);
        }

        /* Memory map*/
        for (int i = 0; i < m_numPlanes; i++) {
            m_buffers[buffer_num].length[i] = buffer.m.planes[i].length;
            m_buffers[buffer_num].start[i] = mmap(
                NULL,
                buffer.m.planes[i].length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                m_cameraFd,
                buffer.m.planes[i].m.mem_offset);

            if (MAP_FAILED == m_buffers[buffer_num].start[i]) {
                perror("mmap");
                exit(errno);
            }
        }
        free(buffer.m.planes);
    }

    printf("Queried buffers\n");

    /* Queue the buffers */
    for (int i = 0; i < req.count; ++i) {

        memset(&buffer, 0, sizeof(struct v4l2_buffer));

        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = m_numPlanes;
        buffer.m.planes = (struct v4l2_plane*)calloc(buffer.length, sizeof(struct v4l2_plane));
        buffer.index = i;

        if (ioctl(m_cameraFd, VIDIOC_QBUF, &buffer) < 0) {
            perror("VIDIOC_QBUF");
            return false;
        }
        free(buffer.m.planes);
    }

    printf("Queued buffers\n");

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(m_cameraFd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return false;
    }

    printf("Stream is on\n");
    return true;
}

bool Camera::m_readFrame(const std::string& dir, int frame_num)
{
    static v4l2_buffer buf = { 0 };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = m_numPlanes;
    buf.m.planes = (struct v4l2_plane*)calloc(m_numPlanes, sizeof(struct v4l2_plane));

    if (ioctl(m_cameraFd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            /* No buffer ready—if nonblocking. Otherwise, it won’t return. */
            printf("Frame not ready\n");
            return false;
        }
        perror("VIDIOC_DQBUF");
        return false;
    }

    printf("Dequed buffer (index %d, planes=%d)\n", buf.index, buf.length);

    std::string filename = dir + "/frame_" + std::to_string(frame_num) + ".bmp";

    int width, height;
    if (m_width * m_height * 1.5 < buf.bytesused) {
        width = ALIGN_16B(m_width);
        height = ALIGN_16B(m_height);
    }
    else {
        width = m_width;
        height = m_height;
    }

    YUVToBMP(filename.c_str(), m_buffers[buf.index].start[0], width, height);

    if (ioctl(m_cameraFd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
        return false;
    }
    return true;
}

bool Camera::save(const std::string& dir, int count)
{
    printf("Capturing %d frames to directory: %s\n", count, dir.c_str());
    for (int i = 0; i < count; ++i) {
        if (!m_readFrame(dir, i)) return false;
    }
    return true;
}

bool Camera::m_stopStreaming()
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(m_cameraFd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("VIDIOC_STREAMOFF");
        return false;
    }
    return true;
}

int Camera::NV21ToRGB24(void* RGB24, void* NV21, int width, int height)
{
    unsigned char* src_y = (unsigned char*)NV21;
    unsigned char* src_v = (unsigned char*)NV21 + (width * height);
    unsigned char* src_u = (unsigned char*)NV21 + (width * height) + 1;

    unsigned char* dst_RGB = (unsigned char*)RGB24;

    int temp[3];

    if (RGB24 == NULL || NV21 == NULL || width <= 0 || height <= 0) {
        printf(" NV21ToRGB24 incorrect input parameter!\n");
        return -1;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int Y = y * width + x;
            int U = ((y >> 1) * (width >> 1) + (x >> 1)) << 1;
            int V = U;

            temp[0] = src_y[Y] + ((7289 * src_u[U]) >> 12) - 228;                             //b
            temp[1] = src_y[Y] - ((1415 * src_u[U]) >> 12) - ((2936 * src_v[V]) >> 12) + 136; //g
            temp[2] = src_y[Y] + ((5765 * src_v[V]) >> 12) - 180;                             //r

            dst_RGB[3 * Y] = (temp[0] < 0 ? 0 : temp[0] > 255 ? 255
                                                              : temp[0]);
            dst_RGB[3 * Y + 1] = (temp[1] < 0 ? 0 : temp[1] > 255 ? 255
                                                                  : temp[1]);
            dst_RGB[3 * Y + 2] = (temp[2] < 0 ? 0 : temp[2] > 255 ? 255
                                                                  : temp[2]);
        }
    }

    return 0;
}

int Camera::YUVToBMP(const char* bmp_path, void* yuv_data, int width, int height)
{

    unsigned char* rgb_24 = NULL;
    FILE* fp = NULL;

    BITMAPFILEHEADER BmpFileHeader;
    BITMAPINFOHEADER BmpInfoHeader;

    if (bmp_path == NULL || yuv_data == NULL || width <= 0 || height <= 0) {
        printf(" YUVToBMP incorrect input parameter!\n");
        return -1;
    }

    /* Fill header information */
    BmpFileHeader.bfType = 0x4d42;
    BmpFileHeader.bfSize = width * height * 3 + sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);
    BmpFileHeader.bfReserved1 = 0;
    BmpFileHeader.bfReserved2 = 0;
    BmpFileHeader.bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader);

    BmpInfoHeader.biSize = sizeof(BmpInfoHeader);
    BmpInfoHeader.biWidth = width;
    BmpInfoHeader.biHeight = -height;
    BmpInfoHeader.biPlanes = 0x01;
    BmpInfoHeader.biBitCount = 24;
    BmpInfoHeader.biCompression = 0;
    BmpInfoHeader.biSizeImage = 0;
    BmpInfoHeader.biClrUsed = 0;
    BmpInfoHeader.biClrImportant = 0;

    rgb_24 = (unsigned char*)malloc(width * height * 3);
    if (rgb_24 == NULL) {
        printf(" YUVToBMP alloc failed!\n");
        return -1;
    }

    NV21ToRGB24(rgb_24, yuv_data, width, height);

    /* Create bmp file */
    fp = fopen(bmp_path, "wb+");
    if (!fp) {
        free(rgb_24);
        printf(" Create bmp file:%s faled!\n", bmp_path);
        return -1;
    }

    fwrite(&BmpFileHeader, sizeof(BmpFileHeader), 1, fp);
    fwrite(&BmpInfoHeader, sizeof(BmpInfoHeader), 1, fp);
    fwrite(rgb_24, width * height * 3, 1, fp);

    free(rgb_24);
    fclose(fp);

    return 0;
}
