package com.example.robotdog;

import android.content.Context;
import android.media.MediaCodec;
import android.media.MediaDataSource;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Nghe – nghĩ – nói qua loa chó. Internet đi bằng 4G, tiếng đi bằng WiFi robot. */
final class DogMind {
    static final class Reply {
        String say = "";
        String[] actions = new String[0];
        String music;
        boolean handshake;
    }

    private static final String TAG = "DogMind";
    private static final String AP_HOST = "192.168.4.1";
    private static final int BEACON_PORT = 4210;
    private static volatile String beaconHost;
    private static volatile long beaconAt;
    private static final int PCM_PORT = 81;

    private static Context app;
    private static TextToSpeech tts;
    private static volatile boolean ttsReady;
    private static volatile Network cell;
    private static volatile Network wifi;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final Object AUDIO = new Object();
    private static volatile boolean cancel;
    private static Socket audioSock;
    private static final ArrayList<String> memUser = new ArrayList<>();
    private static final ArrayList<String> memDog = new ArrayList<>();
    private static final String PERSONA =
            "Bạn là một chú chó robot, nói chuyện qua loa gắn trên người. "
                    + "Hãy trò chuyện như một AI bình thường: hỏi gì trả lời đó, đủ ý, tự nhiên, bằng tiếng Việt. "
                    + "Luôn nhận mình là chó robot và xưng 'mình'. Đừng nói mình là mô hình của Google, Groq hay công ty nào. "
                    + "Chỉ trả về JSON, không markdown, đúng dạng "
                    + "{\"say\":\"câu trả lời đầy đủ\",\"actions\":[],\"music\":\"\"}. "
                    + "say là nội dung trả lời thật, có thể dài vài câu. "
                    + "actions chỉ điền khi người dùng ra lệnh cử động, mỗi phần tử là một trong: "
                    + "walk, run, stand, stop, sit4, sit2, getup, beep, shakel, shaker. "
                    + "walk=đi, run=chạy, stand=đứng, stop=dừng, sit4=ngồi bốn chân, sit2=ngồi hai chân sau, getup=dậy, "
                    + "shakel=bắt tay bằng chân trước trái, shaker=bắt tay bằng chân trước phải. "
                    + "Nếu họ chỉ hỏi chuyện thì actions để mảng rỗng. "
                    + "music chỉ điền khi họ bảo phát nhạc, còn lại để chuỗi rỗng. "
                    + "Nếu họ nói tên bài hoặc ca sĩ cụ thể thì ghi \"song: tên bài tên ca sĩ\" (giữ nguyên dấu tiếng Việt), "
                    + "ví dụ \"song: Lạc Trôi Sơn Tùng\". "
                    + "Nếu họ chỉ nói thể loại hoặc tâm trạng thì ghi \"radio: \" kèm một từ khóa tiếng Anh như "
                    + "lofi, jazz, chill, piano, classical, pop, rock, edm, kpop, hoặc \"radio: vietnam\" cho nhạc Việt. "
                    + "Khi mở nhạc, say chỉ cần một câu ngắn.";

    private DogMind() {}

    static void start(Context ctx, TextToSpeech engine) {
        app = ctx.getApplicationContext();
        tts = engine;
        ConnectivityManager cm = (ConnectivityManager) app.getSystemService(Context.CONNECTIVITY_SERVICE);
        cm.requestNetwork(new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build(), new ConnectivityManager.NetworkCallback() {
            @Override public void onAvailable(Network network) { cell = network; }
            @Override public void onLost(Network network) { if (network.equals(cell)) cell = null; }
        });
        cm.requestNetwork(new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build(), new ConnectivityManager.NetworkCallback() {
            @Override public void onAvailable(Network network) { wifi = network; }
            @Override public void onLost(Network network) { if (network.equals(wifi)) wifi = null; }
        });
        listenBeacon();
    }

    /** Địa chỉ chó: IP trên hotspot nếu vừa nghe chó báo, không thì WiFi riêng của chó. */
    static String host() {
        String h = beaconHost;
        return h != null && System.currentTimeMillis() - beaconAt < 8000 ? h : AP_HOST;
    }

    static boolean viaHotspot() { return !AP_HOST.equals(host()); }

    private static void listenBeacon() {
        Thread t = new Thread(() -> {
            android.net.wifi.WifiManager wm = (android.net.wifi.WifiManager)
                    app.getSystemService(Context.WIFI_SERVICE);
            android.net.wifi.WifiManager.MulticastLock lock = wm == null ? null : wm.createMulticastLock("robotdog");
            if (lock != null) {
                lock.setReferenceCounted(false);
                lock.acquire();
            }
            try (java.net.DatagramSocket ds = new java.net.DatagramSocket(null)) {
                ds.setReuseAddress(true);
                ds.setBroadcast(true);
                ds.bind(new InetSocketAddress(BEACON_PORT));
                byte[] buf = new byte[128];
                while (true) {
                    java.net.DatagramPacket p = new java.net.DatagramPacket(buf, buf.length);
                    ds.receive(p);
                    String msg = new String(p.getData(), 0, p.getLength(), StandardCharsets.US_ASCII);
                    if (!msg.startsWith("ROBOTDOG")) continue;
                    String ip = p.getAddress().getHostAddress();
                    if (!ip.equals(beaconHost)) Log.i(TAG, "dog on hotspot at " + ip);
                    beaconHost = ip;
                    beaconAt = System.currentTimeMillis();
                }
            } catch (Exception e) {
                Log.w(TAG, "beacon: " + e.getMessage());
            } finally {
                if (lock != null) lock.release();
            }
        }, "dog-beacon");
        t.setDaemon(true);
        t.start();
    }

    static void onTtsReady(boolean ok) { ttsReady = ok; }

    static Reply think(String text) {
        Reply local = localRules(text);
        if (local.handshake) return local;
        try {
            Reply ai = askCloud(text);
            if (ai.say == null || ai.say.trim().isEmpty()) {
                ai.say = local.say.isEmpty() ? "Mình đang nghe nè." : local.say;
            }
            if (ai.actions.length == 0) ai.actions = local.actions;
            if ((ai.music == null || ai.music.isEmpty()) && local.music != null) ai.music = local.music;
            remember(text, ai.say);
            return ai;
        } catch (Exception e) {
            Log.e(TAG, "askCloud", e);
            if (local.say.isEmpty()) {
                local.say = "Mình chưa ra mạng được. Bạn bật dữ liệu di động giúp mình nhé.";
            }
            return local;
        }
    }

