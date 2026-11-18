#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SFX_WHEEL_TICK = 0,
    SFX_CONFIRM,
    SFX_BACK,
    SFX_SUCCESS,
    SFX_WRONG,
    SFX_FANFARE,
    SFX_PAGE_TURN,
    SFX_BUBBLE,
    SFX_BOUNCE,
} bsp_sfx_t;

typedef enum {
    MELODY_NONE = 0,
    MELODY_RABBIT,      /* 小兔子乖乖 */
    MELODY_TWINKLE,     /* 小星星 */
    MELODY_LULLABY,     /* 摇篮曲 */
    MELODY_ADVENTURE,   /* 森林奇遇 */
    MELODY_ODE_JOY,     /* 欢乐颂 */
} bsp_melody_t;

typedef enum {
    VOICE_NORMAL    = 0,  /* 1.00x 原声 */
    VOICE_CHIPMUNK  = 1,  /* 1.42x 花栗鼠搞怪声 */
    VOICE_BEAR      = 2,  /* 0.72x 大狗熊憨憨声 */
} bsp_voice_fx_t;

void bsp_audio_init(void);
void bsp_audio_shutdown(void);

/* Volume & Parental Hearing Safety Cap */
void bsp_audio_set_volume(int vol_0_100);
int  bsp_audio_get_volume(void);
void bsp_audio_set_max_volume(int max_vol_50_100);
int  bsp_audio_get_max_volume(void);

/* Non-blocking SFX, Interactive Xylophone Notes & Music-Box Melodies */
void bsp_audio_sfx(bsp_sfx_t sfx);
void bsp_audio_play_note(uint16_t freq_hz, uint16_t dur_ms);
void bsp_audio_start_melody(bsp_melody_t mel, bool loop);
void bsp_audio_stop_all(void);

/* SD Card WAV Story Streaming */
bool bsp_audio_play_wav(const char *filepath);
void bsp_audio_toggle_pause(void);
bool bsp_audio_is_playing(void);
bool bsp_audio_is_paused(void);
int  bsp_audio_get_wav_progress_pct(void);
int  bsp_audio_get_wav_progress_permille(void);

/* Onboard Mic PSRAM Voice Recorder & Parrot Voice Changer (up to 10s) */
bool bsp_audio_record_clip(int duration_sec);
bool bsp_audio_has_recorded_clip(void);
void bsp_audio_play_recorded_clip(bsp_voice_fx_t fx);

/* Offline Mandarin Teacher Pronunciation (16kHz Opus Syllable Bank, 0xFFFF = skip slot) */
void bsp_audio_speak_syllables(uint16_t syl0, uint16_t syl1, uint16_t syl2);

#ifdef __cplusplus
}
#endif
