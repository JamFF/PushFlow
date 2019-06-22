package com.ff.push;

import android.app.Activity;
import android.view.SurfaceHolder;

import com.ff.push.meida.AudioChannel;
import com.ff.push.meida.VideoChannel;

public class LivePusher {

    static {
        System.loadLibrary("native-lib");
    }

    private AudioChannel audioChannel;
    private VideoChannel videoChannel;

    /**
     * 构造方法
     *
     * @param activity
     * @param width       视频采集宽度
     * @param height      视频采集高度
     * @param bitrate     视频码率
     * @param fps         视频帧率
     * @param cameraId    视频摄像头
     * @param samplesInHZ 音频采样率
     * @param channels    音频声道
     * @param audioFormat 音频采样精度
     * @param audioSource 音频麦克风
     */
    public LivePusher(Activity activity, int width, int height, int bitrate, int fps, int cameraId,
                      int samplesInHZ, int channels, int audioFormat, int audioSource) {
        native_init();
        videoChannel = new VideoChannel(this, activity, width, height, bitrate, fps, cameraId);
        audioChannel = new AudioChannel(this, samplesInHZ, channels, audioFormat, audioSource);
    }

    /**
     * 设置摄像头预览的界面
     *
     * @param surfaceHolder {@link android.view.SurfaceHolder}
     */
    public void setPreviewDisplay(SurfaceHolder surfaceHolder) {
        videoChannel.setPreviewDisplay(surfaceHolder);
    }

    /**
     * 切换前后摄像头
     */
    public void switchCamera() {
        videoChannel.switchCamera();
    }

    /**
     * 重新开启摄像头
     */
    public void restartPreview() {
        videoChannel.restartPreview();
    }

    /**
     * 开启推流
     *
     * @param path 推流地址
     */
    public void startLive(String path) {
        native_start(path);
        videoChannel.startLive();
        audioChannel.startLive();
    }

    public void release() {
        videoChannel.release();
        audioChannel.release();
        native_release();
    }

    private native void native_init();

    // 设置推流地址，并开始推流
    private native void native_start(String path);

    // 设置视频采集信息
    public native void native_setVideoInfo(int width, int height, int fps, int bitrate);

    // 推视频流
    public native void native_pushVideo(byte[] data);

    // 设置音频采集信息
    public native void native_setAudioInfo(int samplesInHZ, int channels);

    // 推音频流
    public native void native_pushAudio(byte[] data);

    // 获取faac编码器返回的缓冲区大小
    public native int getInputSamples();

    // 释放资源
    public native void native_release();
}
