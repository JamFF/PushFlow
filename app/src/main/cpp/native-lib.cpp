#include <jni.h>
#include <string>
#include "x264.h"
#include "librtmp/rtmp.h"
#include "VideoChannel.h"
#include "AudioChannel.h"
#include "macro.h"
#include "safe_queue.h"

int isStart = 0;// 为了防止用户重复点击开始直播，导致重新初始化
VideoChannel *pVideoChannel;
AudioChannel *pAudioChannel;
pthread_t pid;
uint32_t start_time;// 记录开始时间
int readyPushing = 0;// 是否准备好推流
// FIXME 提示 initialization of 'packets' with static storage duration may throw an exception that cannot be caught
SafeQueue<RTMPPacket *> packets;// RTMP协议数据包队列，包含音频、视频

/**
 * 封装为RTMP数据后的回调，将数据添加到队列
 * @param packet 按照RTMP协议封装的数据包
 */
void callback(RTMPPacket *packet) {
    if (packet != nullptr) {
        // 设置时间戳
        packet->m_nTimeStamp = RTMP_GetTime() - start_time;
        // 加入队列
        packets.put(packet);
        LOG_D("等待上传 packet 大小: %d", packets.size());
    }
}

void releasePackets(RTMPPacket *packet) {
    if (packet != nullptr) {
        RTMPPacket_Free(packet);
        delete packet;
        packet = nullptr;
    }
}

/**
 * 释放音频、视频资源
 */
void releaseMediaChannel() {
    if (pVideoChannel != nullptr) {
        DELETE(pVideoChannel);
    }
    if (pAudioChannel != nullptr) {
        DELETE(pAudioChannel);
    }
}

/**
 * 被线程调用的指针函数，不断从队列中读取RTMP数据，向服务器推流
 * @param args url
 * @return 
 */
void *start(void *args) {
    char *url = static_cast<char *>(args);
    RTMP *pRTMP = RTMP_Alloc();// 申请内存
    if (!pRTMP) {
        LOG_E("alloc rtmp 失败");
        return nullptr;
    }
    RTMP_Init(pRTMP);// 初始化
    int ret = RTMP_SetupURL(pRTMP, url);// 设置地址
    if (!ret) {
        LOG_E("设置地址失败: %s", url);
        return nullptr;
    }

    pRTMP->Link.timeout = 5;// 超时时间
    RTMP_EnableWrite(pRTMP);// 开启输出模式
    ret = RTMP_Connect(pRTMP, nullptr);// 连接服务器
    if (!ret) {
        LOG_E("连接服务器: %s", url);
        return nullptr;
    }

    ret = RTMP_ConnectStream(pRTMP, 0);// 连接流
    if (!ret) {
        LOG_E("连接流: %s", url);
        return nullptr;
    }
    start_time = RTMP_GetTime();

    packets.setWork(1);// 队列开始工作
    callback(pAudioChannel->getAudioTag());// 将音频第一帧添加到队列

    RTMPPacket *packet = nullptr;
    readyPushing = 1;// 准备推流状态

    while (readyPushing) {
        packets.get(packet);// 队列取数据
        if (!readyPushing) {
            break;
        }
        if (packet == nullptr) {
            continue;
        }
        packet->m_nInfoField2 = pRTMP->m_stream_id;// 设置流类型
        ret = RTMP_SendPacket(pRTMP, packet, 1);
        releasePackets(packet);// packet 释放
        LOG_D("上传后 packet 大小：%d", packets.size());
    }

    isStart = 0;
    packets.setWork(0);// 队列停止工作
    packets.clear();// 清空队列
    RTMP_Close(pRTMP);
    RTMP_Free(pRTMP);
    delete (url);
    releaseMediaChannel();

    return nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1init(JNIEnv *env, jobject instance) {

    pVideoChannel = new VideoChannel;
    // 设置回调，当视频数据封装为RTMP数据后的回调
    pVideoChannel->setVideoCallback(callback);

    pAudioChannel = new AudioChannel;
    // 设置回调，当音频数据封装为RTMP数据后的回调
    pAudioChannel->setAudioCallback(callback);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1start(JNIEnv *env, jobject instance, jstring path_) {

    if (isStart) {
        return;
    }
    isStart = 1;

    const char *path = env->GetStringUTFChars(path_, nullptr);

    // 因为开启线程，path_在方法结束后会销毁，这里新建一个变量
    char *url = new char[strlen(path) + 1];
    strcpy(url, path);

    // 参数1：线程id，pthread_t型指针
    // 参数2：线程的属性，nullptr默认属性
    // 参数3：线程创建之后执行的函数，函数指针，可以写&start，由于函数名就是函数指针，所以可以省略&
    // 参数4：start函数接受的参数，void型指针
    pthread_create(&pid, nullptr, start, url);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1setVideoInfo(JNIEnv *env, jobject instance, jint width,
                                                 jint height, jint fps, jint bitrate) {

    if (!pVideoChannel) {
        return;
    }
    pVideoChannel->setVideoInfo(width, height, fps, bitrate);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1pushVideo(JNIEnv *env, jobject instance, jbyteArray data_) {

    if (pVideoChannel == nullptr || !readyPushing) {
        return;
    }

    jbyte *data = env->GetByteArrayElements(data_, nullptr);

    // 这里data是I420编码
    pVideoChannel->encodeData(data);

    env->ReleaseByteArrayElements(data_, data, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1setAudioInfo(JNIEnv *env, jobject instance, jint samplesInHZ,
                                                 jint channels) {

    if (pAudioChannel != nullptr) {
        pAudioChannel->setAudioInfo(samplesInHZ, channels);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1pushAudio(JNIEnv *env, jobject instance, jbyteArray data_) {

    if (pAudioChannel == nullptr || !readyPushing) {
        return;
    }

    jbyte *data = env->GetByteArrayElements(data_, nullptr);

    pAudioChannel->encodeData(data);

    env->ReleaseByteArrayElements(data_, data, 0);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_ff_push_LivePusher_getInputSamples(JNIEnv *env, jobject instance) {

    if (pAudioChannel == nullptr) {
        return -1;
    }
    return pAudioChannel->getInputSamples();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1release(JNIEnv *env, jobject instance) {

    if (readyPushing) {
        readyPushing = 0;
    } else {
        releaseMediaChannel();
    }
}