    static String hear(byte[] wav) throws Exception {
        Exception last = null;
        for (String key : new String[]{ApiKeys.GROQ_A, ApiKeys.GROQ_B}) {
            try {
                return whisper(wav, key);
            } catch (Exception e) {
                last = e;
            }
        }
        throw last == null ? new Exception("whisper") : last;
    }

    static void speakOnDog(String text) throws Exception {
        String line = text == null ? "" : text.trim();
        if (line.isEmpty()) return;
        final int my = epoch;
        if (line.length() > 700) {
            int cut = line.lastIndexOf('.', 700);
            if (cut < 180) cut = 700;
            line = line.substring(0, Math.min(line.length(), cut + 1));
        }
        byte[] pcm;
        try {
            pcm = geminiTts(line);
        } catch (Exception cloud) {
            if (my != epoch) return;
            pcm = androidTts(line);
        }
        if (my != epoch) return;
        playPcm(pcm);
    }

    static final class Track {
        final String url;
        final String title;
        final boolean live;

        Track(String url, String title, boolean live) {
            this.url = url;
            this.title = title;
            this.live = live;
        }
    }

    /** "song: tên bài" → iTunes/Deezer; "radio: thể loại" → đài phát trực tiếp; Archive.org là đường cuối. */
    static Future<Track> findMusic(String query) {
        String q = query == null ? "" : query.trim();
        String low = q.toLowerCase(Locale.ROOT);
        final boolean radio = low.startsWith("radio:");
        final boolean song = low.startsWith("song:");
        if (radio || song) q = q.substring(q.indexOf(':') + 1).trim();
        final String term = q.isEmpty() ? "lofi" : q;
        Log.i(TAG, "music query: " + query);
        return SEARCH.submit(() -> {
            if (radio) {
                try { return findRadio(term); } catch (Exception e) { Log.w(TAG, "radio: " + e.getMessage()); }
            }
            try { return findItunes(term); } catch (Exception e) { Log.w(TAG, "itunes: " + e.getMessage()); }
            try { return findDeezer(term); } catch (Exception e) { Log.w(TAG, "deezer: " + e.getMessage()); }
            if (!radio) {
                try { return findRadio(term); } catch (Exception e) { Log.w(TAG, "radio: " + e.getMessage()); }
            }
            return new Track(findArchive(term), term, false);
        });
    }

    private static boolean variant(String name) {
        String n = name.toLowerCase(Locale.ROOT);
        return n.contains("remix") || n.contains("live") || n.contains("tour") || n.contains("concert")
                || n.contains("karaoke") || n.contains("instrumental") || n.contains("beat");
    }

