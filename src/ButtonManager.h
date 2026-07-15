#pragma once
#include <Arduino.h>
#include <functional>

enum class ButtonId {
    NONE,
    PLAY_PAUSE,
    PREVIOUS,
    NEXT,
    VOL_DOWN,
    VOL_UP,
    UNKNOWN
};

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
    DOUBLE_PRESS,
    HOLD
};

using ButtonCallback = std::function<void(ButtonId, ButtonEvent)>;

class ButtonManager {
public:
    ButtonManager(uint8_t pin, ButtonCallback cb);
    void begin();
    void loop();
    
    // Call this in setup to map the ladder as parallel
    void configureLadder(float rPullup, float rPlay, float rPrev, float rNext, float rVolDown, float rVolUp);

    // Call this in setup to map the ladder as series (accumulating)
    void configureLadderSeries(float rPullup, float rPlay, float rPrev, float rNext, float rVolDown, float rVolUp);

    // Override the expected millivolts for a specific button (e.g. from EEPROM)
    void setExpectedMv(ButtonId btn, int mv);

    // Debug access
    int getCurrentMvRaw() const { return _currentMvRaw; }
    int getCurrentMvAvg() const { return _currentMvAvg; }
    ButtonId getCurrentButton() const { return _currentButton; }

private:
    uint8_t _pin;
    ButtonCallback _cb;
    
    int _currentMvRaw;
    int _currentMvAvg;
    ButtonId _currentButton;
    ButtonId _lastButtonRaw;
    
    // State
    unsigned long _lastDebounceTime;
    unsigned long _buttonPressTime;
    bool _buttonHandledLongPress;
    bool _isHolding;
    unsigned long _lastHoldRepeat;
    ButtonId _lastReleasedButton;
    unsigned long _lastReleaseTime;

    // Expected mV values
    int _mvPlay;
    int _mvPrev;
    int _mvNext;
    int _mvVolDown;
    int _mvVolUp;

    int getAveragedMilliVolts();
    ButtonId decodeMilliVolts(int mv);
};
