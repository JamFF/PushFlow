//
// Created by FF on 2019-06-16.
//
#include <x264.h>
#include <cstring>
#include "VideoChannel.h"
#include "macro.h"

/**
 * 初始化视频编码信息
 * @param width 宽
 * @param height 高
 * @param fps 帧率
 * @param bitrate 码率，单位kbps 即千位每秒(kb/s)
 */
void VideoChannel::setVideoInfo(int width, int height, int fps, int bitrate) {
    mWidth = width;
    mHeight = height;
    mFps = fps;
    mBitrate = bitrate;
    ySize = static_cast<size_t>(width) * static_cast<size_t>(height);
    uvSize = ySize / 4;

    // 初始化x264的编码器
    x264_param_t param{};
    x264_param_default_preset(&param,
                              "ultrafast",// 因为直播，采用最快编码速度
                              "zerolatency");// 因为直播，采用零延迟
    param.i_level_idc = 32;// 编码复杂度一般是32
    // 输入数据格式：I420，虽然Android是：NV21，但是这里也要使用X264_CSP_I420
    param.i_csp = X264_CSP_I420;
    // 注意：不能使用X264_CSP_NV21，如果使用了，在encodeData中转码yuvI420时会崩溃
    // param.i_csp = X264_CSP_NV21;// 不能使用

    param.i_width = width;
    param.i_height = height;

    param.i_bframe = 0;// 直播中，为了达到首开（打开就可以观看），所以设置无B帧
    // 参数i_rc_method表示码率控制，CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
    param.rc.i_rc_method = X264_RC_ABR;
    // 码率，单位kbps 即千位每秒(kb/s)
    param.rc.i_bitrate = bitrate;
    // 瞬时最大码率
    param.rc.i_vbv_max_bitrate = static_cast<int>(bitrate * 1.2);
    // 设置了i_vbv_max_bitrate必须设置此参数，码率控制区大小，单位kbps 千比特每秒
    param.rc.i_vbv_buffer_size = bitrate;

    // 比如帧率是60fps，即每秒60帧，i_fps_num就是60，i_fps_den就是1
    param.i_fps_num = static_cast<uint32_t>(fps);// 帧率的分子，就是帧数
    param.i_fps_den = 1;// 帧率的分母，就是时间
    // 还是上面的例子，i_timebase_num是1，i_timebase_den是60
    // 时间基数就是每帧的时间间隔，可用于音视频同步
    param.i_timebase_num = param.i_fps_den;// 时间基数的分子
    param.i_timebase_den = param.i_fps_num;// 时间基数的分母

    // VFR输入 1：时间基和时间戳用于码率控制 0：仅帧率用于码率控制
    param.b_vfr_input = 0;// 用fps而不是时间戳来计算帧间距离
    // 帧距离(关键帧)，两倍的帧率，还是上面的例子，相当于120帧（也可以理解为2s）一个关键帧
    param.i_keyint_max = fps * 2;
    // 是否复制sps和pps放在每个关键帧的前面
    param.b_repeat_headers = 1;// 设置1，每个关键帧(I帧)都附带sps/pps
    // 并行编码多帧，线程数，为0则自动多线程编码
    param.i_threads = 1;// 设置1，单线程编码
    x264_param_apply_profile(&param, "baseline");// 设置图像质量的方法

    // 创建一个新的编码器，复制x264_param_t中的所有参数
    pVideoEncoder = x264_encoder_open(&param);

    pic_in = new x264_picture_t;// 一帧NV21输入数据，后面还要编码为NALU
    // 声明空间
    x264_picture_alloc(pic_in, param.i_csp, width, height);
}

/**
 * 对nv21进行编码
 * @param data
 */
void VideoChannel::encodeData(int8_t *data) {
    // data是nv21编码，将其编码为yuvI420
    memcpy(pic_in->img.plane[0], data, ySize);// 将y数据存储在img.plane[0]
    for (int i = 0; i < uvSize; ++i) {
        // 将nv21编码中存放在u、v地址上的值，赋值给yuvI420编码中的u、v
        // u数据 1, 3, 5, 7...
        *(pic_in->img.plane[1] + i) = static_cast<uint8_t>(*(data + ySize + i * 2 + 1));
        // v数据 0, 2, 4, 6...
        *(pic_in->img.plane[2] + i) = static_cast<uint8_t>(*(data + ySize + i * 2));
    }

    x264_nal_t *pp_nal;// NALU单元
    int pi_nal;// 编码出来有几个数据（多少NALU单元）
    x264_picture_t pic_out{};// 用不上，下面也可以传nullptr
    // 将yuvI420编码为h264
    x264_encoder_encode(pVideoEncoder, &pp_nal, &pi_nal, pic_in, &pic_out);
    int sps_len = 0;// SPS数据的长度，去除数据头 00 00 00 01 前4个字节
    int pps_len = 0;// PPS数据的长度，去除数据头 00 00 00 01 前4个字节
    uint8_t sps[100];
    uint8_t pps[100];
    for (int i = 0; i < pi_nal; ++i) {
        if (pp_nal[i].i_type == NAL_SPS) {// 执行顺序：第一步SPS数据
            sps_len = pp_nal[i].i_payload - 4;
            if (sps_len) {
                // 将NALU单元中有效的SPS数据拷贝到sps变量中
                memcpy(sps, pp_nal[i].p_payload + 4, static_cast<size_t>(sps_len));
            }
        } else if (pp_nal[i].i_type == NAL_PPS) {// 执行顺序：第二步PPS数据
            pps_len = pp_nal[i].i_payload - 4;
            if (pps_len) {
                // 将NALU单元中有效的PPS数据拷贝到pps变量中
                memcpy(pps, pp_nal[i].p_payload + 4, static_cast<size_t>(pps_len));
                if (sps_len) {
                    // 先将SPS和PPS数据发送一起发送

                    sendSpsPps(sps, pps, sps_len, pps_len);
                }
            }
        } else {// 执行顺序：第三步关键帧和非关键帧，在SPS和PPS数据之后
            sendFrame(pp_nal[i].i_type, pp_nal[i].p_payload, pp_nal[i].i_payload);
        }
    }
}

