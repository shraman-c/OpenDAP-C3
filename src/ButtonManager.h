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
    
    // Call this in setup to map the ladder
    void configureLadder(float rPullup, float rPlay, float rPrev, float rNext, float rVolDown, float rVolUp);

    // Debug access
    int getCurrentRawAdc() const { return _currentAdcRaw; }
    int getCurrentAvgAdc() const { return _currentAdcAvg; }
    ButtonId getCurrentButton() const { return _currentButton; }

private:
    uint8_t _pin;
    ButtonCallback _cb;
    
    int _currentAdcRaw;
    int _currentAdcAvg;
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

    // Thresholds
    int _threshPlayPrev;
    int _threshPrevNext;
    int _threshNextVolDown;
    int _threshVolDownVolUp;
    int _threshVolUpNone;

    int getAveragedAdc();
    ButtonId decodeAdc(int adcValue);
};
