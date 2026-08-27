#pragma once

#include "wled.h"

// One usermod, two build variants:
// - FlowButton: original 97-LED directional wipe
// - SmoothButton (FLOWBUTTON_NO_WIPE): normal WLED ON/OFF fade, no LED-count dependency
class FlowButton : public Usermod {
private:
  static constexpr uint16_t FLOW_LED_COUNT = 97;
  static constexpr uint32_t FLOW_DURATION_MS = 2500;

  static constexpr uint16_t DEBOUNCE_MS = 50;
  static constexpr uint16_t DOUBLE_CLICK_MS = 600;
  static constexpr uint16_t LONG_PRESS_MS = 600;
  static constexpr uint16_t HOLD_REPEAT_MS = 1000;
  static constexpr uint16_t PERSIST_DELAY_MS = 1800;
  static constexpr uint8_t WHITE_PRESET_COUNT = 4;

  // Wipe state (used only by the FlowButton build).
  float progress = 0.0f;
  int8_t direction = 0;
  uint32_t lastAnimMs = 0;

  // Button/debounce/gesture state.
  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t rawChangedMs = 0;
  uint32_t pressStartedMs = 0;
  uint32_t lastHoldStepMs = 0;
  bool longHandled = false;

  bool clickPending = false;
  uint32_t firstReleaseMs = 0;

  // Remembered user settings.
  uint8_t lastBrightness = 60;
  uint8_t whiteIndex = 2;
  bool configLoaded = false;
  bool persistPending = false;
  uint32_t persistAfterMs = 0;

  bool initialized = false;

  // Four presets reconstructed from the user's WLED screenshots.
  // RGB = wheel position, W = white slider, CCT = WLED 0..255 balance slider.
  uint8_t whiteCct(uint8_t index) const
  {
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return 0;
      case 1: return 0;
      case 2: return 64;
      default: return 180;
    }
  }

  const char* whiteName(uint8_t index) const
  {
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return "Red warm";
      case 1: return "Amber warm";
      case 2: return "Warm white";
      default: return "Cool white";
    }
  }

  uint32_t whiteColor(uint8_t index) const
  {
    switch (index % WHITE_PRESET_COUNT) {
      case 0: return RGBW32(255, 27, 26, 255);
      case 1: return RGBW32(255, 146, 28, 255);
      case 2: return RGBW32(255, 213, 176, 255);
      default: return RGBW32(231, 234, 255, 255);
    }
  }

  uint8_t nearestWhiteIndexFromState() const
  {
    const Segment& seg = strip.getMainSegment();
    const uint32_t current = seg.colors[0];
    const int currentCct = seg.cct;

    uint8_t best = 0;
    uint32_t bestScore = 0xFFFFFFFFUL;

    for (uint8_t i = 0; i < WHITE_PRESET_COUNT; i++) {
      const uint32_t target = whiteColor(i);
      const int dr = (int)R(current) - (int)R(target);
      const int dg = (int)G(current) - (int)G(target);
      const int db = (int)B(current) - (int)B(target);
      const int dw = (int)W(current) - (int)W(target);
      const int dc = currentCct - (int)whiteCct(i);

      const uint32_t score =
        (uint32_t)(dr*dr) + (uint32_t)(dg*dg) +
        (uint32_t)(db*db) + (uint32_t)(dw*dw) +
        (uint32_t)(dc*dc);

      if (score < bestScore) {
        bestScore = score;
        best = i;
      }
    }
    return best;
  }

  void schedulePersist()
  {
    persistPending = true;
    persistAfterMs = millis() + PERSIST_DELAY_MS;
  }

  void immediateStateUpdate(uint8_t callMode)
  {
    const bool oldFadeTransition = fadeTransition;
    fadeTransition = false;
    stateChanged = true;
    stateUpdated(callMode);
    fadeTransition = oldFadeTransition;
    strip.trigger();
  }

  // Set preset values without causing a state update. This is useful for the
  // SmoothButton ON path: color can be prepared while output is still at 0,
  // then WLED performs its normal brightness fade from OFF to ON.
  void setRememberedWhiteRaw()
  {
    const uint8_t cct = whiteCct(whiteIndex);
    const uint32_t color = whiteColor(whiteIndex);

    for (size_t s = 0; s < strip.getSegmentsNum(); s++) {
      Segment& seg = strip.getSegment(s);
      if (!seg.isActive()) continue;
      seg.setColor(0, color);
      seg.setCCT(cct);
    }
    stateChanged = true;
  }

  void applyRememberedWhite()
  {
    setRememberedWhiteRaw();
    immediateStateUpdate(CALL_MODE_BUTTON);
  }

#ifndef FLOWBUTTON_NO_WIPE
  void startOrReverse()
  {
    const uint32_t now = millis();

    if (direction != 0) {
      direction = -direction;
      lastAnimMs = now;
      strip.trigger();
      return;
    }

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

    strip.trigger();
  }
