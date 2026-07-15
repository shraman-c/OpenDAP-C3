#include "ButtonManager.h"

// Timing constants
constexpr unsigned long DEBOUNCE_DELAY_MS = 50;
constexpr unsigned long LONG_PRESS_MS = 2000;
constexpr unsigned long DOUBLE_PRESS_MS = 400;
constexpr unsigned long HOLD_REPEAT_MS = 250;

ButtonManager::ButtonManager(uint8_t pin, ButtonCallback cb)
    : _pin(pin), _cb(cb),
      _currentMvRaw(3300), _currentMvAvg(3300),
      _currentButton(ButtonId::NONE), _lastButtonRaw(ButtonId::NONE),
      _lastDebounceTime(0), _buttonPressTime(0),
      _buttonHandledLongPress(false), _isHolding(false), _lastHoldRepeat(0),
      _lastReleasedButton(ButtonId::NONE), _lastReleaseTime(0),
      _mvPlay(15), _mvPrev(71), _mvNext(300),
      _mvVolDown(1055), _mvVolUp(1650)
{
}

void ButtonManager::begin() {
    pinMode(_pin, INPUT);
    analogReadResolution(12);
}

void ButtonManager::configureLadder(float rPullup, float rPlay, float rPrev, float rNext, float rVolDown, float rVolUp) {
    // Calculate expected nominal Millivolts (approximate V_ref = 3300mV)
    _mvPlay    = (int)(3300.0f * rPlay / (rPullup + rPlay));
    _mvPrev    = (int)(3300.0f * rPrev / (rPullup + rPrev));
    _mvNext    = (int)(3300.0f * rNext / (rPullup + rNext));
    _mvVolDown = (int)(3300.0f * rVolDown / (rPullup + rVolDown));
    _mvVolUp   = (int)(3300.0f * rVolUp / (rPullup + rVolUp));
    
    Serial.println("--- ButtonManager Expected mV ---");
    Serial.printf("PLAY: %d mV\n", _mvPlay);
    Serial.printf("PREV: %d mV\n", _mvPrev);
    Serial.printf("NEXT: %d mV\n", _mvNext);
    Serial.printf("VOLDOWN: %d mV\n", _mvVolDown);
    Serial.printf("VOLUP: %d mV\n", _mvVolUp);
    Serial.println("---------------------------------");
}

int ButtonManager::getAveragedMilliVolts() {
    long sum = 0;
    int minVal = 4000;
    int maxVal = -1;
    
    // Take 34 samples, discard the highest and lowest, average the remaining 32
    for (int i = 0; i < 34; i++) {
        int val = analogReadMilliVolts(_pin);
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
        sum += val;
        delayMicroseconds(50);
    }
    sum = sum - minVal - maxVal;
    return sum / 32;
}

ButtonId ButtonManager::decodeMilliVolts(int mv) {
    if (mv > 2500) return ButtonId::NONE; // Pullup only (~3300mV)

    int diffPlay = abs(mv - _mvPlay);
    int diffPrev = abs(mv - _mvPrev);
    int diffNext = abs(mv - _mvNext);
    int diffVolDown = abs(mv - _mvVolDown);
    int diffVolUp = abs(mv - _mvVolUp);

    int minDiff = diffPlay;
    ButtonId btn = ButtonId::PLAY_PAUSE;

    if (diffPrev < minDiff) { minDiff = diffPrev; btn = ButtonId::PREVIOUS; }
    if (diffNext < minDiff) { minDiff = diffNext; btn = ButtonId::NEXT; }
    if (diffVolDown < minDiff) { minDiff = diffVolDown; btn = ButtonId::VOL_DOWN; }
    if (diffVolUp < minDiff) { minDiff = diffVolUp; btn = ButtonId::VOL_UP; }

    // If the closest match is still wildly off, it's noise
    if (minDiff > 400) return ButtonId::NONE;

    return btn;
}

void ButtonManager::loop() {
    unsigned long now = millis();
    
    _currentMvRaw = analogReadMilliVolts(_pin);
    _currentMvAvg = getAveragedMilliVolts();
    
    ButtonId readButton = decodeMilliVolts(_currentMvAvg);
    
    // Debounce Logic
    if (readButton != _lastButtonRaw) {
        _lastDebounceTime = now;
        _lastButtonRaw = readButton;
    }
    
    if ((now - _lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (readButton != _currentButton) {
            // Button state changed
            if (readButton != ButtonId::NONE) {
                // Button Pressed
                _buttonPressTime = now;
                _buttonHandledLongPress = false;
                _isHolding = false;
                
                // Immediately trigger short press for volume buttons for responsiveness
                if (readButton == ButtonId::VOL_UP || readButton == ButtonId::VOL_DOWN) {
                    _cb(readButton, ButtonEvent::SHORT_PRESS);
                    _buttonHandledLongPress = true; // Prevent triggering on release
                    _isHolding = true;
                    _lastHoldRepeat = now + 500; // Wait 500ms before repeating
                }
            } else {
                // Button Released
                if (!_buttonHandledLongPress && _currentButton != ButtonId::NONE) {
                    // Check for double press
                    if ((_currentButton == ButtonId::NEXT || _currentButton == ButtonId::PREVIOUS) && 
                        _currentButton == _lastReleasedButton && (now - _lastReleaseTime) < DOUBLE_PRESS_MS) {
                        _cb(_currentButton, ButtonEvent::DOUBLE_PRESS);
                        _lastReleasedButton = ButtonId::NONE; // Reset
                    } else {
                        // Register short press
                        _cb(_currentButton, ButtonEvent::SHORT_PRESS);
                        _lastReleasedButton = _currentButton;
                        _lastReleaseTime = now;
                    }
                }
            }
            _currentButton = readButton;
        } else if (_currentButton != ButtonId::NONE) {
            // Button is being held
            if (_currentButton == ButtonId::PLAY_PAUSE && !_buttonHandledLongPress && (now - _buttonPressTime) > LONG_PRESS_MS) {
                _cb(_currentButton, ButtonEvent::LONG_PRESS);
                _buttonHandledLongPress = true;
            }
            
            // Continuous hold action for volume
            if ((_currentButton == ButtonId::VOL_UP || _currentButton == ButtonId::VOL_DOWN) && _isHolding) {
                if (now >= _lastHoldRepeat) {
                    _cb(_currentButton, ButtonEvent::HOLD);
                    _lastHoldRepeat = now + HOLD_REPEAT_MS;
                }
            }
        }
    }
}
