#pragma once

#include "wled.h"

// TouchButton: reusable slow-gesture variant for an external capacitive sensor.
// GPIO33 is read exactly like SmoothButton: active LOW with INPUT_PULLUP,
// independent of any saved WLED button configuration.
//
// Gestures:
// - first short touch: ON/OFF starts immediately on release (normal WLED fade)
// - second short touch started within 1 s: restore pre-first-touch power state + next white preset
// - hold: brightness 60 -> 20 -> 5 -> 60 ... (raw WLED bri values)
class TouchButton : public Usermod {
private:
  static constexpr uint8_t TOUCH_PIN = 33;
  static constexpr uint16_t DEBOUNCE_MS = 50;
  static constexpr uint16_t DOUBLE_CLICK_MS = 1000;
  static constexpr uint16_t LONG_PRESS_MS = 600;
  static constexpr uint16_t HOLD_REPEAT_MS = 1000;
  static constexpr uint16_t PERSIST_DELAY_MS = 1800;
  static constexpr uint8_t WHITE_PRESET_COUNT = 4;

  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t rawChangedMs = 0;
  uint32_t pressStartedMs = 0;
  uint32_t lastHoldStepMs = 0;
  bool longHandled = false;

  bool clickPending = false;
  bool secondClickArmed = false;
  uint32_t firstReleaseMs = 0;

  uint8_t lastBrightness = 60;
  uint8_t whiteIndex = 2;
  bool configLoaded = false;
  bool persistPending = false;
  uint32_t persistAfterMs = 0;
  bool initialized = false;

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
        (uint32_t)(dr * dr) + (uint32_t)(dg * dg) +
        (uint32_t)(db * db) + (uint32_t)(dw * dw) +
        (uint32_t)(dc * dc);

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

  void toggleSmooth()
  {
    if (bri == 0) {
      briLast = lastBrightness;
      setRememberedWhiteRaw();
      toggleOnOff();
    } else {
      lastBrightness = bri;
      briLast = lastBrightness;
      toggleOnOff();
      schedulePersist();
    }

    // Keep WLED's normal transition active. If another toggle happens while the
    // fade is running, WLED starts the new transition from the current briT.
    stateUpdated(CALL_MODE_BUTTON);
  }

  uint8_t nextBrightnessStep(uint8_t current) const
  {
    if (current > 60) return 60;
    if (current > 20) return 20;
    if (current > 5) return 5;
    return 60;
  }

  void stepBrightness()
  {
    const uint8_t base = (bri > 0) ? bri : lastBrightness;
    lastBrightness = nextBrightnessStep(base);

    if (bri == 0) {
      bri = lastBrightness;
      briLast = lastBrightness;
      setRememberedWhiteRaw();
      strip.restartRuntime();
    } else {
      bri = lastBrightness;
      briLast = lastBrightness;
    }

    immediateStateUpdate(CALL_MODE_BUTTON);
    schedulePersist();
  }

  // Tap #1 already changed power immediately. A valid tap #2 toggles power once
  // more, restoring the state that existed before tap #1, while changing the
  // white preset. If a fade is in progress WLED naturally reverses it from briT.
  void completeDoubleClick()
  {
    clickPending = false;
    secondClickArmed = false;

    whiteIndex = (whiteIndex + 1) % WHITE_PRESET_COUNT;
    setRememberedWhiteRaw();
    toggleSmooth();
    schedulePersist();
  }

  void completeFirstClick()
  {
    toggleSmooth();
    clickPending = true;
    firstReleaseMs = millis();
  }

  bool readTouchPressed() const
  {
    return digitalRead(TOUCH_PIN) == LOW;
  }

