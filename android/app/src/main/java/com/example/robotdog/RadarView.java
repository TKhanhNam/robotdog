package com.example.robotdog;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.SweepGradient;
import android.util.AttributeSet;
import android.view.View;

public class RadarView extends View {
    private final Paint grid = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint sweep = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint blip = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint label = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float cm = -1f;

    public RadarView(Context context, AttributeSet attrs) {
        super(context, attrs);
        grid.setStyle(Paint.Style.STROKE);
        grid.setStrokeWidth(2f);
        grid.setColor(0xFF1F8A66);
        sweep.setStyle(Paint.Style.FILL);
        blip.setColor(0xFFFFC14D);
        label.setColor(0xFF9AA8C7);
        label.setTextSize(36f);
        label.setTextAlign(Paint.Align.CENTER);
    }

    public void setDistance(float centimeters) {
        cm = centimeters;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float w = getWidth();
        float h = getHeight();
        float cx = w / 2f;
        float cy = h * 0.92f;
        float r = Math.min(w, h) * 0.86f;
        canvas.drawColor(0xFF10192B);

        for (int i = 1; i <= 4; i++) {
            grid.setAlpha(40 + i * 30);
            canvas.drawArc(cx - r * i / 4f, cy - r * i / 4f, cx + r * i / 4f, cy + r * i / 4f,
                    180, 180, false, grid);
        }
        canvas.drawLine(cx - r, cy, cx + r, cy, grid);
        canvas.drawLine(cx, cy, cx, cy - r, grid);

        float angle = (System.currentTimeMillis() % 2400) / 2400f * 180f;
        sweep.setShader(new SweepGradient(cx, cy, new int[]{0x001FA971, 0xAA1FA971, 0x001FA971},
                new float[]{0f, 0.12f, 0.28f}));
        canvas.save();
        canvas.rotate(angle + 180f, cx, cy);
        canvas.drawCircle(cx, cy, r, sweep);
        canvas.restore();

        if (cm > 0) {
            float shown = Math.min(cm, 200f);
            float dist = (shown / 200f) * r;
            float bx = cx;
            float by = cy - dist;
            blip.setColor(cm < 18f ? 0xFFFF6B6B : 0xFFFFC14D);
            canvas.drawCircle(bx, by, 14f, blip);
            label.setColor(0xFFF4F7FF);
            canvas.drawText(String.format("%.0f cm", cm), cx, 56f, label);
        } else {
            label.setColor(0xFF9AA8C7);
            canvas.drawText("Chưa có vật cản", cx, 56f, label);
        }
        postInvalidateOnAnimation();
    }
}
