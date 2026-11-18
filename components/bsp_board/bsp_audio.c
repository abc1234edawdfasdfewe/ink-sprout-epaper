#include "bsp_audio.h"
#include "bsp_board.h"
#include "bsp_gfx.h"
#include "opus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "BSP_AUDIO";

#define AUDIO_SR            16000
#define MAX_REC_SECONDS     10
#define MAX_REC_SAMPLES     (AUDIO_SR * MAX_REC_SECONDS)

typedef struct {
    uint16_t freq_hz;
    uint16_t dur_ms;
} note_t;

/* Warm Music-Box Melodies */
static const note_t s_mel_rabbit[] = {
    /* 小兔子乖乖 3 5 3 5 | 6 5 3 - | 2 3 2 1 | 2 - - 0 */
    {659,260},{784,260},{659,260},{784,260},{880,260},{784,260},{659,480},
    {587,260},{659,260},{587,260},{523,260},{587,520},{0,180},
    {659,260},{784,260},{880,380},{784,260},{659,260},{587,260},{523,560},{0,300},{0,0}
};
static const note_t s_mel_twinkle[] = {
    {523,280},{523,280},{784,280},{784,280},{880,280},{880,280},{784,520},
    {698,280},{698,280},{659,280},{659,280},{587,280},{587,280},{523,560},{0,300},{0,0}
};
static const note_t s_mel_lullaby[] = {
    {659,320},{659,320},{784,640},{659,320},{659,320},{784,640},
    {659,320},{784,320},{1047,480},{988,320},{880,480},{880,320},{784,640},{0,350},{0,0}
};
static const note_t s_mel_adventure[] = {
    {523,200},{659,200},{784,200},{1047,400},{988,200},{784,200},{880,420},
    {784,200},{659,200},{587,200},{698,380},{659,200},{587,200},{523,480},{0,250},{0,0}
};
static const note_t s_mel_ode[] = {
    {659,260},{659,260},{698,260},{784,260},{784,260},{698,260},{659,260},{587,260},
    {523,260},{523,260},{587,260},{659,260},{659,380},{587,160},{587,480},{0,260},{0,0}
};

typedef enum {
    ACMD_SFX = 0,
    ACMD_NOTE,
    ACMD_MELODY,
    ACMD_WAV,
    ACMD_PLAY_REC,
    ACMD_HANZI_PRON,
    ACMD_STOP,
} audio_cmd_type_t;

typedef struct {
    audio_cmd_type_t type;
    int              arg1;
    bool             flag;
    uint16_t         syls[3];
    char             path[160];
} audio_cmd_t;

static i2s_chan_handle_t s_tx_chan = NULL;
static i2s_chan_handle_t s_rx_chan = NULL;
static esp_codec_dev_handle_t s_play_dev = NULL;
static esp_codec_dev_handle_t s_rec_dev  = NULL;
static QueueHandle_t s_cmd_q = NULL;

static int  s_volume = 70;
static int  s_max_volume = 80;
static volatile bool s_playing = false;
static volatile bool s_paused  = false;
static volatile bool s_recording = false;
static volatile int  s_wav_pct = 0;
static volatile int  s_wav_permille = 0;

static int16_t *s_rec_buf = NULL;
static size_t   s_rec_len = 0;

static void apply_hw_vol(void)
{
    if (!s_play_dev) return;
    int eff = s_volume;
    if (eff > s_max_volume) eff = s_max_volume;
    if (eff <= 0) {
        esp_codec_dev_set_out_mute(s_play_dev, true);
        esp_codec_dev_set_out_vol(s_play_dev, 0);
    } else {
        esp_codec_dev_set_out_mute(s_play_dev, false);
        esp_codec_dev_set_out_vol(s_play_dev, eff);
    }
}

