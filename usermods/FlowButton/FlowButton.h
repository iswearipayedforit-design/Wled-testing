#pragma once

#include "wled.h"

// FlowButton for the user's 97-LED logical chain.
// Bus reversing is handled by WLED, so this code works in logical LED order:
// 0..36  = GPIO2 bus (configured Reversed)
// 37..96 = GPIO16 bus (normal)
class FlowButton : public Usermod {
private:
  static constexpr uint16_t FLOW_LED_COUNT = 97;
  static constexpr uint32_t FLOW_DURATION_MS = 2500;
  static constexpr uint16_t DEBOUNCE_MS = 50;

  float progress = 0.0f;              // visible LEDs, 0.0 .. FLOW_LED_COUNT
  int8_t direction = 0;               // +1 = wipe ON, -1 = wipe OFF, 0 = idle
  uint32_t lastAnimMs = 0;

  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t rawChangedMs = 0;
  bool initialized = false;

  void startOrReverse()
  {
    const uint32_t now = millis();

    if (direction != 0) {
      direction = -direction;          // reverse immediately from current position
      lastAnimMs = now;
      return;
    }

    // If WLED is currently off (or the wipe is fully hidden), start revealing it.
    if (bri == 0 || progress <= 0.001f) {
      if (bri == 0) {
        bri = briLast ? briLast : 128;
        strip.restartRuntime();
        stateChanged = true;
        stateUpdated(CALL_MODE_BUTTON);
      }
      direction = +1;
    } else {
      direction = -1;
    }

    lastAnimMs = now;
  }

  void updateAnimation()
  {
    if (direction == 0) return;

    const uint32_t now = millis();
    uint32_t dt = now - lastAnimMs;
    if (dt == 0) return;
    lastAnimMs = now;

    const float ledsPerMs = (float)FLOW_LED_COUNT / (float)FLOW_DURATION_MS;
    progress += (float)direction * ledsPerMs * (float)dt;

    if (progress >= (float)FLOW_LED_COUNT) {
      progress = (float)FLOW_LED_COUNT;
      direction = 0;
      return;
    }

    if (progress <= 0.0f) {
      progress = 0.0f;
      direction = 0;

      // Keep WLED fully off after the OFF wipe finishes.
      if (bri != 0) {
        briLast = bri;
        bri = 0;
        stateChanged = true;
        stateUpdated(CALL_MODE_BUTTON);
      }
    }
  }

public:
  void setup() override
  {
    // Match the initial overlay to WLED's power state.
    progress = (bri > 0) ? (float)FLOW_LED_COUNT : 0.0f;
    rawPressed = false;
    stablePressed = false;
    rawChangedMs = millis();
    initialized = true;
  }

  void loop() override
  {
    if (!initialized) return;
    updateAnimation();
  }

  // Intercept WLED button 0 and provide our own debounce/edge handling.
  // Returning true prevents WLED's normal short/long/double-click actions.
  bool handleButton(uint8_t b) override
  {
    if (!initialized || b != 0 || btnPin[b] < 0 || buttonType[b] == BTN_TYPE_NONE) return false;

    const uint32_t now = millis();
    const bool pressed = isButtonPressed(b);

    if (pressed != rawPressed) {
      rawPressed = pressed;
      rawChangedMs = now;
    }

    if ((now - rawChangedMs) >= DEBOUNCE_MS && stablePressed != rawPressed) {
      stablePressed = rawPressed;
      if (stablePressed) startOrReverse();
    }

    return true;
  }

  // WLED renders the current effect normally; this overlay only masks the
  // portion that has not yet been reached by the wipe. This means Solid,
  // palettes and AudioReactive effects all keep their native appearance.
  void handleOverlayDraw() override
  {
    if (!initialized) return;

    uint16_t visible = (uint16_t)progress;
    if (visible > FLOW_LED_COUNT) visible = FLOW_LED_COUNT;

    for (uint16_t i = visible; i < FLOW_LED_COUNT; i++) {
      strip.setPixelColor(i, 0);
    }
  }

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray info = user.createNestedArray("FlowButton");
    info.add(direction > 0 ? "Wipe ON" : direction < 0 ? "Wipe OFF" : progress > 0.0f ? "ON" : "OFF");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
