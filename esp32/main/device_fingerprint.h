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

// Device fingerprint for voice notes.
//
// A voice note from a gadget carries no device tag the chat can see: the
// firmware's "mcu-wearable" marker is a style hint, not an identity, and
// the chat-side device_id comes from the paired-device record, which the
// gadget's turns never get. Anyone in the room can speak to the gadget,
// and it has no unlock step, so those turns need stricter rules than a
// voice note recorded in the Muse app -- but only if the two can be told
// apart.
//
// This module marks every voice-note turn with a brief DTMF burst at its
// tail: three digits encoding this device's id plus a check digit. The
// burst is synthesized and fed through the same turn-audio path as the
// microphone, so it travels inside the note itself (the server passes the
// audio through byte-for-byte: the streaming WAV header survives with its
// 0xFFFFFFFF sizes, and a clap at the end of a turn survived in the
// delivered file, Nov 11 2026). The assistant decodes the tail of the
// delivered WAV and knows, from the bytes alone, which device sent it.
//
// The id is the last octet of the Wi-Fi STA MAC, so each device is distinct
// with no configuration: this gadget (node homelink-503f4c) is 0x4C, nibbles
// 4, c, 0 (DTMF keys 4, A, 0). Two devices sharing a last octet would
// the household has one gadget, and the check digit catches a misread,
// not a collision.
//
// Only the voice task calls this, at the end of a turn, after the capture
// loop and before muse_hatch_turn_end(). The tones add ~0.45 s to the
// turn; the reply timeout is ~60 s. Realtime calls (muse_voice_call_audio)
// are a different path and are not marked.

#pragma once

#include <stdbool.h>
#include <stdint.h>

// This device's fingerprint id: the last octet of the Wi-Fi STA MAC
// (0 when the MAC is unavailable). Stable across reboots; distinct per
// device unless two share a last octet.
uint8_t device_fingerprint_id(void);

// Append the fingerprint burst (three DTMF digits, ~0.45 s) to the turn
// in progress, through muse_hatch_turn_audio(). Call after the capture
// loop returns and before muse_hatch_turn_end(). A no-op when the id is
// 0 (MAC unavailable): an unmarked note is treated as unidentified, never
// as this device.
void device_fingerprint_append(void);
