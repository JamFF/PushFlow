package com.ff.push;

import android.Manifest;
import android.hardware.Camera;
import android.media.AudioFormat;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

import java.util.List;

import pub.devrel.easypermissions.AfterPermissionGranted;
import pub.devrel.easypermissions.AppSettingsDialog;
import pub.devrel.easypermissions.EasyPermissions;

public class MainActivity extends AppCompatActivity implements View.OnClickListener,
        EasyPermissions.PermissionCallbacks {

    private static final String TAG_PERMISSION = "Permission";
    private static final int PERMISSION_CODE = 10001;
    private static final String PERMISSION_MSG = "需要摄像头和录音权限，否则无法正常使用";
    private static final String[] PERMS = {Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO};

    // 填入服务器推流地址
    private static final String PATH = "rtmp://xxx";

    private LivePusher mLivePusher;
    private SurfaceView mSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        initView();
        initData();
    }

    private void initView() {
        mSurfaceView = findViewById(R.id.surfaceView);
        findViewById(R.id.bt_camera).setOnClickListener(this);
        findViewById(R.id.bt_start).setOnClickListener(this);
        findViewById(R.id.bt_stop).setOnClickListener(this);
    }

    @AfterPermissionGranted(PERMISSION_CODE)
    private void initData() {
        if (!EasyPermissions.hasPermissions(this, PERMS)) {
            // 申请权限
            EasyPermissions.requestPermissions(this, PERMISSION_MSG, PERMISSION_CODE, PERMS);
            return;
        }
        mLivePusher = new LivePusher(this, 800, 480, // 视频宽高
                800, // 视频码率，单位kbps 即千位每秒(kb/s)
                10, // 视频帧率 每秒10帧
                Camera.CameraInfo.CAMERA_FACING_BACK,// 使用后置摄像头
                44100,// 音频采样率
                2,// 音频声道
                AudioFormat.ENCODING_PCM_16BIT,// 采样精度
                MediaRecorder.AudioSource.MIC);// 使用主麦克风

        // 设置摄像头预览的界面
        mLivePusher.setPreviewDisplay(mSurfaceView.getHolder());
        if (!mSurfaceView.getHolder().isCreating()) {
            // surface创建完成
            mLivePusher.restartPreview();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        EasyPermissions.onRequestPermissionsResult(requestCode, permissions, grantResults, this);
    }

    @Override
    public void onPermissionsGranted(int requestCode, @NonNull List<String> perms) {
        if (requestCode == PERMISSION_CODE) {
            Log.d(TAG_PERMISSION, "onPermissionsGranted: ");
        }
    }

    @Override
    public void onPermissionsDenied(int requestCode, @NonNull List<String> perms) {
        if (EasyPermissions.somePermissionPermanentlyDenied(this, perms)) {
            // 拒绝权限，并不再询问
            new AppSettingsDialog
                    .Builder(this)
                    .setTitle("授权提醒")
                    .setRationale(PERMISSION_MSG)
                    .setPositiveButton("打开设置")
                    .setNegativeButton("取消")
                    .build()
                    .show();
        } else {
            // 拒绝权限
            if (requestCode == PERMISSION_CODE) {
                Log.d(TAG_PERMISSION, "onPermissionsDenied: ");
            }
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.bt_camera:
                mLivePusher.switchCamera();// 切换前后摄像头
                break;
            case R.id.bt_start:
                mLivePusher.startLive(PATH);
                break;
            case R.id.bt_stop:
                mLivePusher.release();
                break;
        }
    }

    @Override
    protected void onDestroy() {
        mLivePusher.release();
        super.onDestroy();
    }
}
