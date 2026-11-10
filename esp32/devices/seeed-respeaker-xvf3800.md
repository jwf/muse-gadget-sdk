<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Seeed reSpeaker XVF3800 with XIAO ESP32-S3 (experimental)

This Link-only profile adds a 12-LED status ring, push-to-talk voice
notes, and a hands-free wake word ("Jarvis"). Hold XIAO BOOT to record
and release to send; replies appear as text in the Muse app. Or just say
"Jarvis" and ask — no button needed. It uses the XMOS-processed
microphone channel over I2S, at 16 kHz.

Fork changes vs upstream: upstream leaves the vendor LED ring unused and
has no wake word. This fork drives the ring over I2C as the Muse status
indicator (orange advertising, blue pairing, green connected, voice
animations while recording) and adds the "Jarvis" wake word (see below).
The driver reads the processed ASR stream from the right I2S slot;
the plain-16 kHz / 48 kHz-master firmware option from the earlier port
is dropped — the plain image is verified on hardware and is the only
supported image (see Hardware below).

## Hardware and XMOS firmware

Use only the reSpeaker XVF3800 SKU with a pre-soldered XIAO ESP32-S3, 8 MB
flash and 8 MB octal PSRAM. Versions without XIAO are not supported. Attach
the XIAO antenna and use 2.4 GHz Wi-Fi.

Install Seeed's `respeaker_xvf3800_i2s_slave_v1.0.9_16k.bin` baseline image,
or a documented compatible newer same-major 16 kHz I2S-slave image. USB-audio,
48 kHz and I2S-master images are out of scope. Keep the stock two-channel
output routing. The ESP32 accepts firmware with the same major version and a
minimum version of 1.0.9, but VERSION alone does not establish the interface,
channel routing or image family.

The ESP32 supplies the I2S clocks at 16 kHz with stereo 32-bit Philips slots.
Capture uses the right ASR slot. The driver does not reset XMOS, rewrite
output routes, save XMOS configuration or claim verified echo cancellation.
Flashing the XIAO does not update XMOS.

Flash the XMOS firmware through the XMOS USB-C port (near the 3.5 mm jack)
with
`dfu-util -R -e -a 1 -D <image>.bin`
(DFU device `2886:001a`). If the board won't enumerate, hold Mute while
powering on to enter Safe Mode, which supports USB DFU. Once the I2S
firmware is on, it supports I2C DFU only, so get the image right before
flashing. The ESP32 flash command below does not update XMOS.

