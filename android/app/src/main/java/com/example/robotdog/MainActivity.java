package com.example.robotdog;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.content.res.ColorStateList;
import android.Manifest;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.ToneGenerator;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Vibrator;
import android.speech.tts.TextToSpeech;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.ScaleAnimation;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity implements TextToSpeech.OnInitListener {

    private static String base() { return "http://" + DogMind.host(); }
    private static final int REQ_MIC = 21;

    private final ExecutorService net = Executors.newFixedThreadPool(4);
    private final ExecutorService brain = Executors.newCachedThreadPool();
    private final ExecutorService pollExec = Executors.newSingleThreadExecutor();
    private final java.util.concurrent.atomic.AtomicBoolean pollBusy = new java.util.concurrent.atomic.AtomicBoolean();
    private TextView statusView;
    private View statusDot;
    private RadarView radar;
    private EditText edtChat;
    private TextToSpeech tts;
    private Vibrator vibrator;
    private ToneGenerator toneGen;
    
    private final Handler poll = new Handler(Looper.getMainLooper());
    private boolean wasFallen = false;
    private boolean wasNear = false;
    private boolean polling = false;
    private volatile boolean hearing;
    private volatile boolean voiceBusy;
    private AudioRecord hearingRec;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        tts = hasPackage("com.google.android.tts")
                ? new TextToSpeech(this, this, "com.google.android.tts")
                : new TextToSpeech(this, this);
        DogMind.start(this, tts);
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        toneGen = new ToneGenerator(android.media.AudioManager.STREAM_ALARM, 100);
        
        statusView = findViewById(R.id.status);
        statusDot = findViewById(R.id.statusDot);
        radar = findViewById(R.id.radar);
        edtChat = findViewById(R.id.edtChat);

        // Nút Mic để nói chuyện với AI
        findViewById(R.id.btnMic).setOnClickListener(v -> {
            animateClick(v);
            if (hearing) hearing = false;
            else startHear();
        });

        // Gán các nút điều khiển di chuyển
        bind(R.id.btnWalk, "walk", "Đi bộ");
        bind(R.id.btnRun, "run", "Chạy");
        bind(R.id.btnStand, "stand", "Đứng");
        bind(R.id.btnStop, "stop", "Dừng");
        
        // Tư thế
        bind(R.id.btnSit4, "sit4", "Ngồi 4 chân");
        bind(R.id.btnSit2, "sit2", "Ngồi 2 chân");
        bind(R.id.btnGetup, "getup", "Dậy");
        bind(R.id.btnRelax, "relax", "Thả servo");
        
        // Tương tác
        bind(R.id.btnBeep, "beep", "Phát Bíp");
        bind(R.id.btnReply, "reply", "Sủa");
        bind(R.id.btnTalkDog, "ear", "Mic Robot");

        findViewById(R.id.btnCheck).setOnClickListener(v -> {
            animateClick(v);
            checkLink();
        });

        findViewById(R.id.btnSend).setOnClickListener(v -> {
            animateClick(v);
            handleChatInput();
        });

        findViewById(R.id.btnHush).setOnClickListener(v -> {
            animateClick(v);
            hush();
        });

        btnCapture = findViewById(R.id.btnCapture);
        btnCapture.setOnClickListener(v -> {
            animateClick(v);
            toggleCapture();
        });

        checkLink();
    }

    private void startHear() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, REQ_MIC);
            Toast.makeText(this, "Cho phép micro rồi bấm mic lại", Toast.LENGTH_SHORT).show();
            return;
        }
        hearing = true;
        show("Đang nghe... bấm mic lần nữa để gửi", R.color.warn);
        final int my = DogMind.epoch();
        new Thread(() -> {
            AudioRecord rec = null;
            try {
                int min = AudioRecord.getMinBufferSize(16000,
                        AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
                rec = new AudioRecord(MediaRecorder.AudioSource.MIC, 16000,
                        AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, Math.max(min, 4096));
                hearingRec = rec;
                rec.startRecording();
                ByteArrayOutputStream pcm = new ByteArrayOutputStream();
                byte[] buf = new byte[1280];
                long t0 = System.currentTimeMillis();
                while (hearing && System.currentTimeMillis() - t0 < 8000) {
                    int n = rec.read(buf, 0, buf.length);
                    if (n > 0) pcm.write(buf, 0, n);
                }
                byte[] voice = DogMind.trimSilence(pcm.toByteArray(), 16000);
                byte[] wav = voice.length < 4800 ? new byte[0] : DogMind.wav(voice, 16000);
                if (my == DogMind.epoch()) runOnUiThread(() -> onHeard(wav));
            } catch (Exception e) {
                show("Không mở được mic", R.color.bad);
            } finally {
                hearing = false;
                if (rec != null) {
                    try { rec.stop(); } catch (Exception ignored) {}
                    rec.release();
                }
                hearingRec = null;
            }
        }, "dog-mic").start();
    }

    private void onHeard(byte[] wav) {
        if (wav.length < 2000) {
            show("Chưa nghe rõ, nói lại giúp mình", R.color.warn);
            return;
        }
        show("Đang hiểu câu nói...", R.color.warn);
        final int my = DogMind.epoch();
        brain.execute(() -> {
            try {
                String text = DogMind.hear(wav);
                if (my != DogMind.epoch()) return;
                if (text.isEmpty()) {
                    show("Chưa nghe rõ, nói lại giúp mình", R.color.warn);
                    return;
                }
                runOnUiThread(() -> {
                    edtChat.setText(text);
                    handleChatInput();
                });
            } catch (Exception e) {
                show("Không nhận ra lời. Bật dữ liệu di động rồi nói lại.", R.color.bad);
            }
        });
    }

    private void animateClick(View v) {
        ScaleAnimation anim = new ScaleAnimation(1f, 0.9f, 1f, 0.9f, 
                Animation.RELATIVE_TO_SELF, 0.5f, Animation.RELATIVE_TO_SELF, 0.5f);
        anim.setDuration(100);
        anim.setRepeatCount(1);
        anim.setRepeatMode(Animation.REVERSE);
        v.startAnimation(anim);
        if (vibrator != null) vibrator.vibrate(30);
    }

    private void handleChatInput() {
        String input = edtChat.getText().toString().trim();
        if (input.isEmpty()) return;

        edtChat.setText("");
        voiceBusy = true;
        show("Đang nghĩ...", R.color.warn);
        DogMind.hush();
        final int my = DogMind.epoch();
        brain.execute(() -> {
            try {
                DogMind.Reply reply = DogMind.think(input);
                if (my != DogMind.epoch()) return;
                boolean stop = false;
                for (String action : reply.actions) {
                    if ("stop".equals(action)) stop = true;
                    final String act = action;
                    runOnUiThread(() -> tap(act));
                }
                if (stop) reply.music = null;
                DogMind.stopAudio();
                boolean wantMusic = reply.music != null && !reply.music.isEmpty();
                java.util.concurrent.Future<DogMind.Track> found = wantMusic ? DogMind.findMusic(reply.music) : null;
                final String said = reply.say == null ? "" : reply.say;
                runOnUiThread(() -> show("Chó: " + said, R.color.ok));
                try {
                    DogMind.speakOnDog(said);
                } catch (Exception speakErr) {
                    if (my != DogMind.epoch()) return;
                    android.util.Log.w("DogMind", "speak", speakErr);
                    show("no local tts".equals(speakErr.getMessage())
                            ? "Giọng Gemini hết lượt hôm nay. Cài Speech Services by Google để chó nói tiếng Việt."
                            : "Chó chưa nói được. Kiểm tra loa và dữ liệu di động.", R.color.bad);
                }
                if (my != DogMind.epoch()) return;
                if (wantMusic) {
                    show("Đang mở nhạc: " + reply.music.replaceFirst("(?i)^(song|radio):\\s*", ""), R.color.warn);
                    try {
                        DogMind.playMusic(found);
                        if (my != DogMind.epoch()) return;
                        show("Đã phát nhạc trên loa chó", R.color.ok);
                    } catch (Exception musicErr) {
                        if (my != DogMind.epoch()) return;
                        android.util.Log.w("DogMind", "music", musicErr);
                        try { DogMind.speakOnDog("Mình không tìm thấy bài nhạc đó"); } catch (Exception ignored) {}
                        show("Không tìm thấy nhạc miễn phí đúng tên đó", R.color.bad);
                    }
                }
            } catch (Exception e) {
                if (my == DogMind.epoch()) show("Chưa xử lý được câu nói", R.color.bad);
            } finally {
                if (my == DogMind.epoch()) voiceBusy = false;
            }
        });
    }

    private android.widget.Button btnCapture;

    private final androidx.activity.result.ActivityResultLauncher<Intent> askCapture =
            registerForActivityResult(new androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult(), r -> {
                if (r.getResultCode() != RESULT_OK || r.getData() == null) {
                    show("Chưa cho phép thu tiếng điện thoại", R.color.bad);
                    return;
                }
                Intent svc = new Intent(this, CaptureService.class)
                        .putExtra(CaptureService.EXTRA_CODE, r.getResultCode())
                        .putExtra(CaptureService.EXTRA_DATA, r.getData());
                startForegroundService(svc);
                btnCapture.setText("Đang phát tiếng điện thoại ra loa chó · bấm để tắt");
                show("Mở Spotify / YouTube Music và bật nhạc", R.color.ok);
            });

    private void toggleCapture() {
        if (Build.VERSION.SDK_INT < 29) {
            show("Cần Android 10 trở lên", R.color.bad);
            return;
        }
        if (CaptureService.isRunning()) {
            stopCapture();
            return;
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
                != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, REQ_MIC);
            show("Cho phép micro rồi bấm lại", R.color.warn);
            return;
        }
        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_MIC + 1);
        }
        android.media.projection.MediaProjectionManager mpm = getSystemService(android.media.projection.MediaProjectionManager.class);
        askCapture.launch(mpm.createScreenCaptureIntent());
    }

    private void stopCapture() {
        if (Build.VERSION.SDK_INT >= 29 && CaptureService.isRunning()) CaptureService.stop(this);
        if (btnCapture != null) btnCapture.setText("Phát nhạc Spotify / YouTube ra loa chó");
    }

    private boolean hasPackage(String pkg) {
        try {
            getPackageManager().getPackageInfo(pkg, 0);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private void hush() {
        stopCapture();
        DogMind.hush();
        hearing = false;
        voiceBusy = false;
        show("Đã dừng", R.color.ok);
        net.execute(() -> {
            try { httpGet(base() + "/cmd?c=hush"); } catch (Exception ignored) {}
        });
    }

    private void tap(String action) {
        int id;
        switch (action) {
            case "walk": id = R.id.btnWalk; break;
            case "run": id = R.id.btnRun; break;
            case "stand": id = R.id.btnStand; break;
            case "stop": id = R.id.btnStop; break;
            case "sit4": id = R.id.btnSit4; break;
            case "sit2": id = R.id.btnSit2; break;
            case "getup": id = R.id.btnGetup; break;
            case "beep": id = R.id.btnBeep; break;
            case "shakel": send("shakel", "Bắt tay trái"); return;
            case "shaker": send("shaker", "Bắt tay phải"); return;
            default: return;
        }
        View v = findViewById(id);
        if (v != null) v.performClick();
    }

    @Override
    public void onInit(int status) {
        boolean vi = false;
        if (status == TextToSpeech.SUCCESS) {
            int r = tts.setLanguage(new Locale("vi", "VN"));
            vi = r != TextToSpeech.LANG_MISSING_DATA && r != TextToSpeech.LANG_NOT_SUPPORTED;
            android.util.Log.i("DogMind", "local tts " + tts.getDefaultEngine() + " vi=" + r);
        }
        // Bộ đọc không có tiếng Việt thì đọc thành giọng Anh khó nghe: thà không dùng.
        DogMind.onTtsReady(vi);
    }

    @Override
    protected void onResume() {
        super.onResume();
        polling = true;
        poll.post(statusTick);
        if (btnCapture != null) {
            boolean on = Build.VERSION.SDK_INT >= 29 && CaptureService.isRunning();
            btnCapture.setText(on ? "Đang phát tiếng điện thoại ra loa chó · bấm để tắt"
                    : "Phát nhạc Spotify / YouTube ra loa chó");
        }
    }

    @Override
    protected void onPause() {
        polling = false;
        poll.removeCallbacks(statusTick);
        super.onPause();
    }

    private final Runnable statusTick = new Runnable() {
        @Override
        public void run() {
            if (!polling) return;
            poll.postDelayed(this, 1000);
            if (!pollBusy.compareAndSet(false, true)) return;
            pollExec.execute(() -> {
                try {
                    String body = httpGet(base() + "/status");
                    float cm = jsonFloat(body, "cm", -1f);
                    boolean fallen = body.contains("\"fallen\":true");
                    
                    runOnUiThread(() -> {
                        if (radar != null) radar.setDistance(cm);
                        
                        // Cảnh báo vật cản
                        if (cm > 0 && cm < 30) {
                            if (!wasNear) {
                                toneGen.startTone(ToneGenerator.TONE_CDMA_PIP, 200);
                                vibrator.vibrate(200);
                                Toast.makeText(MainActivity.this, "CẨN THẬN: Vật cản!", Toast.LENGTH_SHORT).show();
                            }
                            show("VẬT CẢN: " + (int)cm + "cm", R.color.warn);
                            wasNear = true;
                        } else { wasNear = false; }

                        // Cảnh báo ngã
                        if (fallen) {
                            if (!wasFallen) {
                                toneGen.startTone(ToneGenerator.TONE_SUP_ERROR, 500);
                                vibrator.vibrate(500);
                                Toast.makeText(MainActivity.this, "CHÓ BỊ NGÃ!", Toast.LENGTH_LONG).show();
                            }
                            show("ROBOT BỊ NGÃ", R.color.bad);
                        }
                        wasFallen = fallen;
                    });
                } catch (Exception ignored) {
                } finally {
                    pollBusy.set(false);
                }
            });
        }
    };

    private float jsonFloat(String json, String key, float fallback) {
        try {
            int i = json.indexOf("\"" + key + "\"");
            int c = json.indexOf(':', i);
            int end = c + 1;
            while (end < json.length() && "0123456789.-".indexOf(json.charAt(end)) >= 0) end++;
            return Float.parseFloat(json.substring(c + 1, end).trim());
        } catch (Exception e) { return fallback; }
    }

    private void bind(int id, String cmd, String label) {
        View v = findViewById(id);
        if (v != null) v.setOnClickListener(view -> {
            animateClick(view);
            send(cmd, label);
        });
    }

    private void send(String cmd, String label) {
        if (!voiceBusy) show("Gửi lệnh...", R.color.warn);
        net.execute(() -> {
            try {
                httpGet(base() + "/cmd?c=" + URLEncoder.encode(cmd, "UTF-8"));
                if (!voiceBusy) show("Xong: " + label, R.color.ok);
            } catch (Exception e) { show("Mất kết nối Robot", R.color.bad); }
        });
    }

    private void checkLink() {
        show("Đang tìm Robot...", R.color.warn);
        net.execute(() -> {
            try {
                httpGet(base() + "/status");
                show(DogMind.viaHotspot() ? "Liên kết Robot OK qua hotspot" : "Liên kết Robot OK", R.color.ok);
            } catch (Exception e) { show("Không thấy Robot", R.color.bad); }
        });
    }

    private static String httpGet(String url) throws Exception {
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setConnectTimeout(1500);
        conn.setReadTimeout(1500);
        InputStream is = conn.getResponseCode() >= 400 ? conn.getErrorStream() : conn.getInputStream();
        BufferedReader r = new BufferedReader(new InputStreamReader(is, StandardCharsets.UTF_8));
        StringBuilder sb = new StringBuilder();
        String l;
        while ((l = r.readLine()) != null) sb.append(l);
        r.close();
        conn.disconnect();
        return sb.toString();
    }

    private void show(String msg, int colorRes) {
        runOnUiThread(() -> {
            statusView.setText(msg);
            statusDot.setBackgroundTintList(ColorStateList.valueOf(getColor(colorRes)));
        });
    }

    private String read(InputStream is) throws Exception {
        BufferedReader r = new BufferedReader(new InputStreamReader(is, StandardCharsets.UTF_8));
        StringBuilder sb = new StringBuilder();
        String l;
        while ((l = r.readLine()) != null) sb.append(l);
        r.close();
        return sb.toString();
    }

    @Override
    protected void onDestroy() {
        polling = false;
        hearing = false;
        DogMind.stopAudio();
        if (tts != null) { tts.stop(); tts.shutdown(); }
        if (toneGen != null) toneGen.release();
        net.shutdownNow();
        brain.shutdownNow();
        pollExec.shutdownNow();
        super.onDestroy();
    }
}