/* Synthesize gentle music-box / marimba note into stereo 16-bit I2S stream */
static void play_tone_blocking_check(uint16_t freq_hz, uint16_t dur_ms, float gain, bool abort_on_cmd)
{
    if (!s_play_dev || s_volume <= 0 || dur_ms == 0) return;
    int total_frames = (AUDIO_SR * dur_ms) / 1000;
    int16_t chunk[256 * 2]; /* 256 stereo frames = 16ms */
    int pos = 0;
    float phase = 0.0f;
    float dphase = (2.0f * 3.14159265f * (float)freq_hz) / (float)AUDIO_SR;

    while (pos < total_frames) {
        if (abort_on_cmd && uxQueueMessagesWaiting(s_cmd_q) > 0) break;
        int n = total_frames - pos;
        if (n > 256) n = 256;
        for (int i = 0; i < n; i++) {
            int16_t sample = 0;
            if (freq_hz > 0) {
                float t = (float)(pos + i) / (float)AUDIO_SR;
                float attack = (pos + i < 96) ? ((float)(pos + i) / 96.0f) : 1.0f;
                float env = attack * expf(-4.5f * t);
                float w = 0.76f * sinf(phase) + 0.20f * sinf(2.0f * phase) + 0.04f * sinf(3.0f * phase);
                phase += dphase;
                if (phase > 6.2831853f) phase -= 6.2831853f;
                float v = w * env * gain * 16000.0f;
                if (v > 32000.0f) v = 32000.0f;
                if (v < -32000.0f) v = -32000.0f;
                sample = (int16_t)v;
            }
            chunk[2 * i]     = sample;
            chunk[2 * i + 1] = sample;
        }
        esp_codec_dev_write(s_play_dev, chunk, n * 4);
        pos += n;
    }
}

static void run_sfx(bsp_sfx_t sfx)
{
    switch (sfx) {
    case SFX_WHEEL_TICK:
        play_tone_blocking_check(880, 18, 0.35f, false);
        break;
    case SFX_CONFIRM:
        play_tone_blocking_check(659, 45, 0.55f, false);
        play_tone_blocking_check(880, 75, 0.55f, false);
        break;
    case SFX_BACK:
        play_tone_blocking_check(659, 45, 0.45f, false);
        play_tone_blocking_check(523, 70, 0.45f, false);
        break;
    case SFX_SUCCESS:
        play_tone_blocking_check(523, 70, 0.65f, false);
        play_tone_blocking_check(659, 70, 0.65f, false);
        play_tone_blocking_check(784, 70, 0.65f, false);
        play_tone_blocking_check(1047, 160, 0.70f, false);
        break;
    case SFX_WRONG:
        play_tone_blocking_check(392, 95, 0.50f, false);
        play_tone_blocking_check(330, 140, 0.50f, false);
        break;
    case SFX_FANFARE:
        play_tone_blocking_check(523, 90, 0.70f, true);
        play_tone_blocking_check(659, 90, 0.70f, true);
        play_tone_blocking_check(784, 90, 0.70f, true);
        play_tone_blocking_check(1047, 180, 0.75f, true);
        play_tone_blocking_check(784, 80, 0.70f, true);
        play_tone_blocking_check(1047, 260, 0.80f, true);
        break;
    case SFX_PAGE_TURN:
        play_tone_blocking_check(740, 22, 0.30f, false);
        play_tone_blocking_check(988, 30, 0.28f, false);
        break;
    case SFX_BUBBLE:
        play_tone_blocking_check(587, 30, 0.45f, false);
        play_tone_blocking_check(880, 40, 0.45f, false);
        break;
    case SFX_BOUNCE:
        play_tone_blocking_check(784, 28, 0.45f, false);
        break;
    }
}

static const note_t *get_melody_table(bsp_melody_t m)
{
    switch (m) {
    case MELODY_RABBIT:    return s_mel_rabbit;
    case MELODY_TWINKLE:   return s_mel_twinkle;
    case MELODY_LULLABY:   return s_mel_lullaby;
    case MELODY_ADVENTURE: return s_mel_adventure;
    case MELODY_ODE_JOY:   return s_mel_ode;
    default:               return NULL;
    }
}

