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
  static constexpr uint16_t DOUBLE_CLICK_MS = 350;
  static constexpr uint16_t LONG_PRESS_MS = 600;
  static constexpr uint16_t HOLD_REPEAT_MS = 450;
  static constexpr uint16_t PERSIST_DELAY_MS = 1800;

  // Wipe state
  float progress = 0.0f;              // visible LEDs, 0.0 .. FLOW_LED_COUNT
  int8_t direction = 0;               // +1 = wipe ON, -1 = wipe OFF, 0 = idle
  uint32_t lastAnimMs = 0;

  // Button/debounce/gesture state
  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t rawChangedMs = 0;
  uint32_t pressStartedMs = 0;
  uint32_t lastHoldStepMs = 0;
  bool longHandled = false;

  bool clickPending = false;
  uint32_t firstReleaseMs = 0;

  // Remembered user settings. lastBrightness is always a non-zero WLED bri value.
  uint8_t lastBrightness = 255;
  uint8_t cctIndex = 2;                // default 3200 K
  bool configLoaded = false;
  bool persistPending = false;
  uint32_t persistAfterMs = 0;

  bool initialized = false;

  uint16_t cctKelvin(uint8_t index) const
  {
    // Six useful points from very warm to cool daylight.
    switch (index % 6) {
      case 0: return 2200;
      case 1: return 2700;
      case 2: return 3200;
      case 3: return 4000;
      case 4: return 5000;
      default: return 6500;
    }
  }

  uint8_t nearestCctIndex(uint16_t kelvin) const
  {
    uint8_t best = 0;
    uint32_t bestDiff = 0xFFFFFFFFUL;
    for (uint8_t i = 0; i < 6; i++) {
      const uint16_t k = cctKelvin(i);
      const uint32_t diff = (kelvin > k) ? (kelvin - k) : (k - kelvin);
      if (diff < bestDiff) {
        bestDiff = diff;
        best = i;
      }
    }
    return best;
  }

  uint16_t mainSegmentKelvin() const
  {
    // WLED 0.15.1 stores Segment::cct as 0..255, where Kelvin is
    // approximately 1900 + cct*32.
    const uint8_t cct = strip.getMainSegment().cct;
    return 1900U + ((uint16_t)cct << 5);
  }

  void schedulePersist()
  {
    persistPending = true;
    persistAfterMs = millis() + PERSIST_DELAY_MS;
  }

  void applyRememberedCct()
  {
    const uint16_t k = cctKelvin(cctIndex);
    // Apply to every active segment so both physical CCT buses always match.
    for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
      Segment& seg = strip.getSegment(s);
      if (seg.isActive()) seg.setCCT(k);
    }
    strip.trigger();
  }

  void startOrReverse()
  {
    const uint32_t now = millis();

    if (direction != 0) {
      direction = -direction;          // reverse immediately from current position
      lastAnimMs = now;
      strip.trigger();
      return;
    }

    // If WLED is currently off (or the wipe is fully hidden), start revealing it.
    if (bri == 0 || progress <= 0.001f) {
      if (bri == 0) {
        bri = lastBrightness;
        briLast = lastBrightness;
        applyRememberedCct();
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
    const uint32_t dt = now - lastAnimMs;
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

      // Keep WLED fully off after the OFF wipe finishes, but preserve the
      // remembered non-zero brightness for the next ON wipe.
      if (bri != 0) {
        lastBrightness = bri;
        briLast = lastBrightness;
        bri = 0;
        stateChanged = true;
        stateUpdated(CALL_MODE_BUTTON);
        schedulePersist();
      } else {
        strip.trigger();
      }
      return;
    }

    // Solid normally renders only every 350 ms in WLED 0.15.1. Force normal
    // animation frames while the wipe is moving so the overlay stays smooth.
    strip.trigger();
  }

  uint8_t nextBrightnessStep(uint8_t current) const
  {
    // Convert to percentage and make a closed 100 -> 90 -> ... -> 10 -> 100 loop.
    int pct = ((int)current * 100 + 127) / 255;
    if (pct <= 10) pct = 100;
    else {
      pct = ((pct - 1) / 10) * 10; // 100->90, 73->70, 20->10
      if (pct < 10) pct = 10;
    }
    return (uint8_t)((pct * 255 + 50) / 100);
  }

  void stepBrightness()
  {
    const uint8_t base = (bri > 0) ? bri : lastBrightness;
    lastBrightness = nextBrightnessStep(base);

    // Holding the button while OFF should give visible feedback immediately.
    if (bri == 0) {
      progress = (float)FLOW_LED_COUNT;
      direction = 0;
      applyRememberedCct();
      strip.restartRuntime();
    }

    bri = lastBrightness;
    briLast = lastBrightness;
    stateChanged = true;
    stateUpdated(CALL_MODE_BUTTON);
    strip.trigger();
    schedulePersist();
  }

  void cycleCct()
  {
    cctIndex = (cctIndex + 1) % 6;
    applyRememberedCct();
    stateChanged = true;
    stateUpdated(CALL_MODE_BUTTON);
    schedulePersist();
  }

  void handleGestureTimers()
  {
    const uint32_t now = millis();

    // Long press: first brightness step at 600 ms, then one step every 450 ms.
    if (stablePressed) {
      if (!longHandled && (now - pressStartedMs >= LONG_PRESS_MS)) {
        longHandled = true;
        clickPending = false; // a hold is never also a single/double click
        stepBrightness();
        lastHoldStepMs = now;
      } else if (longHandled && (now - lastHoldStepMs >= HOLD_REPEAT_MS)) {
        stepBrightness();
        lastHoldStepMs = now;
      }
    }

    // Single click is delayed only long enough to distinguish it from a double click.
    if (clickPending && !stablePressed && (now - firstReleaseMs > DOUBLE_CLICK_MS)) {
      clickPending = false;
      startOrReverse();
    }

    // Persist only after changes have settled, and never while the strip is busy.
    if (persistPending && (int32_t)(now - persistAfterMs) >= 0 && !strip.isUpdating()) {
      persistPending = false;
      serializeConfig();
    }
  }

public:
  void setup() override
  {
    // On the first firmware boot there is no FlowButton config yet, so inherit
    // the user's current WLED brightness/CCT instead of replacing it.
    if (!configLoaded) {
      uint8_t current = (bri > 0) ? bri : briLast;
      if (current == 0) current = 255;
      lastBrightness = current;
      cctIndex = nearestCctIndex(mainSegmentKelvin());
      schedulePersist();
    }

    // From then on, restore the remembered values after reboot. Preserve WLED's
    // ON/OFF boot state; only replace brightness when it booted ON.
    briLast = lastBrightness;
    if (bri > 0) bri = lastBrightness;
    applyRememberedCct();

    progress = (bri > 0) ? (float)FLOW_LED_COUNT : 0.0f;
    rawPressed = false;
    stablePressed = false;
    rawChangedMs = millis();
    initialized = true;

    stateChanged = true;
    stateUpdated(CALL_MODE_NO_NOTIFY);
  }

  void loop() override
  {
    if (!initialized) return;
    updateAnimation();
    handleGestureTimers();
  }

  // Intercept WLED button 0 completely. We implement debounce, short click,
  // double click and hold ourselves so the gestures cannot conflict.
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

      if (stablePressed) {
        pressStartedMs = now;
        lastHoldStepMs = now;
        longHandled = false;
      } else {
        // A completed long press must not also fire a click action.
        if (!longHandled) {
          if (clickPending && (now - firstReleaseMs <= DOUBLE_CLICK_MS)) {
            // Second short click: consume the pending single and change CCT.
            clickPending = false;
            cycleCct();
          } else {
            // First short click: wait briefly to see whether a second click follows.
            clickPending = true;
            firstReleaseMs = now;
          }
        }
      }
    }

    return true;
  }

  // WLED renders the current effect normally; this overlay masks the portion
  // that has not yet been reached by the wipe. The fractional boundary LED is
  // brightness-scaled for a smoother moving edge.
  void handleOverlayDraw() override
  {
    if (!initialized) return;

    float p = progress;
    if (p < 0.0f) p = 0.0f;
    if (p > (float)FLOW_LED_COUNT) p = (float)FLOW_LED_COUNT;

    const uint16_t fullVisible = (uint16_t)p;
    const float fraction = p - (float)fullVisible;

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

  // Also learn brightness/CCT changed from the WLED UI or presets, so the next
  // physical-button action starts from the user's latest settings.
  void onStateChange(uint8_t mode) override
  {
    if (!initialized) return;

    bool changed = false;
    if (bri > 0 && bri != lastBrightness) {
      lastBrightness = bri;
      changed = true;
    }

    const uint8_t nearest = nearestCctIndex(mainSegmentKelvin());
    if (nearest != cctIndex) {
      cctIndex = nearest;
      changed = true;
    }

    if (changed) schedulePersist();
  }

  void addToConfig(JsonObject& root) override
  {
    JsonObject top = root.createNestedObject("FlowButton");
    top["lastBrightness"] = lastBrightness;
    top["cctIndex"] = cctIndex;
  }

  bool readFromConfig(JsonObject& root) override
  {
    JsonObject top = root["FlowButton"];
    if (top.isNull()) {
      configLoaded = false;
      return false;
    }

    bool complete = true;
    complete &= getJsonValue(top["lastBrightness"], lastBrightness, (uint8_t)255);
    complete &= getJsonValue(top["cctIndex"], cctIndex, (uint8_t)2);

    // Never remember OFF as a brightness level; the loop intentionally starts at 10%.
    if (lastBrightness < 26) lastBrightness = 26;
    if (cctIndex > 5) cctIndex = 2;

    configLoaded = true;
    return complete;
  }

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray stateInfo = user.createNestedArray("FlowButton");
    stateInfo.add(direction > 0 ? "Wipe ON" : direction < 0 ? "Wipe OFF" : progress > 0.0f ? "ON" : "OFF");

    JsonArray briInfo = user.createNestedArray("Flow brightness");
    briInfo.add(((uint16_t)lastBrightness * 100 + 127) / 255);
    briInfo.add(" %");

    JsonArray cctInfo = user.createNestedArray("Flow CCT");
    cctInfo.add(cctKelvin(cctIndex));
    cctInfo.add(" K");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
