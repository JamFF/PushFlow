package com.ff.push.meida;

import android.app.Activity;
import android.hardware.Camera;
import android.view.SurfaceHolder;

import com.ff.push.LivePusher;

public class VideoChannel implements Camera.PreviewCallback, CameraHelper.OnChangedSizeListener {

    private static final String TAG = "VideoChannel";
    private LivePusher mLivePusher;
    private CameraHelper mCameraHelper;
    private int mBitrate;// 码率，单位kbps 即千位每秒(kb/s)
    private int mFps;// 帧率
    private boolean isLive;// 开始直播

    /**
     * 视频通道构造方法
     *
     * @param livePusher
     * @param activity
     * @param width      视频采集宽度
     * @param height     视频采集高度
     * @param bitrate    视频码率
     * @param fps        视频帧率
     * @param cameraId   视频摄像头
     */
    public VideoChannel(LivePusher livePusher, Activity activity, int width, int height,
                        int bitrate, int fps, int cameraId) {
        mLivePusher = livePusher;
        mBitrate = bitrate;
        mFps = fps;
        mCameraHelper = new CameraHelper(activity, cameraId, width, height);
        mCameraHelper.setPreviewCallback(this);
        mCameraHelper.setOnChangedSizeListener(this);
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
//        Log.d(TAG, "onPreviewFrame: 获取到一帧");
        if (isLive) {
            mLivePusher.native_pushVideo(data);
        }
    }

    @Override
    public void onChanged(int w, int h) {
        // 这里是摄像头宽高改变的回调，由摄像头方向改变引起的，不是SurfaceView的宽高改变的回调
        mLivePusher.native_setVideoInfo(w, h, mFps, mBitrate);
    }

    /**
     * 切换前后摄像头
     */
    public void switchCamera() {
        mCameraHelper.switchCamera();
    }

    /**
     * 设置摄像头预览的界面
     *
     * @param surfaceHolder {@link android.view.SurfaceHolder}
     */
    public void setPreviewDisplay(SurfaceHolder surfaceHolder) {
        mCameraHelper.setPreviewDisplay(surfaceHolder);
    }

    /**
     * 重新开启摄像头
     */
    public void restartPreview() {
        mCameraHelper.restartPreview();
    }

    /**
     * 开启推流
     */
    public void startLive() {
        isLive = true;
    }

    public void release() {
        mCameraHelper.release();
    }
}