Firmware sources:
[respeaker/reSpeaker_XVF3800_USB_4MIC_ARRAY](https://github.com/respeaker/reSpeaker_XVF3800_USB_4MIC_ARRAY/tree/master/xmos_firmwares/i2s)
(`xmos_firmwares/i2s/`; the plain image is
`respeaker_xvf3800_i2s_slave_v1.0.9_16k.bin`). Seeed's firmware table on the
[reSpeaker XVF3800 wiki](https://wiki.seeedstudio.com/respeaker_xvf3800_introduction/)
lists the same images.

References: [vendor source and firmware](https://github.com/respeaker/reSpeaker_XVF3800_USB_4MIC_ARRAY),
[XIAO setup](https://wiki.seeedstudio.com/respeaker_xvf3800_xiao_getting_started/),
[I2S](https://wiki.seeedstudio.com/respeaker_xvf3800_xiao_i2s/) and
[I2C control](https://wiki.seeedstudio.com/respeaker_xvf_3800_i2c_list/).
The I2S pins and XMOS servicer protocol (GPO mute, LED ring, audio tuning)
are also documented in the
[formatBCE/Respeaker-XVF3800-ESPHome-integration](https://github.com/formatBCE/Respeaker-XVF3800-ESPHome-integration)
(`packages/hardware.yaml`, `esphome/components/respeaker_xvf3800/`).
The I2S data lines are transposed vs the reSpeaker Lite on this board:
the ESP32 receives mic data on GPIO43 and transmits speaker data on
GPIO44. This matches Seeed's I2S test sketch and the ESPHome
integration; do not "fix" it to the Lite's pinout.

## Build and flash

Use ESP-IDF v6.0.3. Connect USB to the small XIAO board, which exposes the
ESP32 USB Serial/JTAG console (`303a:1001`). From `esp32/`:

```sh
tools/board.sh seeed-respeaker-xvf3800 build
idf.py -B build-seeed-respeaker-xvf3800 menuconfig
```

Set your [SDK token](https://gadgets.muse.ai/settings/sdk-tokens) under ESP32
Device SDK, then rebuild and flash:

```sh
tools/board.sh seeed-respeaker-xvf3800 build
tools/board.sh seeed-respeaker-xvf3800 flash /dev/cu.usbmodemXXXX
```

Use the actual port. Linux normally exposes `/dev/ttyACM0`. See the
[flashing guide](../AGENTS.md#flash) before replacing firmware.

## Pairing and controls

In Muse, enable Settings > Devices > Developer mode and add
`MuseGadget-xvf3800-XXXXXX`. Press XIAO BOOT when prompted and finish Wi-Fi
setup. With the XIAO silver label facing you and USB-C at the top, BOOT is the
small button marked **B** to the right of USB-C. **R** resets the ESP32.

**The case pinhole is NOT BOOT.** It is the main board's Reset for the XMOS.
Pairing confirmation and push-to-talk need the XIAO's **B** button itself,
which is not exposed outside the case — open the case to press it. (A
pinhole press during setup holds the XMOS in reset: the LED ring goes dark
and I2C stops answering, and the phone times out.) The wake word below is
the case-closed way to talk to it.

- Hold XIAO BOOT to record and release to send.
- Say "Jarvis" to start a turn hands-free (see Wake word below). The
  button still works while listening. Muting stops wake-word listening
  until you unmute.
- The 12-LED ring shows the status colours (orange breathing while
  advertising, blue while pairing, green when connected, voice animations
  while recording). The ring is driven over I2C by the XMOS, not by the
  ESP32's RMT peripheral.
- The larger board's hardware mute is read through X0D30. Muted or unreadable
  audio is dropped after every capture chunk as well as before a turn.
- While hardware mute is active, XIAO BOOT keeps its setup role. A five-second
  hold forgets pairing and Wi-Fi.
- X0D31 controls the amplifier, active low. Initialization turns it off.
- Volume uses only the AIC3104's digital mute and attenuation registers. It
  does not enable analog boost or rewrite output routes.
- Calls remain half duplex because this profile does not claim a verified AEC
  playback-reference route.

## Wake word (hands-free)

Say **"Jarvis"** and then ask your question — no button press. The wake
word is on by default for this board (`CONFIG_HOMEHUB_WAKE_WORD`, under
ESP32 Device SDK in menuconfig; turn it off for push-to-talk only). It
uses ESP-SR WakeNet with the prebuilt `wn9_jarvis_tts` model
(`espressif/esp-sr` 2.6.1): no training, runs on-chip, about 291 KB.
No ESP-SR AFE is used — the XMOS already does AEC, noise suppression,
beamforming and AGC, so WakeNet listens directly on the 16 kHz mic
stream.

The model is not in the app binary: it is packed into `srmodels.bin` and
flashed to a 1 MB `model` partition (`partitions.csv`). A full flash
(`tools/board.sh seeed-respeaker-xvf3800 flash ...`) writes it; flashing
only the app (`idf.py app-flash`) does not, and the device then falls
back to push-to-talk only with a log line saying the model partition is
missing.

Behaviour: the device listens only once it is paired and unmuted (the
same guard as the button). While listening the ring stays green
("connected"); on "Jarvis" it switches to the voice-listening animation
exactly as for a button press. The turn ends about 2 s after you stop
talking, or at the 15 s cap in a noisy room. The wake word itself is not
sent — the turn starts after it, so say "Jarvis" and then your question
without a long pause. The button still works while listening: hold it
for a push-to-talk turn instead. Muting stops listening (the DSP mutes
the mics); unmuting resumes it.

To test: flash the full image, pair, and with the case closed say
"Jarvis, what time is it?" (or any question). The serial log should
show `link.wake_word: wake word detected: "wn9_jarvis_tts"` and then the
usual `link.voice: recorded ...` line, and the reply appears as text in
the app. If it does not trigger, check the boot log for
`link.wake_word: wake word ready: "wn9_jarvis_tts"` — if that line is
missing, the model partition did not flash (re-flash the full image).

Nov 9, 2026: the first wake-word build crashed on the first "Jarvis" —
a `LoadProhibited` panic in `model_clean` (`dl_convq_queue_bzero`)
from the `s_wn->clean()` call in `wake_word_feed` right after
detection. `clean` is not part of ESP-SR's documented detect loop, so
it was removed; the detector needs no reset between listens. **The fix
was verified on hardware the same evening:** two "Jarvis, what time is
it?" turns fired with no crash, silence-based turn end worked (2.7 s and
9.4 s recordings), and the first turn round-tripped in ~28 s. The second
turn hit a device-side "NO REPLY FROM MUSE" timeout after ~60 s, but the
message still reached chat and was answered — reply latency vs the
device's reply timeout is an open watch item. Do NOT reintroduce the
`clean()` call.

## Validation limits

Hardware-verified Nov 9, 2026: boot, PSRAM, microphone samples, pairing,
push-to-talk (hold BOOT, speak, release, reply as text), the "Jarvis"
wake word (two turns, no crash — see above), and the LED ring status
colours through pairing. Host tests cover control framing, version
policy, pin configuration, PCM conversion, mute and amplifier policy,
volume and failure behavior. Not yet verified: speaker playback, the
physical mute toggle, volume zero, amplifier sequencing, and setup reset.
The right-slot mic read (upstream driver) replaces the left-slot read of
the earlier port — both slots carry a processed voice stream per the
firmware routing, and PTT plus wake-word detection are the verification
for it. Clean builds do not establish audible channel content or timing.
