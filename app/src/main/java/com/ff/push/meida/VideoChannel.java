package com.ff.push.meida;

import android.app.Activity;
import android.hardware.Camera;
import android.util.Log;
import android.view.SurfaceHolder;

import com.ff.push.LivePusher;

public class VideoChannel implements Camera.PreviewCallback, CameraHelper.OnChangedSizeListener {
    private static final String TAG = "tuch";
    private CameraHelper cameraHelper;
    private int mBitrate;
    private int mFps;
    private boolean isLive;// 直播中
    private LivePusher mLivePusher;

    public VideoChannel(LivePusher livePusher, Activity activity, int width, int height, int bitrate, int fps, int cameraId) {
        mLivePusher = livePusher;
        mBitrate = bitrate;
        mFps = fps;
        cameraHelper = new CameraHelper(activity, cameraId, width, height);
        cameraHelper.setPreviewCallback(this);
        cameraHelper.setOnChangedSizeListener(this);
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        Log.d(TAG, "onPreviewFrame: 获取到一帧");
        if (isLive) {
            mLivePusher.native_pushVideo(data);
        }
    }

    @Override
    public void onChanged(int w, int h) {
        // 这里是摄像头宽高改变的回调，由摄像头方向改变引起的，不是SurfaceView的宽高改变的回调
        mLivePusher.native_setVideoInfo(w, h, mFps, mBitrate);
    }

    public void switchCamera() {
        cameraHelper.switchCamera();
    }

    public void setPreviewDisplay(SurfaceHolder surfaceHolder) {
        cameraHelper.setPreviewDisplay(surfaceHolder);
    }

    public void restartPreview() {
        cameraHelper.restartPreview();
    }

    public void startLive() {
        isLive = true;
    }
}
