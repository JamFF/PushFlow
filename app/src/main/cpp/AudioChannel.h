//
// Created by FF on 2019-06-20.
//

#ifndef PUSHFLOW_AUDIOCHANNEL_H
#define PUSHFLOW_AUDIOCHANNEL_H

#include "faac.h"
#include "librtmp/rtmp.h"

class AudioChannel {

    typedef void (*AudioCallback)(RTMPPacket *packet);

public:
    void setAudioInfo(int samplesInHZ, int channels);

    void encodeData(int8_t *data);

    int getInputSamples();

    RTMPPacket *getAudioTag();

    void setAudioCallback(AudioCallback audioCallback);

    ~AudioChannel();

private:
    int mChannels;
    faacEncHandle audioCodec;// 编码器参数
    u_long inputSamples;// 缓冲区大小
    u_long maxOutputBytes;// 最大缓冲区
    u_char *buffer = 0;

    AudioCallback audioCallback;
};


#endif //PUSHFLOW_AUDIOCHANNEL_H