    private static String fold(String s) {
        return java.text.Normalizer.normalize(s, java.text.Normalizer.Form.NFD)
                .replaceAll("\\p{M}", "").replace('đ', 'd').replace('Đ', 'D')
                .toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9 ]", " ");
    }

    /** Số từ của câu tìm có mặt trong tên bài + ca sĩ; bản remix/live bị trừ nhẹ. */
    private static int score(String[] words, JSONObject t) {
        String hay = " " + fold(t.optString("trackName") + " " + t.optString("artistName")).replaceAll("\\s+", " ") + " ";
        int s = 0;
        for (String w : words) if (!w.isEmpty() && hay.contains(" " + w + " ")) s += 2;
        if (variant(t.optString("trackName"))) s -= 1;
        return s;
    }

    private static Track findItunes(String term) throws Exception {
        String[] words = fold(term).trim().split("\\s+");
        JSONObject best = null;
        int bestScore = Integer.MIN_VALUE;
        for (String country : new String[]{"VN", "US"}) {
            JSONArray res;
            try {
                res = new JSONObject(get("https://itunes.apple.com/search?entity=song&limit=10&country=" + country
                        + "&term=" + URLEncoder.encode(term, "UTF-8"), 8000)).getJSONArray("results");
            } catch (Exception e) {
                if (offline(e)) throw e;
                continue;
            }
            for (int i = 0; i < res.length(); i++) {
                JSONObject t = res.getJSONObject(i);
                if (t.optString("previewUrl").isEmpty()) continue;
                int s = score(words, t);
                if (s > bestScore) { best = t; bestScore = s; }
            }
            if (best != null && bestScore >= words.length * 2) break;
        }
        if (best == null || bestScore <= 0) throw new Exception("no match for " + term);
        return new Track(best.getString("previewUrl"),
                best.optString("trackName") + " - " + best.optString("artistName"), false);
    }

    private static Track findDeezer(String term) throws Exception {
        String plain = java.text.Normalizer.normalize(term, java.text.Normalizer.Form.NFD)
                .replaceAll("\\p{M}", "").replace('đ', 'd').replace('Đ', 'D');
        JSONArray res = new JSONObject(get("https://api.deezer.com/search?limit=5&q="
                + URLEncoder.encode(plain, "UTF-8"), 8000)).getJSONArray("data");
        for (int i = 0; i < res.length(); i++) {
            JSONObject t = res.getJSONObject(i);
            String url = t.optString("preview");
            if (url.isEmpty()) continue;
            JSONObject artist = t.optJSONObject("artist");
            return new Track(url, t.optString("title") + (artist == null ? "" : " - " + artist.optString("name")), false);
        }
        throw new Exception("none");
    }

    private static Track findRadio(String term) throws Exception {
        String low = term.toLowerCase(Locale.ROOT);
        boolean viet = low.contains("việt") || low.contains("viet") || low.contains("vpop") || low.contains("bolero");
        String query = viet ? "countrycode=VN&tagExclude=news" : "tag=" + URLEncoder.encode(low, "UTF-8");
        JSONArray res = new JSONArray(get("https://all.api.radio-browser.info/json/stations/search?limit=8"
                + "&order=clickcount&reverse=true&hidebroken=true&codec=MP3&" + query, 8000));
        if (res.length() == 0 && !viet) {
            res = new JSONArray(get("https://all.api.radio-browser.info/json/stations/search?limit=8"
                    + "&order=clickcount&reverse=true&hidebroken=true&codec=MP3&name="
                    + URLEncoder.encode(low, "UTF-8"), 8000));
        }
        for (int i = 0; i < res.length(); i++) {
            JSONObject st = res.getJSONObject(i);
            String url = st.optString("url_resolved", st.optString("url"));
            if (!url.isEmpty()) return new Track(url, st.optString("name").trim(), true);
        }
        throw new Exception("none");
    }

    static void playMusic(Future<Track> found) throws Exception {
        final int my = epoch;
        long t0 = System.currentTimeMillis();
        Track track = found.get(40, TimeUnit.SECONDS);
        Log.i(TAG, "music found after " + (System.currentTimeMillis() - t0) + "ms: " + track.title + " " + track.url);
        cancel = false;
        if (my != epoch) return;
        HttpURLConnection conn = internet(track.url, 15000);
        conn.setRequestProperty("User-Agent", "RobotDog/1.0");
        int code = conn.getResponseCode();
        if (code < 200 || code >= 300) {
            conn.disconnect();
            throw new Exception("dl " + code);
        }
        long length = conn.getContentLengthLong();
        StreamSource src = new StreamSource(track.live || length > 3_000_000 ? -1 : length);
        Thread pull = new Thread(() -> {
            try (InputStream in = conn.getInputStream()) {
                byte[] buf = new byte[16384];
                int n;
                while (!cancel && !src.closed && src.length() < STREAM_MAX && (n = in.read(buf)) > 0) {
                    src.add(buf, n);
                }
            } catch (Exception e) {
                Log.w(TAG, "music pull: " + e.getMessage());
            } finally {
                src.finish();
                conn.disconnect();
            }
        }, "music-pull");
        pull.start();
        Socket s = openDog();
        synchronized (AUDIO) { audioSock = s; }
        paceReset();
        try {
            s.getOutputStream().write(new byte[]{'P', 'C', 'M', '1'});
            decodeToSocket(src, s, t0);
            Log.i(TAG, "music decoded " + (paceBytes / 32000) + "s, downloaded " + src.length() + "B");
            drain();
        } finally {
            src.close();
            synchronized (AUDIO) {
                try { s.close(); } catch (Exception ignored) {}
                if (audioSock == s) audioSock = null;
            }
        }
    }

    private static final int MUSIC_MAX = 15_000_000;
    private static final int STREAM_MAX = 30_000_000;
    private static final long PLAY_SECONDS = 600;
    private static final ExecutorService SEARCH = Executors.newSingleThreadExecutor();

    /** Nhạc đang tải dở; MediaExtractor đọc tới đâu thì chờ tải tới đó. */
    private static final class StreamSource extends MediaDataSource {
        private final long size;
        private byte[] data = new byte[512 * 1024];
        private int len;
        private boolean done;
        volatile boolean closed;

        StreamSource(long size) { this.size = size; }

        synchronized int length() { return len; }

        synchronized void add(byte[] b, int n) {
            if (len + n > data.length) data = Arrays.copyOf(data, Math.max(data.length * 2, len + n));
            System.arraycopy(b, 0, data, len, n);
            len += n;
            notifyAll();
        }

        synchronized void finish() { done = true; notifyAll(); }

        @Override
        public synchronized int readAt(long position, byte[] buffer, int offset, int size) throws IOException {
            while (!done && !closed && !cancel && position + size > len) {
                try { wait(200); } catch (InterruptedException e) { throw new IOException(e); }
            }
            if (closed || position >= len) return -1;
            int n = (int) Math.min(size, len - position);
            System.arraycopy(data, (int) position, buffer, offset, n);
            return n;
        }

        @Override public long getSize() { return size; }

        @Override public synchronized void close() { closed = true; notifyAll(); }
    }

    private static volatile int epoch;

    static int epoch() { return epoch; }

    /** Nút dừng: bỏ mọi câu trả lời / bản nhạc đang làm dở, kể cả cái chưa kịp phát. */
    static void hush() {
        epoch++;
        stopAudio();
    }

    static void stopAudio() {
        cancel = true;
        synchronized (AUDIO) {
            if (audioSock != null) {
                try { audioSock.close(); } catch (Exception ignored) {}
                audioSock = null;
            }
        }
    }

    static byte[] wav(byte[] pcm, int rate) {
        int data = pcm.length;
        ByteBuffer b = ByteBuffer.allocate(44 + data).order(ByteOrder.LITTLE_ENDIAN);
        b.put(new byte[]{'R', 'I', 'F', 'F'});
        b.putInt(36 + data);
        b.put(new byte[]{'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
        b.putInt(16);
        b.putShort((short) 1);
        b.putShort((short) 1);
        b.putInt(rate);
        b.putInt(rate * 2);
        b.putShort((short) 2);
        b.putShort((short) 16);
        b.put(new byte[]{'d', 'a', 't', 'a'});
        b.putInt(data);
        // putInt writes big... wait ByteBuffer default order was set LITTLE so putInt is LE.
        // But the RIFF chunk size and fmt fields are correct in LE.
        // I already put the header with LE. PCM follows.
        b.put(pcm);
        return b.array();
    }

    private static void remember(String user, String dog) {
        memUser.add(user);
        memDog.add(dog == null ? "" : dog);
        while (memUser.size() > 8) {
            memUser.remove(0);
            memDog.remove(0);
        }
    }

    private static boolean isQuestion(String raw) {
        String t = raw.trim();
        return t.contains("?") || t.contains("là gì") || t.contains("la gi")
                || t.startsWith("tại sao") || t.startsWith("tai sao") || t.startsWith("vì sao")
                || t.startsWith("thế nào") || t.startsWith("the nao") || t.contains("như thế nào")
                || t.contains("bao nhiêu") || t.contains("ở đâu") || t.contains("khi nào")
                || t.startsWith("ai ") || t.contains("có phải") || t.contains("giải thích")
                || t.contains("cho mình biết") || t.contains("kể");
    }

    private static boolean commandTone(String raw) {
        if (isQuestion(raw)) return false;
        String t = raw.trim();
        if (t.length() <= 36) return true;
        return t.startsWith("hãy") || t.startsWith("làm ơn") || t.startsWith("giúp")
                || t.startsWith("đi") || t.startsWith("chạy") || t.startsWith("đứng")
                || t.startsWith("ngồi") || t.startsWith("dừng") || t.startsWith("dậy");
    }

    private static Reply localRules(String text) {
        String raw = text.toLowerCase(Locale.ROOT);
        Reply r = new Reply();
        ArrayList<String> acts = new ArrayList<>();
        boolean command = commandTone(raw);
        boolean askedShake = raw.contains("bắt tay") || raw.contains("bat tay") || raw.contains("đưa tay")
                || raw.contains("giơ tay") || raw.contains("shake");
        if (askedShake) {
            boolean left = raw.contains("trái") || raw.contains("trai") || raw.contains("left");
            boolean right = raw.contains("phải") || raw.contains("phai") || raw.contains("right");
            if (left == right) left = Math.random() < 0.5;
            r.handshake = true;
            r.say = left ? "Tay trái nè, bắt tay nào." : "Tay phải nè, bắt tay nào.";
            r.actions = new String[]{left ? "shakel" : "shaker"};
            return r;
        }
        if (command && (raw.contains("dừng") || raw.contains("dung lai") || raw.contains("đứng lại")
                || raw.contains("stop"))) {
            acts.add("stop");
            r.say = "Dừng lại nè.";
        }
        if (command && (raw.contains("đứng dậy") || raw.contains("dậy") || raw.contains("đứng lên"))) {
            acts.add("getup");
            r.say = "Dậy nha.";
        } else if (command && !raw.contains("đứng lại")
                && (raw.contains("đứng") || raw.equals("dung") || raw.contains("đứng yên"))) {
            acts.add("stand");
            r.say = "Mình đứng yên.";
        }
        if (command && (raw.contains("chạy") || raw.contains("chay"))) {
            acts.add("run");
            r.say = "Chạy đây.";
        }
        if (command && (raw.contains("đi bộ") || raw.contains("đi nào") || raw.contains("đi đi")
                || raw.contains("đi thôi") || raw.contains("hãy đi") || raw.equals("đi"))) {
            acts.add("walk");
            r.say = "Mình đi đây.";
        }
        if (command && (raw.contains("ngồi 2") || raw.contains("ngồi hai") || raw.contains("hai chân")
                || raw.contains("ngoi 2"))) {
            acts.add("sit2");
            r.say = "Ngồi hai chân nè.";
        } else if (command && (raw.contains("ngồi") || raw.contains("ngoi"))) {
            acts.add("sit4");
            r.say = "Mình ngồi xuống.";
        }
        if (raw.contains("phát nhạc") || raw.contains("mở nhạc") || raw.contains("bật nhạc")
                || raw.contains("hát bài") || raw.contains("phat nhac") || raw.contains("mo nhac")) {
            r.music = raw.replace("phát", " ").replace("mở", " ").replace("nhạc", " ")
                    .replace("nhac", " ").replace("hát", " ").replace("bài", " ")
                    .replace("cho tôi", " ").replace("cho mình", " ").replaceAll("\\s+", " ").trim();
            if (r.music.isEmpty()) r.music = "piano";
            if (r.say.isEmpty()) r.say = "Để mình mở nhạc.";
        }
        r.actions = acts.toArray(new String[0]);
        return r;
    }

    private static Reply askCloud(String text) throws Exception {
        String prompt = PERSONA + "\nNgười dùng vừa nói: " + text;
        String body = null;
        Exception last = null;
        String[] gemini = {"gemini-3.5-flash-lite", "gemini-flash-lite-latest", "gemini-3.8-flash", "gemini-flash-latest"};
        for (String model : gemini) {
            try {
                body = geminiText(model, prompt);
                break;
            } catch (Exception e) {
                last = e;
                Log.w(TAG, "gemini " + model + ": " + e.getMessage());
                if (offline(e)) throw e;
            }
        }
        if (body == null) {
            outer:
            for (String model : new String[]{"openai/gpt-oss-120b", "qwen/qwen3.8-27b"}) {
                for (String key : new String[]{ApiKeys.GROQ_A, ApiKeys.GROQ_B}) {
                    try {
                        body = chatCompletions("https://api.groq.com/openai/v1/chat/completions",
                                key, model, prompt, false);
                        break outer;
                    } catch (Exception e) {
                        last = e;
                        Log.w(TAG, "groq " + model + ": " + e.getMessage());
                        if (offline(e)) throw e;
                    }
                }
            }
        }
        if (body == null) {
            for (String model : new String[]{"nvidia/nemotron-3-super-120b-a12b:free", "google/gemma-4-31b-it:free"}) {
                try {
                    body = chatCompletions("https://openrouter.ai/api/v1/chat/completions",
                            ApiKeys.OPENROUTER, model, prompt, true);
                    break;
                } catch (Exception e) { last = e; Log.w(TAG, "openrouter " + model + ": " + e.getMessage()); }
            }
        }
        if (body == null) throw last == null ? new Exception("ai") : last;
        return parseReply(body);
    }

    private static String geminiText(String model, String prompt) throws Exception {
        JSONArray contents = new JSONArray();
        for (int i = 0; i < memUser.size(); i++) {
            contents.put(new JSONObject().put("role", "user").put("parts",
                    new JSONArray().put(new JSONObject().put("text", memUser.get(i)))));
            contents.put(new JSONObject().put("role", "model").put("parts",
                    new JSONArray().put(new JSONObject().put("text", memDog.get(i)))));
        }
        contents.put(new JSONObject().put("role", "user").put("parts",
                new JSONArray().put(new JSONObject().put("text", prompt))));
        JSONObject json = new JSONObject();
        json.put("contents", contents);
        String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                + model + ":generateContent?key=" + ApiKeys.GEMINI;
        String raw = post(url, json.toString(), null, 35000);
        JSONArray parts = new JSONObject(raw).getJSONArray("candidates").getJSONObject(0)
                .getJSONObject("content").getJSONArray("parts");
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < parts.length(); i++) {
            JSONObject p = parts.getJSONObject(i);
            if (!p.optBoolean("thought")) out.append(p.optString("text"));
        }
        if (out.length() == 0) throw new Exception("empty");
        return out.toString();
    }

    private static String chatCompletions(String url, String key, String model, String prompt, boolean openRouter) throws Exception {
        JSONObject json = new JSONObject();
        JSONArray messages = new JSONArray();
        messages.put(new JSONObject().put("role", "system").put("content", PERSONA));
        for (int i = 0; i < memUser.size(); i++) {
            messages.put(new JSONObject().put("role", "user").put("content", memUser.get(i)));
            messages.put(new JSONObject().put("role", "assistant").put("content", memDog.get(i)));
        }
        messages.put(new JSONObject().put("role", "user").put("content", prompt));
        json.put("model", model);
        json.put("messages", messages);
        HttpURLConnection conn = internet(url, 20000);
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Authorization", "Bearer " + key);
        conn.setRequestProperty("Content-Type", "application/json");
        if (openRouter) conn.setRequestProperty("HTTP-Referer", "https://localhost");
        conn.setDoOutput(true);
        try (OutputStream os = conn.getOutputStream()) {
            os.write(json.toString().getBytes(StandardCharsets.UTF_8));
        }
        int code = conn.getResponseCode();
        String raw = read(code >= 400 ? conn.getErrorStream() : conn.getInputStream());
        conn.disconnect();
        if (code < 200 || code >= 300) throw new Exception("chat " + code);
        return new JSONObject(raw).getJSONArray("choices").getJSONObject(0)
                .getJSONObject("message").getString("content");
    }

    private static Reply parseReply(String body) {
        Reply r = new Reply();
        try {
            String t = body.trim();
            int a = t.indexOf('{');
            int b = t.lastIndexOf('}');
            if (a >= 0 && b > a) t = t.substring(a, b + 1);
            JSONObject o = new JSONObject(t);
            r.say = o.optString("say", "").trim();
            String music = o.optString("music", "").trim();
            if (!music.isEmpty() && !music.equals("null")) r.music = music;
            JSONArray arr = o.optJSONArray("actions");
            ArrayList<String> acts = new ArrayList<>();
            if (arr != null) {
                for (int i = 0; i < arr.length(); i++) {
                    String n = normAction(arr.optString(i));
                    if (n != null && !acts.contains(n)) acts.add(n);
                }
            }
            r.actions = acts.toArray(new String[0]);
        } catch (Exception e) {
            r.say = body.replace("\n", " ").trim();
            if (r.say.length() > 160) r.say = r.say.substring(0, 160);
        }
        return r;
    }

    private static String normAction(String s) {
        if (s == null) return null;
        String x = s.toLowerCase(Locale.ROOT).trim();
        if (x.equals("walk") || x.equals("di") || x.equals("đi")) return "walk";
        if (x.equals("run") || x.equals("chay") || x.equals("chạy")) return "run";
        if (x.equals("stand") || x.equals("dung") || x.equals("đứng")) return "stand";
        if (x.equals("stop") || x.equals("dừng")) return "stop";
        if (x.equals("sit4") || x.equals("sit") || x.equals("ngồi")) return "sit4";
        if (x.equals("sit2")) return "sit2";
        if (x.equals("getup") || x.equals("dậy")) return "getup";
        if (x.equals("beep")) return "beep";
        if (x.equals("shakel")) return "shakel";
        if (x.equals("shaker")) return "shaker";
        if (x.equals("shake")) return Math.random() < 0.5 ? "shakel" : "shaker";
        return null;
    }

    private static byte[] geminiTts(String text) throws Exception {
        String[] models = {"gemini-3.8-flash-lite-tts", "gemini-3.8-flash-tts",
                "gemini-3.1-flash-tts-preview", "gemini-2.5-flash-preview-tts"};
        Exception last = null;
        for (String model : models) {
            try {
                JSONObject speech = new JSONObject()
                        .put("voiceConfig", new JSONObject()
                                .put("prebuiltVoiceConfig", new JSONObject().put("voiceName", "Kore")));
                JSONObject gen = new JSONObject()
                        .put("responseModalities", new JSONArray().put("AUDIO"))
                        .put("speechConfig", speech);
                JSONObject json = new JSONObject();
                json.put("contents", new JSONArray().put(new JSONObject()
                        .put("parts", new JSONArray().put(new JSONObject().put("text", text)))));
                json.put("generationConfig", gen);
                String url = "https://generativelanguage.googleapis.com/v1beta/models/"
                        + model + ":generateContent?key=" + ApiKeys.GEMINI;
                String raw = post(url, json.toString(), null, 25000);
                JSONArray parts = new JSONObject(raw).getJSONArray("candidates").getJSONObject(0)
                        .getJSONObject("content").getJSONArray("parts");
                for (int i = 0; i < parts.length(); i++) {
                    JSONObject inline = parts.getJSONObject(i).optJSONObject("inlineData");
                    if (inline == null) continue;
                    byte[] data = android.util.Base64.decode(inline.getString("data"), android.util.Base64.DEFAULT);
                    String mime = inline.optString("mimeType", "");
                    int rate = 24000;
                    int at = mime.indexOf("rate=");
                    if (at >= 0) {
                        int j = at + 5;
                        int k = j;
                        while (k < mime.length() && Character.isDigit(mime.charAt(k))) k++;
                        rate = Integer.parseInt(mime.substring(j, k));
                    }
                    if (mime.contains("wav")) return wavPcm(data);
                    return to16kMono(data, rate, 1);
                }
                throw new Exception("no audio");
            } catch (Exception e) {
                last = e;
                Log.w(TAG, "tts " + model + ": " + e.getMessage());
                if (offline(e)) throw e;
            }
        }
        throw last == null ? new Exception("tts") : last;
    }

    private static byte[] androidTts(String text) throws Exception {
        if (tts == null || !ttsReady || app == null) throw new Exception("no local tts");
        File f = new File(app.getCacheDir(), "dog-tts.wav");
        CountDownLatch latch = new CountDownLatch(1);
        AtomicBoolean ok = new AtomicBoolean(false);
        MAIN.post(() -> {
            tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                @Override public void onStart(String utteranceId) {}
                @Override public void onDone(String utteranceId) { ok.set(true); latch.countDown(); }
                @Override public void onError(String utteranceId) { latch.countDown(); }
            });
            Bundle bundle = new Bundle();
            int r = tts.synthesizeToFile(text, bundle, f, "dog");
            if (r != TextToSpeech.SUCCESS) latch.countDown();
        });
        if (!latch.await(12, TimeUnit.SECONDS) || !ok.get()) throw new Exception("tts file");
        byte[] wav = readFile(f);
        return wavPcm(wav);
    }

    private static String whisper(byte[] wav, String key) throws Exception {
        String boundary = "----dogboundary";
        HttpURLConnection conn = internet("https://api.groq.com/openai/v1/audio/transcriptions", 25000);
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Authorization", "Bearer " + key);
        conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
        conn.setDoOutput(true);
        ByteArrayOutputStream body = new ByteArrayOutputStream();
        String head = "--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\n"
                + "Content-Type: audio/wav\r\n\r\n";
        body.write(head.getBytes(StandardCharsets.UTF_8));
        body.write(wav);
        String mid = "\r\n--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-large-v3-turbo"
                + "\r\n--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"language\"\r\n\r\nvi"
                + "\r\n--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n0"
                + "\r\n--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"response_format\"\r\n\r\nverbose_json"
                + "\r\n--" + boundary + "\r\n"
                + "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n"
                + "Người dùng nói chuyện với chú chó robot: xin chào, bắt tay, ngồi xuống, bật nhạc."
                + "\r\n--" + boundary + "--\r\n";
        body.write(mid.getBytes(StandardCharsets.UTF_8));
        try (OutputStream os = conn.getOutputStream()) { os.write(body.toByteArray()); }
        int code = conn.getResponseCode();
        String raw = read(code >= 400 ? conn.getErrorStream() : conn.getInputStream());
        conn.disconnect();
        if (code < 200 || code >= 300) throw new Exception("stt");
        JSONObject o = new JSONObject(raw);
        JSONArray segs = o.optJSONArray("segments");
        String text;
        if (segs == null) {
            text = o.optString("text", "");
        } else {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < segs.length(); i++) {
                JSONObject s = segs.getJSONObject(i);
                if (s.optDouble("no_speech_prob", 0) > 0.6) continue;
                sb.append(s.optString("text", "")).append(' ');
            }
            text = sb.toString();
        }
        text = HALLUCINATION.matcher(text).replaceAll(" ").replaceAll("\\s+", " ").trim();
        Log.i(TAG, "heard: " + text);
        return text;
    }

    // Whisper học từ video YouTube tiếng Việt nên gặp im lặng hay bịa câu kết video.
    private static final java.util.regex.Pattern HALLUCINATION = java.util.regex.Pattern.compile(
            "(?iu)[^.!?]*(ghiền mì gõ|mì gõ|subscribe|đăng ký kênh|đăng kí kênh|like và share|"
                    + "lalaschool|la la school|theo dõi kênh|bỏ lỡ những video|video hấp dẫn|"
                    + "hẹn gặp lại các bạn|cảm ơn các bạn đã (theo dõi|xem)|nhấn chuông)[^.!?]*[.!?]?");

    // Cắt khoảng lặng đầu/cuối bản ghi (PCM 16 bit mono) để Whisper không bịa chữ vào chỗ im.
    static byte[] trimSilence(byte[] pcm, int rate) {
        final int frame = rate / 50 * 2;
        final int frames = pcm.length / frame;
        if (frames == 0) return new byte[0];
        double[] rms = new double[frames];
        for (int f = 0; f < frames; f++) {
            double sum = 0;
            for (int i = f * frame; i + 1 < (f + 1) * frame; i += 2) {
                int v = (short) ((pcm[i] & 0xff) | (pcm[i + 1] << 8));
                sum += (double) v * v;
            }
            rms[f] = Math.sqrt(sum / (frame / 2.0));
        }
        double[] sorted = rms.clone();
        java.util.Arrays.sort(sorted);
        double thr = Math.max(300, sorted[frames / 5] * 3);
        int first = -1, last = -1;
        for (int f = 0; f < frames; f++) {
            if (rms[f] > thr) {
                if (first < 0) first = f;
                last = f;
            }
        }
        if (first < 0) return new byte[0];
        first = Math.max(0, first - 15);
        last = Math.min(frames - 1, last + 20);
        return java.util.Arrays.copyOfRange(pcm, first * frame, (last + 1) * frame);
    }

    private static String findArchive(String query) throws Exception {
        String search = "https://archive.org/advancedsearch.php?q="
                + URLEncoder.encode("(" + query + ") AND mediatype:audio AND format:\"VBR MP3\"", "UTF-8")
                + "&fl[]=identifier&sort[]=" + URLEncoder.encode("downloads desc", "UTF-8")
                + "&rows=6&output=json";
        JSONArray docs = new JSONObject(get(search)).getJSONObject("response").getJSONArray("docs");
        for (int i = 0; i < docs.length(); i++) {
            String id = docs.getJSONObject(i).optString("identifier", "");
            if (id.isEmpty()) continue;
            JSONArray files;
            try {
                files = new JSONObject(get("https://archive.org/metadata/" + URLEncoder.encode(id, "UTF-8") + "/files", 8000))
                        .optJSONArray("result");
            } catch (Exception slow) {
                Log.w(TAG, "archive meta " + id + ": " + slow.getMessage());
                continue;
            }
            if (files == null) continue;
            for (int j = 0; j < files.length(); j++) {
                JSONObject f = files.getJSONObject(j);
                String name = f.optString("name", "");
                String format = f.optString("format", "").toLowerCase(Locale.ROOT);
                long size = 0;
                try { size = Long.parseLong(f.optString("size", "0")); } catch (Exception ignored) {}
                if (!name.toLowerCase(Locale.ROOT).endsWith(".mp3")) continue;
                if (!format.contains("mp3") && !format.contains("mpeg") && !name.endsWith(".mp3")) continue;
                if (size > MUSIC_MAX || (size > 0 && size < 100_000)) continue;
                return "https://archive.org/download/" + urlPath(id) + "/" + urlPath(name);
            }
        }
        throw new Exception("nomusic");
    }

    private static void decodeToSocket(MediaDataSource source, Socket sock, long t0) throws Exception {
        MediaExtractor ex = new MediaExtractor();
        ex.setDataSource(source);
        int track = -1;
        String mime = null;
        for (int i = 0; i < ex.getTrackCount(); i++) {
            String m = ex.getTrackFormat(i).getString(MediaFormat.KEY_MIME);
            if (m != null && m.startsWith("audio/")) { track = i; mime = m; break; }
        }
        if (track < 0) throw new Exception("noaudio");
        ex.selectTrack(track);
        MediaFormat fmt = ex.getTrackFormat(track);
        MediaCodec codec = MediaCodec.createDecoderByType(mime);
        codec.configure(fmt, null, null, 0);
        codec.start();
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        boolean inDone = false;
        int rate = fmt.containsKey(MediaFormat.KEY_SAMPLE_RATE) ? fmt.getInteger(MediaFormat.KEY_SAMPLE_RATE) : 44100;
        int ch = fmt.containsKey(MediaFormat.KEY_CHANNEL_COUNT) ? fmt.getInteger(MediaFormat.KEY_CHANNEL_COUNT) : 2;
        long sent = 0;
        double pos = 0;
        int idle = 0;
        try {
            while (!cancel && sent < 16000L * PLAY_SECONDS) {
                if (!inDone) {
                    int in = codec.dequeueInputBuffer(4000);
                    if (in >= 0) {
                        ByteBuffer buf = codec.getInputBuffer(in);
                        int n = buf == null ? -1 : ex.readSampleData(buf, 0);
                        if (n < 0) {
                            codec.queueInputBuffer(in, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                            inDone = true;
                        } else {
                            codec.queueInputBuffer(in, 0, n, ex.getSampleTime(), 0);
                            ex.advance();
                        }
                    }
                }
                int out = codec.dequeueOutputBuffer(info, 4000);
                if (out == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    MediaFormat of = codec.getOutputFormat();
                    if (of.containsKey(MediaFormat.KEY_SAMPLE_RATE)) rate = of.getInteger(MediaFormat.KEY_SAMPLE_RATE);
                    if (of.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) ch = of.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
                } else if (out >= 0) {
                    ByteBuffer buf = codec.getOutputBuffer(out);
                    if (buf != null && info.size > 0) {
                        byte[] pcm = new byte[info.size];
                        buf.position(info.offset);
                        buf.limit(info.offset + info.size);
                        buf.get(pcm);
                        byte[] piece = resample(pcm, rate, ch, pos);
                        pos = piecePos;
                        if (sent == 0) Log.i(TAG, "music first sound after " + (System.currentTimeMillis() - t0) + "ms");
                        writeAll(sock, piece);
                        sent += piece.length / 2L;
                    }
                    codec.releaseOutputBuffer(out, false);
                    if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) break;
                } else if (inDone && ++idle > 100) {
                    break;
                }
            }
        } finally {
            try { codec.stop(); } catch (Exception ignored) {}
            codec.release();
            ex.release();
        }
    }

    private static double piecePos;

    private static byte[] resample(byte[] pcm, int rate, int channels, double startPos) {
        if (channels < 1) channels = 1;
        int frames = pcm.length / (2 * channels);
        if (frames <= 0) { piecePos = 0; return new byte[0]; }
        double step = Math.max(1.0, rate / 16000.0);
        ByteBuffer in = ByteBuffer.wrap(pcm).order(ByteOrder.LITTLE_ENDIAN);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        double pos = startPos;
        ByteBuffer o = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN);
        while (pos < frames) {
            int i0 = (int) pos;
            int i1 = Math.min(i0 + 1, frames - 1);
            short s0 = mix(in, i0, channels);
            short s1 = mix(in, i1, channels);
            double f = pos - i0;
            int s = (int) (s0 + (s1 - s0) * f);
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            o.clear();
            o.putShort((short) s);
            out.write(o.array(), 0, 2);
            pos += step;
        }
        piecePos = pos - frames;
        return out.toByteArray();
    }

    private static short mix(ByteBuffer in, int frame, int channels) {
        int acc = 0;
        for (int c = 0; c < channels; c++) acc += in.getShort((frame * channels + c) * 2);
        return (short) (acc / channels);
    }

    private static byte[] to16kMono(byte[] pcm, int rate, int channels) {
        piecePos = 0;
        ByteArrayOutputStream all = new ByteArrayOutputStream();
        int hop = 4096 * Math.max(1, channels);
        double pos = 0;
        for (int off = 0; off < pcm.length; ) {
            int n = Math.min(hop, pcm.length - off);
            if ((n % (2 * Math.max(channels, 1))) != 0) n -= n % (2 * Math.max(channels, 1));
            if (n <= 0) break;
            byte[] slice = new byte[n];
            System.arraycopy(pcm, off, slice, 0, n);
            byte[] part = resample(slice, rate, channels, pos);
            pos = piecePos;
            try { all.write(part); } catch (Exception ignored) {}
            off += n;
        }
        return all.toByteArray();
    }

    private static byte[] wavPcm(byte[] wav) {
        if (wav.length < 44) return wav;
        int channels = 1;
        int rate = 16000;
        int dataAt = 44;
        for (int i = 12; i + 8 < wav.length; ) {
            String id = new String(wav, i, 4, StandardCharsets.US_ASCII);
            int size = (wav[i + 4] & 255) | ((wav[i + 5] & 255) << 8)
                    | ((wav[i + 6] & 255) << 16) | ((wav[i + 7] & 255) << 24);
            if ("fmt ".equals(id) && i + 8 + 8 < wav.length) {
                channels = (wav[i + 8 + 2] & 255) | ((wav[i + 8 + 3] & 255) << 8);
                rate = (wav[i + 8 + 4] & 255) | ((wav[i + 8 + 5] & 255) << 8)
                        | ((wav[i + 8 + 6] & 255) << 16) | ((wav[i + 8 + 7] & 255) << 24);
            }
            if ("data".equals(id)) { dataAt = i + 8; break; }
            i += 8 + Math.max(0, size);
        }
        int n = Math.min(wav.length - dataAt, wav.length);
        if (n < 2) return new byte[0];
        byte[] pcm = new byte[n - (n % 2)];
        System.arraycopy(wav, dataAt, pcm, 0, pcm.length);
        if (rate == 16000 && channels == 1) return pcm;
        return to16kMono(pcm, rate, Math.max(1, channels));
    }

    private static void playPcm(byte[] pcm) throws Exception {
        if (pcm == null || pcm.length < 2) throw new Exception("empty");
        cancel = false;
        Socket s = openDog();
        synchronized (AUDIO) { audioSock = s; }
        paceReset();
        try {
            OutputStream os = s.getOutputStream();
            os.write(new byte[]{'P', 'C', 'M', '1'});
            writeAll(s, pcm);
            drain();
        } finally {
            synchronized (AUDIO) {
                try { s.close(); } catch (Exception ignored) {}
                if (audioSock == s) audioSock = null;
            }
        }
    }

    /** Chó đang phát câu trả lời hoặc nhạc do AI mở. */
    static boolean audioBusy() {
        synchronized (AUDIO) { return audioSock != null; }
    }

    static Socket openDogStream() throws Exception { return openDog(); }

    private static Socket openDog() throws Exception {
        boolean hotspot = viaHotspot();
        Socket s = !hotspot && wifi != null ? wifi.getSocketFactory().createSocket() : new Socket();
        s.connect(new InetSocketAddress(host(), PCM_PORT), 4000);
        s.setTcpNoDelay(true);
        s.setSoTimeout(8000);
        return s;
    }

    /** 16 kHz mono 16-bit = 32000 byte/s. Gửi trước tối đa LEAD_MS so với tiếng chó đang phát. */
    private static final int LEAD_MS = 1500;
    private static long paceStart;
    private static long paceBytes;

    private static void paceReset() {
        paceStart = 0;
        paceBytes = 0;
    }

    private static void writeAll(Socket s, byte[] data) throws Exception {
        OutputStream os = s.getOutputStream();
        if (paceStart == 0) paceStart = System.currentTimeMillis();
        int off = 0;
        while (off < data.length) {
            if (cancel) throw new Exception("cancel");
            long ahead = paceBytes * 1000 / 32000 - (System.currentTimeMillis() - paceStart);
            if (ahead > LEAD_MS) {
                Thread.sleep(Math.min(40, ahead - LEAD_MS));
                continue;
            }
            int n = Math.min(1024, data.length - off);
            os.write(data, off, n);
            off += n;
            paceBytes += n;
        }
        os.flush();
    }

    /** Chờ chó phát hết phần đã gửi rồi mới đóng kết nối. */
    private static void drain() throws InterruptedException {
        if (paceStart == 0) return;
        long left = paceBytes * 1000 / 32000 - (System.currentTimeMillis() - paceStart) + 300;
        while (left > 0 && !cancel) {
            Thread.sleep(Math.min(100, left));
            left -= 100;
        }
    }

    private static String post(String url, String json, String bearer, int timeout) throws Exception {
        HttpURLConnection conn = internet(url, timeout);
        conn.setRequestMethod("POST");
        conn.setRequestProperty("Content-Type", "application/json");
        if (bearer != null) conn.setRequestProperty("Authorization", "Bearer " + bearer);
        conn.setDoOutput(true);
        try (OutputStream os = conn.getOutputStream()) {
            os.write(json.getBytes(StandardCharsets.UTF_8));
        }
        int code = conn.getResponseCode();
        String raw = read(code >= 400 ? conn.getErrorStream() : conn.getInputStream());
        conn.disconnect();
        if (code < 200 || code >= 300) throw new Exception("http " + code);
        return raw;
    }

    private static String get(String url) throws Exception {
        return get(url, 20000);
    }

    private static String get(String url, int timeout) throws Exception {
        HttpURLConnection conn = internet(url, timeout);
        conn.setRequestProperty("User-Agent", "RobotDog/1.0");
        int code = conn.getResponseCode();
        String raw = read(code >= 400 ? conn.getErrorStream() : conn.getInputStream());
        conn.disconnect();
        if (code < 200 || code >= 300) throw new Exception("get");
        return raw;
    }

    private static HttpURLConnection internet(String url, int timeout) throws Exception {
        URL u = new URL(url);
        Network net = online();
        HttpURLConnection conn = net != null
                ? (HttpURLConnection) net.openConnection(u)
                : (HttpURLConnection) u.openConnection();
        conn.setConnectTimeout(Math.min(timeout, 8000));
        conn.setReadTimeout(timeout);
        return conn;
    }

    /** Mạng đang thật sự ra được Internet: ưu tiên 4G, không thì WiFi nhà; WiFi robot không bao giờ được chọn. */
    private static Network online() {
        ConnectivityManager cm = app == null ? null
                : (ConnectivityManager) app.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return cell;
        Network c = cell;
        if (validated(cm, c)) return c;
        Network d = cm.getActiveNetwork();
        if (validated(cm, d) && !robotWifi(cm, d)) return d;
        for (Network n : cm.getAllNetworks()) {
            if (validated(cm, n) && !robotWifi(cm, n)) return n;
        }
        return c;
    }

    private static boolean validated(ConnectivityManager cm, Network n) {
        if (n == null) return false;
        NetworkCapabilities nc = cm.getNetworkCapabilities(n);
        return nc != null && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
    }

    private static boolean robotWifi(ConnectivityManager cm, Network n) {
        android.net.LinkProperties lp = cm.getLinkProperties(n);
        if (lp == null) return false;
        for (android.net.LinkAddress a : lp.getLinkAddresses()) {
            if (a.getAddress().getHostAddress().startsWith("192.168.4.")) return true;
        }
        return false;
    }

    static boolean offline(Throwable e) {
        for (Throwable t = e; t != null; t = t.getCause()) {
            if (t instanceof java.net.UnknownHostException || t instanceof java.net.ConnectException
                    || t instanceof java.net.NoRouteToHostException) return true;
            String m = t.getMessage();
            if (t instanceof java.net.SocketTimeoutException && m != null && m.contains("connect")) return true;
        }
        return false;
    }

    private static String read(InputStream in) throws Exception {
        if (in == null) return "";
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
        in.close();
        return bos.toString(StandardCharsets.UTF_8.name());
    }

    private static byte[] readFile(File f) throws Exception {
        try (InputStream in = new java.io.FileInputStream(f)) {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
            return bos.toByteArray();
        }
    }

    private static String urlPath(String s) throws Exception {
        return URLEncoder.encode(s, "UTF-8").replace("+", "%20");
    }
}
