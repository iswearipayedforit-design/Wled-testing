#pragma once

#include "wled.h"

// FlowButton: 97-LED directional wipe with immediate single-click response.
// Logical LED order is handled by WLED bus configuration:
// 0..36  = GPIO2 bus (Reversed)
// 37..96 = GPIO16 bus (normal)
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

  float progress = 0.0f;  // visible LEDs, 0..97
  int8_t direction = 0;   // +1 = wipe ON, -1 = wipe OFF, 0 = idle
  uint32_t lastAnimMs = 0;

  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t rawChangedMs = 0;
  uint32_t pressStartedMs = 0;
  uint32_t lastHoldStepMs = 0;
  bool longHandled = false;

  // A first short click acts immediately. This state only keeps the 600 ms
  // opportunity open for a second click.
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

  void applyRememberedWhite()
  {
    setRememberedWhiteRaw();
    immediateStateUpdate(CALL_MODE_BUTTON);
  }

  void startOrReverse()
  {
    const uint32_t now = millis();

    // If a wipe is already running, reverse from the exact current position.
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

    // Force normal frame-rate rendering so Solid does not update the overlay
    // only at its much slower native interval.
    strip.trigger();
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
      // A hold is a direct brightness command, not a wipe command.
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

  void completeFirstClick()
  {
    // No 600 ms wait: start/reverse the wipe immediately on release.
    startOrReverse();
    clickPending = true;
    firstReleaseMs = millis();
  }

  void completeDoubleClick()
  {
    clickPending = false;
    secondClickArmed = false;

    // The first click already started a wipe. Change color and reverse that wipe
    // so the final power state is the same as before the first click.
    cycleWhite();
    startOrReverse();
  }

  void processButtonSample(bool pressed)
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

      // Count the double-click window to the START of click #2. This makes the
      // gesture easier while preserving immediate response to click #1.
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

  void handleGestureTimers()
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

    // Single-click action already happened immediately. Expiry only closes the
    // chance for a second click; no visible action is delayed here.
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

  bool handleButton(uint8_t b) override
  {
    if (!initialized || b != 0 || btnPin[b] < 0 || buttonType[b] == BTN_TYPE_NONE) return false;
    processButtonSample(isButtonPressed(b));
    return true;
  }

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

    JsonArray stateInfo = user.createNestedArray("FlowButton");
    stateInfo.add(direction > 0 ? "Wipe ON" : direction < 0 ? "Wipe OFF" : progress > 0.0f ? "ON" : "OFF");

    JsonArray windowInfo = user.createNestedArray("Flow double window");
    windowInfo.add(DOUBLE_CLICK_MS);
    windowInfo.add(" ms");

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
