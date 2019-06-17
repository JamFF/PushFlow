//
// Created by FF on 2019-06-16.
//

#ifndef PUSHFLOW_VIDEOCHANNEL_H
#define PUSHFLOW_VIDEOCHANNEL_H

#include "librtmp/rtmp.h"

class VideoChannel {

    typedef void (*VideoCallback)(RTMPPacket *packet);

public:
    /**
     * 初始化编码信息
     * @param width
     * @param height
     * @param fps
     * @param bitrate
     */
    void setVideoInfo(int width, int height, int fps, int bitrate);

    void encodeData(int8_t *data);

    void setVideoCallback(VideoCallback videoCallback);

private:
    int mWidth;
    int mHeight;
    int mFps;
    int mBitrate;
    size_t ySize;// yuv中y的个数
    size_t uvSize;// yuv中u、v的个数，因为u、v个数一致，使用一个变量表示
    x264_t *pVideoEncoder;// 视频编码器
    x264_picture_t *pic_in;// 一帧NV21输入数据

    VideoCallback videoCallback;

    void sendSpsPps(uint8_t sps[100], uint8_t pps[100], int len, int pps_len);

    void sendFrame(int type, uint8_t *payload, int i_payload);
};


#endif //PUSHFLOW_VIDEOCHANNEL_H
