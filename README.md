# WLED 0.15.1 AudioReactive + FlowButton

Custom OTA build for classic ESP32 based on upstream WLED `v0.15.1`.

## Target configuration

- MCU: classic ESP32 (`esp32dev` OTA-compatible build, not `ESP32_V4`)
- WLED: 0.15.1
- AudioReactive: enabled via the stock WLED 0.15.1 ESP32 build
- FlowButton GPIO: 33, configured in WLED as a pushbutton
- LED chain (logical order):
  - GPIO2: Start 0, Length 37, Reversed = ON
  - GPIO16: Start 37, Length 60, Reversed = OFF
- Total FlowButton span: 97 LEDs
- Full wipe duration: 2500 ms
- Debounce: 50 ms

## FlowButton controls

### Single click

Starts the smooth wipe ON from logical LED 0 to LED 96. When the lamp is already on, a single click wipes OFF in the opposite direction. A click during a running wipe reverses direction from the current position.

Because the same button also supports double-click, the single-click action is confirmed after a **600 ms** double-click window.

### Hold

After 600 ms the global brightness moves through three raw WLED `bri` levels:

`60 -> 20 -> 5 -> 60 -> ...`

These are direct values on WLED's 1-255 brightness scale, not percentages. Another step is applied every 1000 ms while the button remains held.

### Double click

Cycles through **four presets reconstructed directly from the user's WLED screenshots**. Each preset reproduces all three visible color controls: color-wheel RGB, white slider at 255, and the CCT slider position.

1. Red warm — RGBW `(255, 27, 26, 255)`, CCT `0/255`
2. Amber warm — RGBW `(255, 146, 28, 255)`, CCT `0/255`
3. Warm white — RGBW `(255, 213, 176, 255)`, CCT `64/255`
4. Cool white — RGBW `(231, 234, 255, 255)`, CCT `180/255`

The presets loop continuously with each double click.

### Memory

FlowButton remembers the last non-zero brightness and the last white preset. Changes made from the WLED UI are matched to the nearest of the four presets. The remembered values are saved to WLED `cfg.json` after changes settle so they survive reboot without repeatedly writing flash during a button hold.

## Effects

The wipe is an overlay over WLED's normal rendering. AudioReactive effects continue rendering normally. While a wipe is active, FlowButton forces normal animation frames so Solid does not update in large jumps.

## OTA

The GitHub Actions workflow builds one firmware file named:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

Use that file in WLED: **Config -> Security & Updates -> Manual OTA Update**.

Existing WLED configuration and presets are stored separately from the firmware and should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
