/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device_fingerprint.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"

#include "identity.h"
#include "muse_chat.h"
#include "voice_board.h"

static const char *TAG = "link.fingerprint";

// DTMF, 100 ms of tone + 50 ms of silence per digit, three digits.
// 16 kHz mono; fed in 20 ms chunks, the capture loop's own size.
#define FP_TONE_MS      100
#define FP_GAP_MS       50
#define FP_DIGITS       3
#define FP_CHUNK        (VOICE_MIC_RATE / 50)
// Each sine at 0.30 full scale: the pair peaks near 0.60, clearly above
// the room's noise floor and below clipping. 5 ms raised-cosine ramps
// keep the tone edges from clicking.
#define FP_AMP          (0.30f * 32767.0f)
#define FP_RAMP_SAMPLES (VOICE_MIC_RATE * 5 / 1000)

// Standard DTMF: rows are the low group, columns the high group. Value
// 0-9 is the digit, 10 '*', 11 '#', 12-15 'A'-'D'.
static const uint16_t s_low[4] = {697, 770, 852, 941};
static const uint16_t s_high[4] = {1209, 1336, 1477, 1633};

// The keypad row/column of a 4-bit value.
static void fp_freqs(uint8_t value, uint16_t *low, uint16_t *high)
{
    static const uint8_t row_of[16] = {3, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 0, 1, 2, 3};
    static const uint8_t col_of[16] = {1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 2, 3, 3, 3, 3};
    *low = s_low[row_of[value & 0xF]];
    *high = s_high[col_of[value & 0xF]];
}

static uint8_t fp_parse_hex(const char *s)
{
    uint8_t v = 0;
    for (int i = 0; i < 2 && s[i]; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint8_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint8_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint8_t)(c - 'A' + 10);
    }
    return v;
}

uint8_t device_fingerprint_id(void)
{
    // identity_mac() is "xx:xx:xx:xx:xx:xx"; the id is its last octet.
    const char *mac = identity_mac();
    size_t len = strlen(mac);
    if (len < 2) return 0;
    return fp_parse_hex(mac + len - 2);
}

// One digit of tone (plus its trailing gap) into the turn, in FP_CHUNK
// pieces. phase carries across chunks so the sine stays continuous.
static void fp_tone(uint16_t low, uint16_t high, float *phase_low, float *phase_high)
{
    static int16_t chunk[FP_CHUNK];
    const int tone_samples = VOICE_MIC_RATE * FP_TONE_MS / 1000;
    const int gap_samples = VOICE_MIC_RATE * FP_GAP_MS / 1000;
    const float step_low = 2.0f * (float)M_PI * (float)low / (float)VOICE_MIC_RATE;
    const float step_high = 2.0f * (float)M_PI * (float)high / (float)VOICE_MIC_RATE;
    int sent = 0;
    while (sent < tone_samples + gap_samples) {
        int n = tone_samples + gap_samples - sent;
        if (n > FP_CHUNK) n = FP_CHUNK;
        for (int i = 0; i < n; i++) {
            int pos = sent + i;
            int16_t s = 0;
            if (pos < tone_samples) {
                // Raised-cosine ramps at both edges of the tone.
                float ramp = 1.0f;
                if (pos < FP_RAMP_SAMPLES) {
                    ramp = 0.5f - 0.5f * cosf((float)M_PI * (float)pos / (float)FP_RAMP_SAMPLES);
                } else if (pos >= tone_samples - FP_RAMP_SAMPLES) {
                    ramp = 0.5f - 0.5f * cosf((float)M_PI * (float)(tone_samples - 1 - pos) /
                                              (float)FP_RAMP_SAMPLES);
                }
                float v = (sinf(*phase_low) + sinf(*phase_high)) * FP_AMP * ramp;
                s = (int16_t)v;
                *phase_low += step_low;
                *phase_high += step_high;
                if (*phase_low >= 2.0f * (float)M_PI) *phase_low -= 2.0f * (float)M_PI;
                if (*phase_high >= 2.0f * (float)M_PI) *phase_high -= 2.0f * (float)M_PI;
            }
            chunk[i] = s;
        }
        muse_hatch_turn_audio(chunk, (size_t)n);
        sent += n;
    }
}

void device_fingerprint_append(void)
{
    uint8_t id = device_fingerprint_id();
    if (!id) {
        ESP_LOGW(TAG, "no MAC, note unmarked");
        return;
    }
    uint8_t digits[FP_DIGITS];
    digits[0] = (uint8_t)(id >> 4);
    digits[1] = (uint8_t)(id & 0xF);
    digits[2] = (uint8_t)((digits[0] + digits[1]) & 0xF);
    float phase_low = 0.0f, phase_high = 0.0f;
    for (int i = 0; i < FP_DIGITS; i++) {
        uint16_t low, high;
        fp_freqs(digits[i], &low, &high);
        fp_tone(low, high, &phase_low, &phase_high);
    }
    ESP_LOGI(TAG, "marked note as device 0x%02x (digits %x %x %x)", id,
             digits[0], digits[1], digits[2]);
}