#else
  // SmoothButton single click: use WLED's native brightness transition.
  // No per-pixel overlay, so it works with any strip length/bus arrangement.
  void toggleSmooth()
  {
    if (bri == 0) {
      briLast = lastBrightness;
      setRememberedWhiteRaw();
      toggleOnOff();               // restores briLast and restarts effect runtime
    } else {
      lastBrightness = bri;
      briLast = lastBrightness;
      toggleOnOff();               // stores current bri and sets target to 0
      schedulePersist();
    }

    // Do NOT disable fadeTransition here. stateUpdated() uses WLED's normal
    // configured transition time, giving the standard smooth ON/OFF fade.
    stateUpdated(CALL_MODE_BUTTON);
  }
#endif

  uint8_t nextBrightnessStep(uint8_t current) const
  {
    // Raw WLED bri values, not percentages.
    // Closed loop: >60 -> 60 -> 20 -> 5 -> 60 -> ...
    if (current > 60) return 60;
    if (current > 20) return 20;
    if (current > 5)  return 5;
    return 60;
  }

  void stepBrightness()
  {
    const uint8_t base = (bri > 0) ? bri : lastBrightness;
    lastBrightness = nextBrightnessStep(base);

    if (bri == 0) {
#ifndef FLOWBUTTON_NO_WIPE
      progress = (float)FLOW_LED_COUNT;
      direction = 0;
#endif
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

    if (stablePressed) {
      if (!longHandled && (now - pressStartedMs >= LONG_PRESS_MS)) {
        longHandled = true;
        clickPending = false;
        stepBrightness();
        lastHoldStepMs = now;
      } else if (longHandled && (now - lastHoldStepMs >= HOLD_REPEAT_MS)) {
        stepBrightness();
        lastHoldStepMs = now;
      }
    }

    if (clickPending && !stablePressed && (now - firstReleaseMs > DOUBLE_CLICK_MS)) {
      clickPending = false;
#ifndef FLOWBUTTON_NO_WIPE
      startOrReverse();
#else
      toggleSmooth();
#endif
    }

    if (persistPending && (int32_t)(now - persistAfterMs) >= 0 && !strip.isUpdating()) {
      persistPending = false;
      serializeConfig();
    }
  }

public:
  void setup() override
  {
    if (!configLoaded) {
      uint8_t current = (bri > 0) ? bri : briLast;
      if (current == 0) current = 60;
      lastBrightness = current;
      whiteIndex = nearestWhiteIndexFromState();
      schedulePersist();
    }

    if (lastBrightness < 5) lastBrightness = 5;

    briLast = lastBrightness;
    if (bri > 0) bri = lastBrightness;
    applyRememberedWhite();

#ifndef FLOWBUTTON_NO_WIPE
    progress = (bri > 0) ? (float)FLOW_LED_COUNT : 0.0f;
#endif
    rawPressed = false;
    stablePressed = false;
    rawChangedMs = millis();
    initialized = true;

    immediateStateUpdate(CALL_MODE_NO_NOTIFY);
  }

  void loop() override
  {
    if (!initialized) return;
#ifndef FLOWBUTTON_NO_WIPE
    updateAnimation();
#endif
    handleGestureTimers();
  }

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
      } else if (!longHandled) {
        if (clickPending && (now - firstReleaseMs <= DOUBLE_CLICK_MS)) {
          clickPending = false;
          cycleWhite();
        } else {
          clickPending = true;
          firstReleaseMs = now;
        }
      }
    }

    return true;
  }

  void handleOverlayDraw() override
  {
#ifndef FLOWBUTTON_NO_WIPE
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
#endif
  }

  void onStateChange(uint8_t mode) override
  {
    if (!initialized) return;

    bool changed = false;
    if (bri > 0 && bri != lastBrightness) {
      lastBrightness = bri;
      changed = true;
    }

    const uint8_t nearest = nearestWhiteIndexFromState();
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
    complete &= getJsonValue(top["lastBrightness"], lastBrightness, (uint8_t)60);

    if (!getJsonValue(top["whiteIndex"], whiteIndex)) {
      complete &= getJsonValue(top["cctIndex"], whiteIndex, (uint8_t)2);
    }

    if (lastBrightness < 5) lastBrightness = 5;
    if (whiteIndex >= WHITE_PRESET_COUNT) whiteIndex = WHITE_PRESET_COUNT - 1;

    configLoaded = true;
    return complete;
  }

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

#ifdef FLOWBUTTON_NO_WIPE
    JsonArray stateInfo = user.createNestedArray("SmoothButton");
    stateInfo.add(bri > 0 ? "ON" : "OFF");
#else
    JsonArray stateInfo = user.createNestedArray("FlowButton");
    stateInfo.add(direction > 0 ? "Wipe ON" : direction < 0 ? "Wipe OFF" : progress > 0.0f ? "ON" : "OFF");
#endif

    JsonArray briInfo = user.createNestedArray("Button brightness");
    briInfo.add(lastBrightness);
    briInfo.add(" / 255");

    JsonArray whiteInfo = user.createNestedArray("Button white");
    whiteInfo.add(whiteName(whiteIndex));
    whiteInfo.add(" / CCT ");
    whiteInfo.add(whiteCct(whiteIndex));
    whiteInfo.add("/255");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
