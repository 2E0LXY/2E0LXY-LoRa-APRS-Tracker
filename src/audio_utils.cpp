#include "audio_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include <driver/i2s.h>
#include <math.h>

namespace {
    bool i2sReady = false;
    const uint32_t SAMPLE_RATE = 16000;

    bool ensureI2S() {
        if (i2sReady) return true;
        i2s_config_t cfg = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = true,
        };
        if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
        i2s_pin_config_t pins = {
            .bck_io_num = I2S_BCK,
            .ws_io_num = I2S_WS,
            .data_out_num = I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE,
        };
        if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
        i2sReady = true;
        return true;
    }

    // Writes a single sine-wave tone burst. Blocking (short — a few ms per
    // call via I2S DMA); fine for an occasional notification, not called
    // from any latency-sensitive path (LoRa RX handling happens elsewhere
    // and this only runs after a message is already fully parsed).
    void writeTone(float freqHz, uint32_t durationMs, uint8_t volumePct) {
        if (!ensureI2S()) return;
        uint32_t samples = (SAMPLE_RATE * durationMs) / 1000;
        int16_t amp = (int16_t)(32000.0f * (volumePct / 100.0f));
        const size_t CHUNK = 256;
        int16_t buf[CHUNK];
        size_t written = 0;
        size_t bytesWritten;
        while (written < samples) {
            size_t n = min(CHUNK, samples - written);
            for (size_t i = 0; i < n; i++) {
                float t = (float)(written + i) / SAMPLE_RATE;
                // Fade in/out 5ms to avoid a click at the start/end
                float env = 1.0f;
                float fadeS = 0.005f;
                float elapsed = t;
                float remain = (float)durationMs / 1000.0f - t;
                if (elapsed < fadeS) env = elapsed / fadeS;
                else if (remain < fadeS) env = max(0.0f, remain / fadeS);
                buf[i] = (int16_t)(amp * env * sinf(2.0f * PI * freqHz * t));
            }
            i2s_write(I2S_NUM_0, buf, n * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
            written += n;
        }
    }
}

void Audio_Utils::setup() {
    // Lazy-init on first use (ensureI2S) — avoids reserving the I2S
    // peripheral/DMA buffers at boot for devices that never get a message.
}

void Audio_Utils::playMessageTone() {
    if (!Config.audio.enabled) return;
    uint8_t vol = (uint8_t)constrain(Config.audio.volume, 0, 100);
    if (vol == 0) return;
    // Two-tone "ding-dong" style notification, distinct from LoRa/GPS beeps
    // elsewhere on similar devices.
    writeTone(1568.0f, 120, vol);  // G6
    writeTone(1245.0f, 160, vol);  // D#6
}

void Audio_Utils::loop() {
    // Reserved for future non-blocking playback (e.g. queued alert tones).
    // Current implementation is blocking-per-call, invoked directly from
    // the message-received handler.
}
