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

## FlowButton behavior

A press starts a wipe from logical LED 0 toward LED 96. The currently rendered WLED output is revealed, so Solid uses the currently selected color/brightness and AudioReactive effects remain AudioReactive.

When all LEDs are on, the next press wipes OFF in the opposite direction (LED 96 back toward LED 0). When the wipe finishes, WLED is switched fully off.

If the button is pressed during a wipe, direction reverses immediately from the current position, without blocking the WLED loop.

## OTA

The GitHub Actions workflow builds one firmware file named:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

Use that file in WLED: **Config -> Security & Updates -> Manual OTA Update**.

Existing WLED configuration and presets are stored separately from the firmware and should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