  void processTouchSample(bool pressed)
  {
    const uint32_t now = millis();

    if (pressed != rawPressed) {
      rawPressed = pressed;
      rawChangedMs = now;
    }

    if ((now - rawChangedMs) < DEBOUNCE_MS || stablePressed == rawPressed) return;

    stablePressed = rawPressed;

    if (stablePressed) {
      pressStartedMs = now;
      lastHoldStepMs = now;
      longHandled = false;

      // Measure the 1 s window to the BEGINNING of tap #2. This is much more
      // forgiving for a slow hand movement over a capacitive sensor.
      secondClickArmed = clickPending && (now - firstReleaseMs <= DOUBLE_CLICK_MS);
      return;
    }

    if (longHandled) {
      secondClickArmed = false;
      return;
    }

    if (secondClickArmed) completeDoubleClick();
    else completeFirstClick();
  }

  void handleTimers()
  {
    const uint32_t now = millis();

    if (stablePressed) {
      if (!longHandled && (now - pressStartedMs >= LONG_PRESS_MS)) {
        longHandled = true;
        clickPending = false;
        secondClickArmed = false;
        stepBrightness();
        lastHoldStepMs = now;
      } else if (longHandled && (now - lastHoldStepMs >= HOLD_REPEAT_MS)) {
        stepBrightness();
        lastHoldStepMs = now;
      }
    }

    // The single-click action has already happened, so expiry only closes the
    // double-click opportunity; there is no 1 s visible delay.
    if (clickPending && !stablePressed && (now - firstReleaseMs > DOUBLE_CLICK_MS)) {
      clickPending = false;
      secondClickArmed = false;
    }

    if (persistPending && (int32_t)(now - persistAfterMs) >= 0 && !strip.isUpdating()) {
      persistPending = false;
      serializeConfig();
    }
  }

public:
  void setup() override
  {
    pinMode(TOUCH_PIN, INPUT_PULLUP);

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
    setRememberedWhiteRaw();

    rawPressed = readTouchPressed();
    stablePressed = rawPressed;
    rawChangedMs = millis();
    initialized = true;

    immediateStateUpdate(CALL_MODE_NO_NOTIFY);
  }

  void loop() override
  {
    if (!initialized) return;
    processTouchSample(readTouchPressed());
    handleTimers();
  }

  bool handleButton(uint8_t b) override
  {
    // TouchButton polls GPIO33 itself. Consume WLED Button 0 if an older saved
    // configuration still exists, preventing duplicate button actions.
    return b == 0;
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
    JsonObject top = root.createNestedObject("TouchButton");
    top["lastBrightness"] = lastBrightness;
    top["whiteIndex"] = whiteIndex;
  }

  bool readFromConfig(JsonObject& root) override
  {
    JsonObject top = root["TouchButton"];

    // A controller previously running Flow/SmoothButton can inherit its saved
    // brightness and color when moved to this variant.
    if (top.isNull()) top = root["FlowButton"];

    if (top.isNull()) {
      configLoaded = false;
      return false;
    }

    bool complete = true;
    complete &= getJsonValue(top["lastBrightness"], lastBrightness, (uint8_t)60);
    complete &= getJsonValue(top["whiteIndex"], whiteIndex, (uint8_t)2);

    if (lastBrightness < 5) lastBrightness = 5;
    if (whiteIndex >= WHITE_PRESET_COUNT) whiteIndex = WHITE_PRESET_COUNT - 1;

    configLoaded = true;
    return complete;
  }

  void addToJsonInfo(JsonObject& root) override
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray stateInfo = user.createNestedArray("TouchButton");
    stateInfo.add(bri > 0 ? "ON" : "OFF");

    JsonArray windowInfo = user.createNestedArray("Touch double window");
    windowInfo.add(DOUBLE_CLICK_MS);
    windowInfo.add(" ms");

    JsonArray briInfo = user.createNestedArray("Touch brightness");
    briInfo.add(lastBrightness);
    briInfo.add(" / 255");

    JsonArray whiteInfo = user.createNestedArray("Touch white");
    whiteInfo.add(whiteName(whiteIndex));
    whiteInfo.add(" / CCT ");
    whiteInfo.add(whiteCct(whiteIndex));
    whiteInfo.add("/255");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};