static void stream_wav_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGW(TAG, "Cannot open WAV: %s", path);
        return;
    }
    uint8_t hdr[44];
    if (fread(hdr, 1, 44, fp) < 44 || memcmp(hdr, "RIFF", 4) != 0) {
        fclose(fp);
        return;
    }
    uint16_t channels = (uint16_t)(hdr[22] | (hdr[23] << 8));
    uint32_t wav_sr   = (uint32_t)(hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24));
    uint16_t bits     = (uint16_t)(hdr[34] | (hdr[35] << 8));
    if (channels < 1 || channels > 2 || bits != 16 || wav_sr < 8000 || wav_sr > 48000) {
        ESP_LOGW(TAG, "Unsupported WAV format: ch=%u sr=%lu bits=%u", channels, (unsigned long)wav_sr, bits);
        fclose(fp);
        return;
    }
    /* Seek to 'data' chunk properly in case of metadata headers */
    fseek(fp, 12, SEEK_SET);
    uint32_t data_size = 0;
    while (!feof(fp)) {
        uint8_t chdr[8];
        if (fread(chdr, 1, 8, fp) < 8) break;
        uint32_t csz = (uint32_t)(chdr[4] | (chdr[5] << 8) | (chdr[6] << 16) | (chdr[7] << 24));
        if (memcmp(chdr, "data", 4) == 0) {
            data_size = csz;
            break;
        }
        fseek(fp, csz, SEEK_CUR);
    }
    if (data_size == 0) { fclose(fp); return; }

    s_playing = true;
    s_paused = false;
    s_wav_pct = 0;
    s_wav_permille = 0;

    int16_t in_buf[512];
    int16_t out_stereo[512 * 2];
    uint32_t bytes_done = 0;
    float ratio = (float)wav_sr / (float)AUDIO_SR;
    float src_pos = 0.0f;

    while (bytes_done < data_size) {
        if (uxQueueMessagesWaiting(s_cmd_q) > 0) break;
        if (s_paused) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        size_t rd = fread(in_buf, 1, sizeof(in_buf), fp);
        if (rd == 0) break;
        bytes_done += (uint32_t)rd;
        s_wav_pct = (int)((uint64_t)bytes_done * 100ULL / data_size);
        s_wav_permille = (int)((uint64_t)bytes_done * 1000ULL / data_size);

        int in_frames = (int)(rd / (2 * channels));
        int out_frames = 0;
        src_pos = 0.0f;
        while ((int)src_pos < in_frames && out_frames < 512) {
            int idx = (int)src_pos;
            int16_t L = (channels == 2) ? in_buf[2 * idx]     : in_buf[idx];
            int16_t R = (channels == 2) ? in_buf[2 * idx + 1] : in_buf[idx];
            out_stereo[2 * out_frames]     = L;
            out_stereo[2 * out_frames + 1] = R;
            out_frames++;
            src_pos += ratio;
        }
        if (out_frames > 0 && s_play_dev) {
            esp_codec_dev_write(s_play_dev, out_stereo, out_frames * 4);
        }
    }
    fclose(fp);
    s_playing = false;
    s_paused = false;
    s_wav_pct = 100;
    s_wav_permille = 1000;
}

static void play_recorded_fx(bsp_voice_fx_t fx)
{
    if (!s_rec_buf || s_rec_len == 0 || !s_play_dev) return;
    float step = (fx == VOICE_CHIPMUNK) ? 1.42f : (fx == VOICE_BEAR ? 0.72f : 1.0f);
    s_playing = true;
    float pos = 0.0f;
    int16_t out_stereo[256 * 2];
    while ((size_t)pos < s_rec_len) {
        if (uxQueueMessagesWaiting(s_cmd_q) > 0) break;
        int n = 0;
        while (n < 256 && (size_t)pos < s_rec_len) {
            int16_t v = s_rec_buf[(size_t)pos];
            /* Boost recorded voice cleanly with soft limiter */
            int32_t boosted = (int32_t)v * 3;
            if (boosted > 32000) boosted = 32000;
            if (boosted < -32000) boosted = -32000;
            out_stereo[2 * n]     = (int16_t)boosted;
            out_stereo[2 * n + 1] = (int16_t)boosted;
            n++;
            pos += step;
        }
        esp_codec_dev_write(s_play_dev, out_stereo, n * 4);
    }
    s_playing = false;
}

