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


    public LivePusher(Activity activity, int width, int height, int bitrate,
                      int fps, int cameraId) {
        native_init();
        videoChannel = new VideoChannel(this, activity, width, height, bitrate, fps, cameraId);
        audioChannel = new AudioChannel(this);
    }

    public void setPreviewDisplay(SurfaceHolder surfaceHolder) {
        videoChannel.setPreviewDisplay(surfaceHolder);
    }

    public void switchCamera() {
        videoChannel.switchCamera();
    }

    /**
     * 重新开启摄像头
     */
    public void restartPreview() {
        videoChannel.restartPreview();
    }

    public void startLive(String path) {
        native_start(path);
        videoChannel.startLive();
    }

    private native void native_init();

    public native void native_setVideoInfo(int width, int height, int fps, int bitrate);

    private native void native_start(String path);

    public native void native_pushVideo(byte[] data);
}
