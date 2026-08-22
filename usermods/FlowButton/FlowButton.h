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
      strip.trigger();                 // render the new direction immediately
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
    strip.trigger();
  }

  void updateAnimation()
  {
    if (direction == 0) return;

    const uint32_t now = millis();
    uint32_t dt = now - lastAnimMs;
    if (dt == 0) {
      strip.trigger();
      return;
    }
    lastAnimMs = now;

    const float ledsPerMs = (float)FLOW_LED_COUNT / (float)FLOW_DURATION_MS;
    progress += (float)direction * ledsPerMs * (float)dt;

    if (progress >= (float)FLOW_LED_COUNT) {
      progress = (float)FLOW_LED_COUNT;
      direction = 0;
      strip.trigger();
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
      } else {
        strip.trigger();
      }
      return;
    }

    // Critical for smooth FlowButton animation:
    // effects such as Solid normally render only every 350 ms in WLED 0.15.1.
    // trigger() forces the next frame to be computed on all active segments,
    // so the overlay follows progress at WLED's normal frame rate instead of
    // jumping by many LEDs at a time.
    strip.trigger();
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

  // WLED renders the current effect normally; this overlay masks the portion
  // that has not yet been reached by the wipe. A fractional boundary pixel is
  // brightness-scaled to make the moving edge visually smoother.
  void handleOverlayDraw() override
  {
    if (!initialized) return;

    float p = progress;
    if (p < 0.0f) p = 0.0f;
    if (p > (float)FLOW_LED_COUNT) p = (float)FLOW_LED_COUNT;

    const uint16_t fullVisible = (uint16_t)p;
    const float fraction = p - (float)fullVisible;

    // Smooth the boundary LED between black and the effect's rendered color.
    if (fullVisible < FLOW_LED_COUNT && fraction > 0.0f) {
      const uint32_t c = strip.getPixelColor(fullVisible);
      const uint16_t a = (uint16_t)(fraction * 255.0f);
      strip.setPixelColor(fullVisible, RGBW32(
        ((uint16_t)R(c) * a) / 255,
        ((uint16_t)G(c) * a) / 255,
        ((uint16_t)B(c) * a) / 255,
        ((uint16_t)W(c) * a) / 255
      ));
    }

    const uint16_t firstHidden = fullVisible + ((fraction > 0.0f) ? 1 : 0);
    for (uint16_t i = firstHidden; i < FLOW_LED_COUNT; i++) {
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
