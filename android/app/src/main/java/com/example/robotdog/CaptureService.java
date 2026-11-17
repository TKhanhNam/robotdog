package com.example.robotdog;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.RequiresApi;

import java.io.OutputStream;
import java.net.Socket;

/** Thu tiếng các app khác đang phát (Spotify, YouTube Music, Zing...) rồi đẩy sang loa chó. */
@RequiresApi(29)
public class CaptureService extends Service {
    static final String EXTRA_CODE = "code";
    static final String EXTRA_DATA = "data";
    private static final String TAG = "DogCapture";
    private static final String CHANNEL = "dog-capture";

    private static volatile boolean running;
    private volatile boolean on;
    private MediaProjection projection;
    private Thread worker;

    static boolean isRunning() { return running; }

    static void stop(Context ctx) {
        ctx.stopService(new Intent(ctx, CaptureService.class));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        NotificationManager nm = getSystemService(NotificationManager.class);
        nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Loa chó", NotificationManager.IMPORTANCE_LOW));
        Notification n = new Notification.Builder(this, CHANNEL)
                .setSmallIcon(R.drawable.ic_paw)
                .setContentTitle("Đang phát tiếng điện thoại ra loa chó")
                .setContentText("Mở app RobotDog, bấm nút dừng để tắt")
                .setOngoing(true)
                .build();
        startForeground(1, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);

        if (intent == null || projection != null) return START_NOT_STICKY;
        MediaProjectionManager mpm = getSystemService(MediaProjectionManager.class);
        projection = mpm.getMediaProjection(intent.getIntExtra(EXTRA_CODE, 0), intent.getParcelableExtra(EXTRA_DATA));
        if (projection == null) {
            stopSelf();
            return START_NOT_STICKY;
        }
        projection.registerCallback(new MediaProjection.Callback() {
            @Override public void onStop() { stopSelf(); }
        }, new Handler(Looper.getMainLooper()));
        running = true;
        on = true;
        worker = new Thread(this::pump, "dog-capture");
        worker.start();
        return START_NOT_STICKY;
    }

    private void pump() {
        AudioRecord rec = null;
        try {
            AudioPlaybackCaptureConfiguration cfg = new AudioPlaybackCaptureConfiguration.Builder(projection)
                    .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                    .addMatchingUsage(AudioAttributes.USAGE_GAME)
                    .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                    .build();
            int min = AudioRecord.getMinBufferSize(16000, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
            rec = new AudioRecord.Builder()
                    .setAudioFormat(new AudioFormat.Builder()
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                            .setSampleRate(16000)
                            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                            .build())
                    .setBufferSizeInBytes(Math.max(min, 16000))
                    .setAudioPlaybackCaptureConfig(cfg)
                    .build();
            rec.startRecording();
            byte[] buf = new byte[640];
            while (on) {
                // Chó đang nói câu trả lời / phát nhạc AI tìm: nhường loa, vẫn đọc để bộ đệm thu không đầy.
                if (DogMind.audioBusy()) {
                    rec.read(buf, 0, buf.length);
                    continue;
                }
                Socket s;
                try {
                    s = DogMind.openDogStream();
                } catch (Exception e) {
                    Log.w(TAG, "connect: " + e.getMessage());
                    Thread.sleep(1000);
                    continue;
                }
                try {
                    OutputStream os = s.getOutputStream();
                    os.write(new byte[]{'P', 'C', 'M', '1'});
                    while (on && !DogMind.audioBusy()) {
                        int n = rec.read(buf, 0, buf.length);
                        if (n > 0) os.write(buf, 0, n);
                    }
                } catch (Exception e) {
                    Log.w(TAG, "stream: " + e.getMessage());
                    Thread.sleep(500);
                } finally {
                    try { s.close(); } catch (Exception ignored) {}
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "capture", e);
        } finally {
            if (rec != null) {
                try { rec.stop(); } catch (Exception ignored) {}
                rec.release();
            }
            stopSelf();
        }
    }

    @Override
    public void onDestroy() {
        on = false;
        running = false;
        if (worker != null) worker.interrupt();
        if (projection != null) projection.stop();
        projection = null;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