/**
 * 发送NALU中SPS和PPS数据
 * @param sps
 * @param pps
 * @param sps_len
 * @param pps_len
 */
void VideoChannel::sendSpsPps(uint8_t *sps, uint8_t *pps, int sps_len, int pps_len) {

    // 将sps, pps数据以RTMP协议封装到数据包
    auto *pPacket = new RTMPPacket;
    // 根据RTMP编码文档计算出数据长度
    int bodySize = 13 + sps_len + 3 + pps_len;
    RTMPPacket_Alloc(pPacket, bodySize);// 申请空间

    int i = 0;
    // 固定头
    pPacket->m_body[i++] = 0x17;
    // 类型
    pPacket->m_body[i++] = 0x00;
    pPacket->m_body[i++] = 0x00;
    pPacket->m_body[i++] = 0x00;
    pPacket->m_body[i++] = 0x00;

    // 版本
    pPacket->m_body[i++] = 0x01;
    // 编码规格
    pPacket->m_body[i++] = sps[1];
    pPacket->m_body[i++] = sps[2];
    pPacket->m_body[i++] = sps[3];
    pPacket->m_body[i++] = 0xFF;// 通常为0xFF

    // sps
    pPacket->m_body[i++] = 0xE1;// sps个数，通常为0xE1
    // sps长度
    pPacket->m_body[i++] = (sps_len >> 8) & 0xFF;// 高八位
    pPacket->m_body[i++] = sps_len & 0xff;// 低八位

    // sps内容，整体拷贝
    memcpy(&pPacket->m_body[i], sps, static_cast<size_t>(sps_len));
    i += sps_len;

    // pps
    pPacket->m_body[i++] = 0x01;// pps个数，通常为0x01
    // pps长度
    pPacket->m_body[i++] = (pps_len >> 8) & 0xFF;// 高八位
    pPacket->m_body[i++] = (pps_len) & 0xFF;// 低八位
    memcpy(&pPacket->m_body[i], pps, static_cast<size_t>(pps_len));

    // 设置视频类型
    pPacket->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    pPacket->m_nBodySize = static_cast<uint32_t>(bodySize);
    // 分配一个管道（值随意，尽量避开rtmp.c中使用的，也不要与自己定义的其它频道重复）
    pPacket->m_nChannel = 0x10;
    // sps、pps没有时间戳，设置为0
    pPacket->m_nTimeStamp = 0;
    // 不使用绝对时间
    pPacket->m_hasAbsTimestamp = 0;
    pPacket->m_headerType = RTMP_PACKET_SIZE_MEDIUM;

    if (videoCallback != nullptr) {
        // LOG_D("发送 SPS PPS");
        // 回调数据到native-lib的队列中
        videoCallback(pPacket);
    }
}

/**
 * 发送NALU关键帧和非关键帧
 * @param type
 * @param payload
 * @param i_payload
 */
void VideoChannel::sendFrame(int type, uint8_t *payload, int i_payload) {
    // TODO 这段代码的含义
    if (payload[2] == 0x00) {
        i_payload -= 4;
        payload += 4;
    } else {
        i_payload -= 3;
        payload += 3;
    }
    // 按照RTMP协议，封装到数据包
    auto *pPacket = new RTMPPacket;
    // 根据RTMP编码文档，h264裸数据前面有9个字节
    int bodySize = 9 + i_payload;
    RTMPPacket_Alloc(pPacket, bodySize);// 申请空间

    pPacket->m_body[0] = 0x27;// 非关键帧是0x27
    if (type == NAL_SLICE_IDR) {
        // LOG_D("发送 关键帧");
        pPacket->m_body[0] = 0x17;// 关键帧是0x17
    }
    // 类型
    pPacket->m_body[1] = 0x01;
    // 时间戳
    pPacket->m_body[2] = 0x00;
    pPacket->m_body[3] = 0x00;
    pPacket->m_body[4] = 0x00;
    // 数据长度 int 4个字节
    pPacket->m_body[5] = (i_payload >> 24) & 0xFF;
    pPacket->m_body[6] = (i_payload >> 16) & 0xFF;
    pPacket->m_body[7] = (i_payload >> 8) & 0xFF;
    pPacket->m_body[8] = (i_payload) & 0xFF;

    // 图片数据
    memcpy(&pPacket->m_body[9], payload, static_cast<size_t>(i_payload));

    pPacket->m_hasAbsTimestamp = 0;
    pPacket->m_nBodySize = static_cast<uint32_t>(bodySize);
    pPacket->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    // 分配一个管道（值随意，尽量避开rtmp.c中使用的，也不要与自己定义的其它频道重复）
    pPacket->m_nChannel = 0x11;
    pPacket->m_headerType = RTMP_PACKET_SIZE_LARGE;

    if (videoCallback != nullptr) {
        // 回调数据到native-lib的队列中
        videoCallback(pPacket);
    }
}

void VideoChannel::setVideoCallback(VideoCallback videoCallback) {
    this->videoCallback = videoCallback;
}

VideoChannel::~VideoChannel() {
    if (pVideoEncoder != nullptr) {
        x264_encoder_close(pVideoEncoder);
        pVideoEncoder = nullptr;
    }
    if (pic_in != nullptr) {
        x264_picture_clean(pic_in);
        DELETE(pic_in);
    }
}