static OpusDecoder *s_hz_opus = NULL;

static void play_hanzi_syllables(const uint16_t syls[3])
{
    if (!s_play_dev || s_volume <= 0) return;
    if (!s_hz_opus) {
        int err = 0;
        s_hz_opus = opus_decoder_create(AUDIO_SR, 1, &err);
        if (!s_hz_opus || err != OPUS_OK) return;
    }

    s_playing = true;
    opus_int16 mono[320];
    int16_t    stereo[320 * 2];

    for (int k = 0; k < 3; k++) {
        if (uxQueueMessagesWaiting(s_cmd_q) > 0) break;
        if (syls[k] == 0xFFFF) continue;

        const uint8_t *clip = NULL;
        uint16_t clip_len = 0;
        if (!gfx_hanzi_get_opus_clip(syls[k], &clip, &clip_len) || !clip || clip_len < 2) {
            continue;
        }

        /* Natural pause between single character reading and 2-char word compound */
        if (k == 1) {
            play_tone_blocking_check(0, 175, 0.0f, true);
        } else if (k == 2) {
            play_tone_blocking_check(0, 25, 0.0f, true);
        }

        opus_decoder_ctl(s_hz_opus, OPUS_RESET_STATE);
        size_t pos = 0;
        while (pos < clip_len) {
            if (uxQueueMessagesWaiting(s_cmd_q) > 0) break;
            uint8_t pkt_len = clip[pos++];
            if (pkt_len == 0 || pos + pkt_len > clip_len) break;
            int n = opus_decode(s_hz_opus, clip + pos, (opus_int32)pkt_len, mono, 320, 0);
            pos += pkt_len;
            if (n > 0) {
                for (int i = 0; i < n; i++) {
                    stereo[2 * i]     = mono[i];
                    stereo[2 * i + 1] = mono[i];
                }
                esp_codec_dev_write(s_play_dev, stereo, n * 4);
            }
        }
    }
    s_playing = false;
}

static void audio_worker_task(void *arg)
{
    audio_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_cmd_q, &cmd, portMAX_DELAY) != pdTRUE) continue;
        if (s_recording) continue;
        apply_hw_vol();

        if (cmd.type == ACMD_SFX) {
            run_sfx((bsp_sfx_t)cmd.arg1);
        } else if (cmd.type == ACMD_NOTE) {
            uint16_t f = (uint16_t)(cmd.arg1 & 0xFFFF);
            uint16_t d = (uint16_t)((cmd.arg1 >> 16) & 0xFFFF);
            play_tone_blocking_check(f, d, 0.72f, true);
        } else if (cmd.type == ACMD_MELODY) {
            const note_t *mel = get_melody_table((bsp_melody_t)cmd.arg1);
            if (mel) {
                s_playing = true;
                do {
                    for (int i = 0; mel[i].dur_ms > 0; i++) {
                        if (uxQueueMessagesWaiting(s_cmd_q) > 0) break;
                        play_tone_blocking_check(mel[i].freq_hz, mel[i].dur_ms, 0.55f, true);
                    }
                } while (cmd.flag && uxQueueMessagesWaiting(s_cmd_q) == 0);
                s_playing = false;
            }
        } else if (cmd.type == ACMD_WAV) {
            stream_wav_file(cmd.path);
        } else if (cmd.type == ACMD_PLAY_REC) {
            play_recorded_fx((bsp_voice_fx_t)cmd.arg1);
        } else if (cmd.type == ACMD_HANZI_PRON) {
            play_hanzi_syllables(cmd.syls);
        } else if (cmd.type == ACMD_STOP) {
            s_playing = false;
            s_paused = false;
        }
    }
}

