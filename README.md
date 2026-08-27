# WLED 0.15.1 AudioReactive + custom button controls

Custom OTA builds for classic ESP32 based on upstream WLED `v0.15.1`.

## Shared button controls

- Button GPIO: 33, pushbutton
- Double-click window: 600 ms
- Hold brightness loop, raw WLED `bri` values: `60 -> 20 -> 5 -> 60 -> ...`
- Hold repeat: every 1000 ms after the initial 600 ms hold
- Double click cycles four user-picked RGBW+CCT presets:
  1. RGBW `(255, 27, 26, 255)`, CCT `0/255`
  2. RGBW `(255, 146, 28, 255)`, CCT `0/255`
  3. RGBW `(255, 213, 176, 255)`, CCT `64/255`
  4. RGBW `(231, 234, 255, 255)`, CCT `180/255`
- Last non-zero brightness and white preset are remembered in WLED config
- AudioReactive remains enabled through the stock WLED 0.15.1 ESP32 build

## FlowButton variant

File:

`WLED_0.15.1_ESP32_AudioReactive_FlowButton_OTA.bin`

This is the original installation-specific build for the 97-LED chain:

- GPIO2: Start 0, Length 37, Reversed
- GPIO16: Start 37, Length 60, normal
- Single click performs the directional 2.5 s wipe ON/OFF

## SmoothButton variant

File:

`WLED_0.15.1_ESP32_AudioReactive_SmoothButton_OTA.bin`

This is the reusable build for other WLED installations.

- No per-pixel wipe or sweep
- No dependency on 97 LEDs or a particular LED bus layout
- Single click uses WLED's native ON/OFF brightness transition
- Turning ON fades naturally from 0 to the remembered brightness
- Turning OFF fades naturally from the current brightness to 0
- Fade duration follows the normal transition setting configured in WLED
- Double-click colors, hold brightness levels and memory are identical to FlowButton

## OTA

Use the appropriate `.bin` in WLED:

**Config -> Security & Updates -> Manual OTA Update**

Existing WLED configuration and presets should remain in place during normal OTA. Backing them up before firmware changes is still recommended.
