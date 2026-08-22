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

Cycles through six white-color presets:

`2200 K eq. -> 2700 K eq. -> 3200 K eq. -> 4000 K eq. -> 5000 K eq. -> 6500 K eq. -> ...`

These are no longer applied using WLED's `setCCT()` value. Instead, FlowButton converts the chosen white temperature to the corresponding RGB color-wheel value and lets WLED's **CCT from RGB** path derive the warm/cold channel ratio. This matches the physical behavior seen when changing the color manually in Solid.

The change is followed by an immediate forced render so it should be visible without waiting for Solid's normal refresh interval.

### Memory

FlowButton remembers the last non-zero brightness and the last white preset. Changes made from the WLED UI or presets are also learned. The remembered values are saved to WLED `cfg.json` after changes settle, so they survive reboot without repeatedly writing flash during a button hold.

## Effects

The wipe is an overlay over WLED's normal rendering. AudioReactive effects continue rendering normally. While a wipe is active, FlowButton forces normal animation frames so Solid does not update in large 350 ms jumps.

## OTA

The GitHub Actions workflow builds one firmware file named:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

Use that file in WLED: **Config -> Security & Updates -> Manual OTA Update**.

Existing WLED configuration and presets are stored separately from the firmware and should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
