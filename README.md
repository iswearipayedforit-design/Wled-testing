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

Because the same button also supports double-click, the single-click action is confirmed after a 350 ms double-click window.

### Hold

After 600 ms the global brightness moves through a closed loop in 10 percentage-point steps. While the button remains held, another step is applied every 450 ms:

`100 -> 90 -> 80 -> 70 -> 60 -> 50 -> 40 -> 30 -> 20 -> 10 -> 100 -> ...`

Holding while the lamp is OFF turns it on at the next brightness step so the change is immediately visible.

### Double click

Cycles the CCT temperature through six useful white points, from very warm to cool daylight:

`2200 K -> 2700 K -> 3200 K -> 4000 K -> 5000 K -> 6500 K -> 2200 K -> ...`

The CCT selection is applied to every active segment so both physical CCT LED buses stay matched.

### Memory

FlowButton remembers the last non-zero brightness and the last CCT selection. Changes made from the WLED UI or presets are also learned. The remembered values are saved to WLED `cfg.json` after changes settle, so they survive reboot without repeatedly writing flash during a button hold.

## Effects

The wipe is an overlay over WLED's normal rendering. Solid uses the selected CCT/brightness, and AudioReactive effects continue rendering normally. While a wipe is active, FlowButton forces normal animation frames so Solid does not update in large 350 ms jumps.

## OTA

The GitHub Actions workflow builds one firmware file named:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

Use that file in WLED: **Config -> Security & Updates -> Manual OTA Update**.

Existing WLED configuration and presets are stored separately from the firmware and should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
