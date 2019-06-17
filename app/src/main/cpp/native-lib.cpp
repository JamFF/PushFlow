#include <jni.h>
#include <string>
#include "x264.h"
#include "librtmp/rtmp.h"
#include "VideoChannel.h"
#include "macro.h"
#include "safe_queue.h"

// 为了防止用户重复点击开始直播，导致重新初始化
int isStart = 0;
VideoChannel *pVideoChannel;
pthread_t pid;
uint32_t start_time;// 记录开始时间
int readyPushing = 0;// 是否准备好推流
// FIXME 提示 initialization of 'packets' with static storage duration may throw an exception that cannot be caught
SafeQueue<RTMPPacket *> packets;// 队列

void callback(RTMPPacket *packet) {

    if (packet) {
        // 设置时间戳
        packet->m_nTimeStamp = RTMP_GetTime() - start_time;
        // 加入队列
        packets.put(packet);
    }
}

void releasePackets(RTMPPacket *packet) {
    if (packet) {
        RTMPPacket_Free(packet);
        delete packet;
        packet = nullptr;
    }
}

/**
 * 被线程调用的指针函数
 * @param args 
 * @return 
 */
void *start(void *args) {
    char *url = static_cast<char *>(args);
    RTMP *pRTMP = RTMP_Alloc();// 申请内存
    if (!pRTMP) {
        LOG_E("alloc rtmp失败");
        return nullptr;
    }
    RTMP_Init(pRTMP);// 初始化
    int ret = RTMP_SetupURL(pRTMP, url);// 设置地址
    if (!ret) {
        LOG_E("设置地址失败:%s", url);
        return nullptr;
    }

    pRTMP->Link.timeout = 5;
    RTMP_EnableWrite(pRTMP);// 开启输出模式
    ret = RTMP_Connect(pRTMP, nullptr);// 连接服务器
    if (!ret) {
        LOG_E("连接服务器:%s", url);
        return nullptr;
    }

    ret = RTMP_ConnectStream(pRTMP, 0);// 连接流
    if (!ret) {
        LOG_E("连接流:%s", url);
        return nullptr;
    }
    start_time = RTMP_GetTime();
    packets.setWork(1);// 队列开始工作
    RTMPPacket *packet = nullptr;
    // 表示可以开始推流了
    readyPushing = 1;
    while (readyPushing) {
        packets.get(packet);// 队列取数据
        LOG_D("取出一帧数据");
        if (!readyPushing) {
            break;
        }
        if (packet == nullptr) {
            continue;
        }
        packet->m_nInfoField2 = pRTMP->m_stream_id;// 设置流类型
        ret = RTMP_SendPacket(pRTMP, packet, 1);
        releasePackets(packet);// packet 释放
    }

    isStart = 0;
    readyPushing = 0;
    packets.setWork(0);// 队列停止工作
    packets.clear();
    RTMP_Close(pRTMP);
    RTMP_Free(pRTMP);
    delete (url);
    return nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ff_push_LivePusher_native_1init(JNIEnv *env, jobject instance) {

    pVideoChannel = new VideoChannel;
    pVideoChannel->setVideoCallback(callback);
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
Java_com_ff_push_LivePusher_native_1start(JNIEnv *env, jobject instance, jstring path_) {
    const char *path = env->GetStringUTFChars(path_, nullptr);

    if (isStart) {
        return;
    }
    isStart = 1;

    // 因为开启线程，path_在方法结束后会销毁，这里新建一个变量
    char *url = new char[strlen(path) + 1];
    strcpy(url, path);

    pthread_create(&pid, nullptr, start, url);

    env->ReleaseStringUTFChars(path_, path);
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