void bsp_audio_init(void)
{
    s_cmd_q = xQueueCreate(8, sizeof(audio_cmd_t));
    s_rec_buf = (int16_t *)heap_caps_malloc(MAX_REC_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    if (i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SR),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK_PIN,
            .bclk = BSP_I2S_BCLK_PIN,
            .ws   = BSP_I2S_WS_PIN,
            .dout = BSP_I2S_DOUT_PIN,
            .din  = BSP_I2S_DIN_PIN,
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    i2s_channel_enable(s_tx_chan);
    i2s_channel_enable(s_rx_chan);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = (void *)bsp_i2c_get_bus(),
    };
    const audio_codec_ctrl_if_t *i2c_ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if  = audio_codec_new_gpio();
    if (!i2c_ctrl || !gpio_if) {
        ESP_LOGW(TAG, "ES8311 control init skipped");
        return;
    }

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if = i2c_ctrl,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_PA_CTRL_PIN,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .hw_gain = { .pa_voltage = 5.0f, .codec_dac_voltage = 3.3f },
    };
    const audio_codec_if_t *es_dev = es8311_codec_new(&es_cfg);
    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = s_rx_chan,
        .tx_handle = s_tx_chan,
    };
    const audio_codec_data_if_t *i2s_data = audio_codec_new_i2s_data(&i2s_data_cfg);
    if (es_dev && i2s_data) {
        esp_codec_dev_cfg_t play_cfg = {
            .codec_if = es_dev,
            .data_if  = i2s_data,
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        };
        s_play_dev = esp_codec_dev_new(&play_cfg);

        esp_codec_dev_cfg_t rec_cfg = {
            .codec_if = es_dev,
            .data_if  = i2s_data,
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
        };
        s_rec_dev = esp_codec_dev_new(&rec_cfg);

        esp_codec_dev_sample_info_t fs_out = { .sample_rate = AUDIO_SR, .channel = 2, .bits_per_sample = 16 };
        esp_codec_dev_sample_info_t fs_in  = { .sample_rate = AUDIO_SR, .channel = 1, .bits_per_sample = 16 };
        if (s_play_dev) esp_codec_dev_open(s_play_dev, &fs_out);
        if (s_rec_dev) {
            esp_codec_dev_open(s_rec_dev, &fs_in);
            esp_codec_dev_set_in_gain(s_rec_dev, 24.0f);
        }
        apply_hw_vol();
        ESP_LOGI(TAG, "ES8311 + NS4150B Audio Engine ready");
    }

    xTaskCreatePinnedToCore(audio_worker_task, "audio_task", 24576, NULL, 4, NULL, 1);
}

void bsp_audio_shutdown(void)
{
    bsp_audio_stop_all();
    vTaskDelay(pdMS_TO_TICKS(60));
    if (s_play_dev) {
        esp_codec_dev_set_out_mute(s_play_dev, true);
        esp_codec_dev_set_out_vol(s_play_dev, 0);
        esp_codec_dev_close(s_play_dev);
    }
    if (s_rec_dev) {
        esp_codec_dev_close(s_rec_dev);
    }
    gpio_set_level(BSP_PA_CTRL_PIN, 0);
}

void bsp_audio_set_volume(int vol_0_100)
{
    if (vol_0_100 < 0) vol_0_100 = 0;
    if (vol_0_100 > s_max_volume) vol_0_100 = s_max_volume;
    s_volume = vol_0_100;
    apply_hw_vol();
}

int bsp_audio_get_volume(void) { return s_volume; }

void bsp_audio_set_max_volume(int max_vol_50_100)
{
    if (max_vol_50_100 < 40) max_vol_50_100 = 40;
    if (max_vol_50_100 > 100) max_vol_50_100 = 100;
    s_max_volume = max_vol_50_100;
    if (s_volume > s_max_volume) s_volume = s_max_volume;
    apply_hw_vol();
}

int bsp_audio_get_max_volume(void) { return s_max_volume; }

void bsp_audio_sfx(bsp_sfx_t sfx)
{
    if (!s_cmd_q || s_playing) return; /* Do not interrupt active story narration */
    audio_cmd_t c = { .type = ACMD_SFX, .arg1 = (int)sfx };
    xQueueSend(s_cmd_q, &c, 0);
}

