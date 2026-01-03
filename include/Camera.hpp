#pragma once

#include <string>
#include <linux/videodev2.h>
#include <cstdint>

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

/* Bitmap header */
typedef struct tagBITMAPFILEHEADER {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

/* Bitmap info header */
typedef struct tagBITMAPINFOHEADER {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
}__attribute__((packed)) BITMAPINFOHEADER;

struct buffer
{
    void* start[3];
    size_t length[3];
};

class Camera
{
  public:
    Camera(const int width, const int height);
    ~Camera();
    bool ready();
    bool save(const std::string& dir, int count);

  private:
    bool m_initDevice(int width_, int height_);
    bool m_startStreaming();
    bool m_stopStreaming();
    bool m_readFrame(const std::string& dir, int frame_num);
    bool m_ready = false;

    int m_cameraFd = -1;
    const char* m_devPath = "/dev/video0";
    int m_width, m_height;
    struct buffer* m_buffers = NULL;
    uint8_t m_numBuffers;
    uint8_t m_numPlanes;

    int NV21ToRGB24(void *RGB24,void *NV21,int width,int height);
    int YUVToBMP(const char *bmp_path, void *yuv_data, int width,int height);
};