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

Because the same button also supports double-click, the single-click action is confirmed after a 450 ms double-click window.

### Hold

After 600 ms the global brightness begins moving through this closed dimming loop:

`90% -> 60% -> 30% -> 10% -> 2% -> 90% -> ...`

If the current brightness is above or between these values, the first hold step chooses the next lower value. Another step is applied every 1000 ms while the button remains held.

Each brightness change bypasses the normal WLED transition delay and forces an immediate render, just like the wipe animation.

### Double click

Cycles through five deliberately strong white presets:

1. Very warm — red/orange wheel tint + 1900 K CCT
2. Warm — amber/orange wheel tint + 2600 K CCT
3. Neutral — warm-neutral wheel tint + 3500 K CCT
4. Cool — blue-white wheel tint + 5500 K CCT
5. Very cool — strong blue wheel tint + 10000 K CCT

Each preset sets **both** the primary color-wheel value and the CCT slider value. This reproduces the stronger manual combinations possible in WLED (for example red/orange plus maximum warm CCT) instead of relying on CCT or RGB alone.

The change is followed by an immediate forced render so it is visible without waiting for Solid's normal refresh interval.

### Memory

FlowButton remembers the last non-zero brightness and the last white preset. Changes made from the WLED UI or presets are also learned. The remembered values are saved to WLED `cfg.json` after changes settle, so they survive reboot without repeatedly writing flash during a button hold.

## Effects

The wipe is an overlay over WLED's normal rendering. AudioReactive effects continue rendering normally. While a wipe is active, FlowButton forces normal animation frames so Solid does not update in large 350 ms jumps.

## OTA

The GitHub Actions workflow builds one firmware file named:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

Use that file in WLED: **Config -> Security & Updates -> Manual OTA Update**.

Existing WLED configuration and presets are stored separately from the firmware and should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
