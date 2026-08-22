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
  static constexpr uint16_t DOUBLE_CLICK_MS = 450;
  static constexpr uint16_t LONG_PRESS_MS = 600;
  static constexpr uint16_t HOLD_REPEAT_MS = 1000;
  static constexpr uint16_t PERSIST_DELAY_MS = 1800;
  static constexpr uint8_t WHITE_PRESET_COUNT = 5;

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

  // Remembered user settings. lastBrightness is always non-zero.
  uint8_t lastBrightness = 255;
  uint8_t whiteIndex = 2;              // default neutral preset
  bool configLoaded = false;
  bool persistPending = false;
  uint32_t persistAfterMs = 0;

  bool initialized = false;

  uint16_t whiteKelvin(uint8_t index) const
  {
    // CCT part of each combined RGB+CCT preset.
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return 1900;   // warmest possible WLED CCT end
      case 1: return 2600;
      case 2: return 3500;
      case 3: return 5500;
      default: return 10000; // coldest practical WLED CCT end
    }
  }

  const char* whiteName(uint8_t index) const
  {
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return "Very warm";
      case 1: return "Warm";
      case 2: return "Neutral";
      case 3: return "Cool";
      default: return "Very cool";
    }
  }

  uint32_t whiteColor(uint8_t index) const
  {
    // Deliberately stronger wheel tints than a pure Kelvin conversion.
    // The CCT slider is also set separately in applyRememberedWhite(), so the
    // extreme presets reproduce the user's manual "red + full warm" / 
    // "blue + full cold" behaviour instead of looking washed out.
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return RGBW32(255,  18,   0, 255); // deep red-orange + max warm CCT
      case 1: return RGBW32(255, 105,  12, 255); // amber/orange + warm CCT
      case 2: return RGBW32(255, 225, 185, 255); // warm-neutral white
      case 3: return RGBW32(185, 220, 255, 255); // cool white with blue tint
      default:return RGBW32( 65, 135, 255, 255); // strong blue tint + max cold CCT
    }
  }

  uint8_t nearestWhiteIndex(uint16_t kelvin) const
  {
    uint8_t best = 0;
    uint32_t bestDiff = 0xFFFFFFFFUL;
    for (uint8_t i = 0; i < WHITE_PRESET_COUNT; i++) {
      const uint16_t k = whiteKelvin(i);
      const uint32_t diff = (kelvin > k) ? (kelvin - k) : (k - kelvin);
      if (diff < bestDiff) {
        bestDiff = diff;
        best = i;
      }
    }
    return best;
  }

  uint16_t mainSegmentKelvinFromColor() const
  {
    return approximateKelvinFromRGB(strip.getMainSegment().colors[0]);
  }

  uint8_t brightnessFromPercent(uint8_t pct) const
  {
    return (uint8_t)(((uint16_t)pct * 255U + 50U) / 100U);
  }

  void schedulePersist()
  {
    persistPending = true;
    persistAfterMs = millis() + PERSIST_DELAY_MS;
  }

  void immediateStateUpdate(uint8_t callMode)
  {
    // Button actions should appear immediately, regardless of WLED's configured
    // transition time. trigger() also bypasses Solid's normal 350 ms refresh delay.
    const bool oldFadeTransition = fadeTransition;
    fadeTransition = false;
    stateChanged = true;
    stateUpdated(callMode);
    fadeTransition = oldFadeTransition;
    strip.trigger();
  }

  void applyRememberedWhite()
  {
    const uint16_t k = whiteKelvin(whiteIndex);
    const uint32_t color = whiteColor(whiteIndex);

    // Apply BOTH controls that gave the strongest result manually in WLED:
    // primary wheel color plus the CCT slider position.
    for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
      Segment& seg = strip.getSegment(s);
      if (!seg.isActive()) continue;
      seg.setColor(0, color);
      seg.setCCT(k);
    }

    immediateStateUpdate(CALL_MODE_BUTTON);
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
        applyRememberedWhite();
        strip.restartRuntime();
        immediateStateUpdate(CALL_MODE_BUTTON);
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
        immediateStateUpdate(CALL_MODE_BUTTON);
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
    // Closed dimming loop:
    // >90 -> 90 -> 60 -> 30 -> 10 -> 2 -> 90 -> ...
    const uint8_t pct = ((uint16_t)current * 100U + 127U) / 255U;

    if (pct > 90) return brightnessFromPercent(90);
    if (pct > 60) return brightnessFromPercent(60);
    if (pct > 30) return brightnessFromPercent(30);
    if (pct > 10) return brightnessFromPercent(10);
    if (pct > 2)  return brightnessFromPercent(2);
    return brightnessFromPercent(90);
  }

  void stepBrightness()
  {
    const uint8_t base = (bri > 0) ? bri : lastBrightness;
    lastBrightness = nextBrightnessStep(base);

    // Holding the button while OFF gives visible feedback immediately.
    if (bri == 0) {
      progress = (float)FLOW_LED_COUNT;
      direction = 0;
      bri = lastBrightness;
      briLast = lastBrightness;
      applyRememberedWhite();
      strip.restartRuntime();
    } else {
      bri = lastBrightness;
      briLast = lastBrightness;
    }

    immediateStateUpdate(CALL_MODE_BUTTON);
    schedulePersist();
  }

  void cycleWhite()
  {
    whiteIndex = (whiteIndex + 1) % WHITE_PRESET_COUNT;
    applyRememberedWhite();
    schedulePersist();
  }

  void handleGestureTimers()
  {
    const uint32_t now = millis();

    // Long press: first step at 600 ms, then one brightness step every second.
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
    // the user's current WLED brightness and current wheel-derived white color.
    if (!configLoaded) {
      uint8_t current = (bri > 0) ? bri : briLast;
      if (current == 0) current = 255;
      lastBrightness = current;
      whiteIndex = nearestWhiteIndex(mainSegmentKelvinFromColor());
      schedulePersist();
    }

    // Restore remembered values after reboot. Preserve WLED's ON/OFF boot state.
    briLast = lastBrightness;
    if (bri > 0) bri = lastBrightness;
    applyRememberedWhite();

    progress = (bri > 0) ? (float)FLOW_LED_COUNT : 0.0f;
    rawPressed = false;
    stablePressed = false;
    rawChangedMs = millis();
    initialized = true;

    immediateStateUpdate(CALL_MODE_NO_NOTIFY);
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
            // Second short click: consume the pending single and change white color.
            clickPending = false;
            cycleWhite();
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

  // Learn brightness/white changes made from the WLED UI or presets, so the
  // next physical-button action starts from the user's latest settings.
  void onStateChange(uint8_t mode) override
  {
    if (!initialized) return;

    bool changed = false;
    if (bri > 0 && bri != lastBrightness) {
      lastBrightness = bri;
      changed = true;
    }

    const uint8_t nearest = nearestWhiteIndex(mainSegmentKelvinFromColor());
    if (nearest != whiteIndex) {
      whiteIndex = nearest;
      changed = true;
    }

    if (changed) schedulePersist();
  }

  void addToConfig(JsonObject& root) override
  {
    JsonObject top = root.createNestedObject("FlowButton");
    top["lastBrightness"] = lastBrightness;
    top["whiteIndex"] = whiteIndex;
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

    // New builds save whiteIndex. For compatibility with previous test builds,
    // accept cctIndex as the same position and clamp the old 6th position to 5th.
    if (!getJsonValue(top["whiteIndex"], whiteIndex)) {
      complete &= getJsonValue(top["cctIndex"], whiteIndex, (uint8_t)2);
    }

    // Never remember OFF as a brightness level. 2% is the minimum loop step.
    const uint8_t minimumBrightness = brightnessFromPercent(2);
    if (lastBrightness < minimumBrightness) lastBrightness = minimumBrightness;
    if (whiteIndex >= WHITE_PRESET_COUNT) whiteIndex = WHITE_PRESET_COUNT - 1;

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

    JsonArray whiteInfo = user.createNestedArray("Flow white");
    whiteInfo.add(whiteName(whiteIndex));
    whiteInfo.add(" / ");
    whiteInfo.add(whiteKelvin(whiteIndex));
    whiteInfo.add(" K");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};