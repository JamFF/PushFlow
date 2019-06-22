//
// Created by FF on 2019-06-20.
//

#include <cstring>
#include "AudioChannel.h"
#include "macro.h"

/**
 * 音频首帧数据，包含编码器信息
 * @return
 */
RTMPPacket *AudioChannel::getAudioTag() {
    u_char *buf;
    u_long len;
    // 编码器信息，需要音频第一帧发送，通知后面音频如何解码
    faacEncGetDecoderSpecificInfo(audioCodec, &buf, &len);
    int bodySizw = 2 + len;
    int bodySize = 2 + len;
    auto *pPacket = new RTMPPacket;
    RTMPPacket_Alloc(pPacket, bodySize);
    pPacket->m_body[0] = 0xAF;// 双声道
    if (mChannels == 1) {
        pPacket->m_body[0] = 0xAE;// 单声道
    }
    pPacket->m_body[1] = 0x00;
    //图片数据
    memcpy(&pPacket->m_body[2], buf, len);

    pPacket->m_hasAbsTimestamp = 0;
    pPacket->m_nBodySize = static_cast<uint32_t>(bodySize);
    pPacket->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    // 分配一个管道（值随意，尽量避开rtmp.c中使用的，也不要与自己定义的其它频道重复）
    pPacket->m_nChannel = 0x12;
    pPacket->m_headerType = RTMP_PACKET_SIZE_LARGE;
    return pPacket;
}

/**
 * 初始化视频编码信息
 * @param samplesInHZ 采样率
 * @param channels 声道数
 */
void AudioChannel::setAudioInfo(int samplesInHZ, int channels) {

    mChannels = channels;

    // 打开faac编码器，根据采样率和声道数，计算出缓冲区大小和最大缓冲区大小
    audioCodec = faacEncOpen(static_cast<unsigned long>(samplesInHZ),
                             static_cast<unsigned int>(channels),
                             &inputSamples,// faac编码器返回的缓冲区大小
                             &maxOutputBytes);// faac编码器返回的最大缓冲区大小

    // 获取编码器配置信息
    faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(audioCodec);
    // 设置aac编码版本
    config->mpegVersion = MPEG4;
    // 设置编码标准，越低编码速度越快
    config->aacObjectType = LOW;
    // 设置pcm输入格式：16位
    config->inputFormat = FAAC_INPUT_16BIT;
    // 设置编码后输出格式：Raw原始格式（既不是ADIF也不是ADTS）
    config->outputFormat = 0;
    // 设置编码器配置
    faacEncSetConfiguration(audioCodec, config);
    buffer = new u_char[maxOutputBytes];
}

/**
 * 音频编码，不断被调用
 * @param data
 */
void AudioChannel::encodeData(int8_t *data) {

    // 将音频数据存储在buffer中
    int bytesLen = faacEncEncode(audioCodec,// 编码器参数
                                 reinterpret_cast<int32_t *>(data),// 数据
                                 static_cast<unsigned int>(inputSamples),// 缓冲区大小
                                 buffer,
                                 static_cast<unsigned int>(maxOutputBytes));// 最大缓冲区

    if (bytesLen > 0) {// 存在音频数据
        // 按照RTMP协议，封装到数据包
        auto *pPacket = new RTMPPacket;
        int bodySize = 2 + bytesLen;
        RTMPPacket_Alloc(pPacket, bodySize);
        pPacket->m_body[0] = 0xAF;// 双声道
        if (mChannels == 1) {
            pPacket->m_body[0] = 0xAE;// 单声道
        }
        // 编码出的声音 都是 0x01
        pPacket->m_body[1] = 0x01;

        // 编码之后aac数据内容是不固定的，要根据编码大小得到的
        memcpy(&pPacket->m_body[2], buffer, static_cast<size_t>(bytesLen));

        pPacket->m_hasAbsTimestamp = 0;// 0代表相对时间
        pPacket->m_nBodySize = static_cast<uint32_t>(bodySize);// 数据包大小
        pPacket->m_packetType = RTMP_PACKET_TYPE_AUDIO;// 数据包类型
        // 分配一个管道（值随意，尽量避开rtmp.c中使用的，也不要与自己定义的其它频道重复）
        pPacket->m_nChannel = 0x13;
        pPacket->m_headerType = RTMP_PACKET_SIZE_LARGE;

        if (audioCallback != nullptr) {
            // 回调数据到native-lib的队列中
            audioCallback(pPacket);
        }
    }
}

/**
 * faac编码器返回的缓冲区大小，需要传回java层，与AudioRecord的缓冲区比较
 * @return
 */
int AudioChannel::getInputSamples() {
    return static_cast<int>(inputSamples);
}

void AudioChannel::setAudioCallback(AudioChannel::AudioCallback audioCallback) {
    this->audioCallback = audioCallback;
}

AudioChannel::~AudioChannel() {
    DELETE(buffer);
    //释放编码器
    if (audioCodec) {
        faacEncClose(audioCodec);
        audioCodec = nullptr;
    }
}