void bsp_audio_play_note(uint16_t freq_hz, uint16_t dur_ms)
{
    if (!s_cmd_q || s_playing) return;
    xQueueReset(s_cmd_q);
    audio_cmd_t c = {
        .type = ACMD_NOTE,
        .arg1 = ((int)dur_ms << 16) | (int)freq_hz,
    };
    xQueueSend(s_cmd_q, &c, 0);
}

void bsp_audio_start_melody(bsp_melody_t mel, bool loop)
{
    if (!s_cmd_q) return;
    xQueueReset(s_cmd_q);
    audio_cmd_t c = { .type = ACMD_MELODY, .arg1 = (int)mel, .flag = loop };
    xQueueSend(s_cmd_q, &c, 0);
}

void bsp_audio_stop_all(void)
{
    if (!s_cmd_q) return;
    xQueueReset(s_cmd_q);
    s_paused = false;
    audio_cmd_t c = { .type = ACMD_STOP };
    xQueueSend(s_cmd_q, &c, 0);
}

bool bsp_audio_play_wav(const char *filepath)
{
    if (!s_cmd_q || !filepath) return false;
    xQueueReset(s_cmd_q);
    s_wav_pct = 0;
    s_wav_permille = 0;
    s_paused = false;
    s_playing = true;
    audio_cmd_t c = { .type = ACMD_WAV };
    strncpy(c.path, filepath, sizeof(c.path) - 1);
    xQueueSend(s_cmd_q, &c, 0);
    return true;
}

void bsp_audio_toggle_pause(void) { s_paused = !s_paused; }
bool bsp_audio_is_playing(void)   { return s_playing; }
bool bsp_audio_is_paused(void)    { return s_paused; }
int  bsp_audio_get_wav_progress_pct(void)      { return s_wav_pct; }
int  bsp_audio_get_wav_progress_permille(void) { return s_wav_permille; }

bool bsp_audio_record_clip(int duration_sec)
{
    if (!s_rec_dev || !s_rec_buf) return false;
    if (duration_sec < 1) duration_sec = 1;
    if (duration_sec > MAX_REC_SECONDS) duration_sec = MAX_REC_SECONDS;

    bsp_audio_stop_all();
    vTaskDelay(pdMS_TO_TICKS(60));
    s_recording = true;
    /* Mute speaker PA during mic capture to prevent acoustic feedback */
    if (s_play_dev) esp_codec_dev_set_out_mute(s_play_dev, true);

    size_t target = (size_t)duration_sec * AUDIO_SR;
    size_t got = 0;
    while (got < target) {
        size_t chunk = (target - got > 512) ? 512 : (target - got);
        if (esp_codec_dev_read(s_rec_dev, &s_rec_buf[got], (int)(chunk * sizeof(int16_t))) != ESP_OK) {
            break;
        }
        got += chunk;
        /* Allow user to press thumbwheel OK or BOOT to stop recording early after 0.6s */
        if (got > AUDIO_SR / 2 &&
            (gpio_get_level(BSP_KEY_WHEEL_OK_PIN) == 0 || gpio_get_level(BSP_KEY_BOOT_PIN) == 0)) {
            break;
        }
    }
    s_rec_len = got;
    s_recording = false;
    apply_hw_vol();
    return s_rec_len > (AUDIO_SR / 4);
}

bool bsp_audio_has_recorded_clip(void) { return s_rec_len > 0; }

void bsp_audio_play_recorded_clip(bsp_voice_fx_t fx)
{
    if (!s_cmd_q || s_rec_len == 0) return;
    xQueueReset(s_cmd_q);
    audio_cmd_t c = { .type = ACMD_PLAY_REC, .arg1 = (int)fx };
    xQueueSend(s_cmd_q, &c, 0);
}

void bsp_audio_speak_syllables(uint16_t syl0, uint16_t syl1, uint16_t syl2)
{
    if (!s_cmd_q) return;
    xQueueReset(s_cmd_q);
    audio_cmd_t c = {
        .type = ACMD_HANZI_PRON,
        .syls = { syl0, syl1, syl2 },
    };
    xQueueSend(s_cmd_q, &c, 0);
}
