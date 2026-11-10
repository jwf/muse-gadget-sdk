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

// Hands-free wake word ("Jarvis") via ESP-SR WakeNet. See wake_word.h.

#include "wake_word.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "model_path.h"

#include "voice_board.h"

static const char *TAG = "link.wake_word";

// Prebuilt WakeNet9 model (ESP-SR "Jarvis"). Selected in the board overlay
// with CONFIG_SR_WN_WN9_JARVIS_TTS=y; the esp-sr component packs it into
// srmodels.bin and flashes it to the "model" partition.
static const char *kModelName = "wn9_jarvis_tts";

// The mic stream is 16 kHz mono (voice_board.h: VOICE_MIC_RATE). WakeNet
// must agree, or the chunking below would be meaningless.
static const int kExpectedRate = VOICE_MIC_RATE;

static const esp_wn_iface_t *s_wn;
static model_iface_data_t *s_model;
static int16_t *s_chunk;          // one WakeNet-sized chunk (heap)
static int s_chunk_size;          // samples per detect() call
static size_t s_chunk_fill;       // samples currently buffered in s_chunk
static volatile bool s_available; // set only once init has fully succeeded

bool wake_word_init(void)
{
    if (s_available) return true;

    // Mounts the model partition (mmap, MODEL_IN_FLASH) and builds the
    // model list. The list must stay alive for the life of the device;
    // esp_wn_handle_from_name() looks the model up in it.
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "no model partition (flash the full image, not just "
                      "the app) - wake word off, push-to-talk still works");
        return false;
    }

    s_wn = esp_wn_handle_from_name(kModelName);
    if (!s_wn) {
        ESP_LOGE(TAG, "model %s not found in the model partition - "
                      "wake word off, push-to-talk still works", kModelName);
        return false;
    }

    // DET_MODE_90 is the normal threshold (the _MODEL_INFO_ in the model
    // is 0.627 at 90%% / 0.632 at 95%%). DET_MODE_95 is the aggressive
    // one; it loads from PSRAM and would false-trigger more.
    s_model = s_wn->create(kModelName, DET_MODE_90);
    if (!s_model) {
        ESP_LOGE(TAG, "WakeNet create failed for %s", kModelName);
        return false;
    }

    s_chunk_size = s_wn->get_samp_chunksize(s_model);
    int rate = s_wn->get_samp_rate(s_model);
    if (rate != kExpectedRate) {
        ESP_LOGE(TAG, "model wants %d Hz, mic is %d Hz - wake word off",
                 rate, kExpectedRate);
        s_wn->destroy(s_model);
        s_model = NULL;
        return false;
    }
    if (s_chunk_size <= 0) {
        ESP_LOGE(TAG, "model reports chunk size %d - wake word off",
                 s_chunk_size);
        s_wn->destroy(s_model);
        s_model = NULL;
        return false;
    }

    s_chunk = malloc((size_t)s_chunk_size * sizeof(int16_t));
    if (!s_chunk) {
        ESP_LOGE(TAG, "out of memory for the wake-word chunk buffer");
        s_wn->destroy(s_model);
        s_model = NULL;
        return false;
    }
    s_chunk_fill = 0;

    s_available = true;
    ESP_LOGI(TAG, "wake word ready: \"%s\" (chunk %d samples @ %d Hz)",
             kModelName, s_chunk_size, rate);
    return true;
}

bool wake_word_available(void)
{
    return s_available;
}

bool wake_word_feed(const int16_t *samples, size_t count)
{
    if (!s_available || !samples) return false;

    while (count > 0) {
        size_t n = (size_t)s_chunk_size - s_chunk_fill;
        if (n > count) n = count;
        memcpy(s_chunk + s_chunk_fill, samples, n * sizeof(int16_t));
        s_chunk_fill += n;
        samples += n;
        count -= n;

        if (s_chunk_fill == (size_t)s_chunk_size) {
            s_chunk_fill = 0;
            // >0 means detected (the example in esp_wn_models.h uses
            // r>0); 0 is no detection, negative is a channel-verified
            // false positive.
            wakenet_state_t r = s_wn->detect(s_model, s_chunk);
            if (r > 0) {
                // NOTE: do NOT call s_wn->clean() here. ESP-SR's documented
                // detect loop (esp_wn_models.h) is detect-only and has no
                // clean step; on this quantized model clean() crashes in
                // model_clean -> dl_convq_queue_bzero (null conv queue).
                // The detector is a sliding window and needs no reset:
                // after a detection the turn takes the mic over, and
                // listening resumes without one.
                ESP_LOGI(TAG, "wake word detected: \"%s\"", kModelName);
                return true;
            }
        }
    }
    return false;
}

void wake_word_deinit(void)
{
    if (s_model) {
        s_wn->destroy(s_model);
        s_model = NULL;
    }
    free(s_chunk);
    s_chunk = NULL;
    s_available = false;
}
