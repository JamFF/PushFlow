package com.ff.push.meida;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.util.Log;

import com.ff.push.LivePusher;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AudioChannel {

    private static final String TAG = "AudioChannel";

    private LivePusher mLivePusher;
    private AudioRecord mAudioRecord;

    private int inputSamples;// native返回的缓冲区大小

    private boolean isLive;// 开始直播

    private ExecutorService mExecutor;// 单线程线程池

    /**
     * 音频通道构造方法
     *
     * @param livePusher
     * @param samplesInHZ 音频采样率
     * @param channels    音频声道
     * @param audioFormat 音频采样精度
     * @param audioSource 音频麦克风
     */
    public AudioChannel(LivePusher livePusher, int samplesInHZ, int channels,
                        int audioFormat, int audioSource) {
        mExecutor = Executors.newSingleThreadExecutor();

        mLivePusher = livePusher;
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_IN_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_IN_STEREO;
        } else {
            throw new RuntimeException("The " + channels + " of channels is not supported");
        }

        // 设置录音信息
        mLivePusher.native_setAudioInfo(samplesInHZ, channels);

        // 8位是一个字节，16位的采样精度，需要乘以2
        inputSamples = mLivePusher.getInputSamples() * 2;

        // AudioRecord 计算的最小缓冲区大小，这里不需要乘以2，因为参数中已经传入了16位采样精度
        int minBufferSize = AudioRecord.getMinBufferSize(samplesInHZ,
                channelConfig,
                audioFormat);

        Log.d(TAG, "inputSamples = " + inputSamples);
        Log.d(TAG, "minBufferSize = " + minBufferSize);

        mAudioRecord = new AudioRecord(audioSource,// 麦克风
                samplesInHZ,// 采样率
                channelConfig,// 声道
                audioFormat,// 采样精度
                minBufferSize < inputSamples ? inputSamples : minBufferSize);// 最小缓冲buffer，取较大值
    }

    /**
     * 开启推流
     */
    public void startLive() {
        isLive = true;
        mExecutor.submit(new AudioTask(this));
    }

    public void release() {
        mAudioRecord.release();
    }

    private static class AudioTask implements Runnable {

        private WeakReference<AudioChannel> mReference;

        private AudioTask(AudioChannel audioChannel) {
            mReference = new WeakReference<>(audioChannel);
        }

        @Override
        public void run() {
            if (mReference == null || mReference.get() == null) {
                return;
            }
            mReference.get().mAudioRecord.startRecording();
            // PCM 音频原始数据，注意这里不是minBufferSize，会造成语速有问题，因为minBufferSize不是真实的大小
            byte[] bytes = new byte[mReference.get().inputSamples];
            while (mReference != null && mReference.get() != null && mReference.get().isLive) {
                int len = mReference.get().mAudioRecord.read(bytes, 0, bytes.length);
                // 推送音频数据
                mReference.get().mLivePusher.native_pushAudio(bytes);
            }
        }
    }
}
