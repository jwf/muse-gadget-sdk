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


#pragma once

#include <stdint.h>

#include "esp_err.h"

// reSpeaker XVF3800 extras shared with the LED status backend. The voice
// board file owns the I2C bus to the XMOS, so the ring goes through it.

#define XVF3800_LED_COUNT 12

// Show one 12-LED frame on the ring: rgb is 12 x {R, G, B}. Unchanged
// frames skip the bus. Returns ESP_ERR_INVALID_STATE until the voice board
// has the I2C bus up (the LED task starts before voice init).
esp_err_t xvf3800_led_ring_show(const uint8_t rgb[XVF3800_LED_COUNT][3]);
