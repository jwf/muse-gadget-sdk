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

// Hands-free wake word ("Jarvis") via ESP-SR WakeNet, for boards whose talk
// button is not reachable with the case on (the reSpeaker XVF3800's XIAO
// BOOT sits inside the case). WakeNet runs directly on the 16 kHz mic
// stream: the XMOS already does AEC/NS/beamforming/AGC, so no ESP-SR AFE
// is used.
//
// The model is the prebuilt wn9_jarvis_tts model. It lives in the "model"
// partition (srmodels.bin), NOT in the app binary, and is loaded through
// esp_srmodel_init(). If the partition or the model is missing, init fails
// and voice falls back to push-to-talk only; nothing here panics.
//
// Only the voice task feeds samples (single consumer, same as the mic
// itself); init runs on the main task in voice_init(), before the voice
// task starts, so there is no race on the handle.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Load the "Jarvis" model and prepare WakeNet. Returns false (and logs)
// if the model partition or model is missing, or the model wants a sample
// rate the mic does not provide; voice then stays push-to-talk only.
bool wake_word_init(void);

// True once init has succeeded. The voice task listens for the wake word
// only while this is true.
bool wake_word_available(void);

// Feed 16 kHz mono mic samples. Returns true the first time "Jarvis" is
// detected in the stream. Samples are accumulated into WakeNet-sized
// chunks internally; the model state carries across calls. After a
// detection the model state is reset, so the next listen starts clean.
bool wake_word_feed(const int16_t *samples, size_t count);

// Release the WakeNet handle. Not used by the app (the model stays loaded
// for the life of the device); provided for completeness.
void wake_word_deinit(void);
