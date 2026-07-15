#include "ButtonManager.h"

// Timing constants
constexpr unsigned long DEBOUNCE_DELAY_MS = 50;
constexpr unsigned long LONG_PRESS_MS = 2000;
constexpr unsigned long DOUBLE_PRESS_MS = 400;
constexpr unsigned long HOLD_REPEAT_MS = 250;

ButtonManager::ButtonManager(uint8_t pin, ButtonCallback cb)
    : _pin(pin), _cb(cb),
      _currentAdcRaw(4095), _currentAdcAvg(4095),
      _currentButton(ButtonId::NONE), _lastButtonRaw(ButtonId::NONE),
      _lastDebounceTime(0), _buttonPressTime(0),
      _buttonHandledLongPress(false), _isHolding(false), _lastHoldRepeat(0),
      _lastReleasedButton(ButtonId::NONE), _lastReleaseTime(0),
      _threshPlayPrev(100), _threshPrevNext(500), _threshNextVolDown(1500),
      _threshVolDownVolUp(2500), _threshVolUpNone(3500)
{
}

void ButtonManager::begin() {
    pinMode(_pin, INPUT);
    analogReadResolution(12);
}

void ButtonManager::configureLadder(float rPullup, float rPlay, float rPrev, float rNext, float rVolDown, float rVolUp) {
    // Calculate expected nominal ADC values (12-bit ADC -> 4095 max)
    float adcPlay    = 4095.0f * rPlay / (rPullup + rPlay);
    float adcPrev    = 4095.0f * rPrev / (rPullup + rPrev);
    float adcNext    = 4095.0f * rNext / (rPullup + rNext);
    float adcVolDown = 4095.0f * rVolDown / (rPullup + rVolDown);
    float adcVolUp   = 4095.0f * rVolUp / (rPullup + rVolUp);
    float adcNone    = 4095.0f; // Pullup only

    // Set thresholds halfway between nominal values
    _threshPlayPrev     = (int)((adcPlay + adcPrev) / 2.0f);
    _threshPrevNext     = (int)((adcPrev + adcNext) / 2.0f);
    _threshNextVolDown  = (int)((adcNext + adcVolDown) / 2.0f);
    _threshVolDownVolUp = (int)((adcVolDown + adcVolUp) / 2.0f);
    _threshVolUpNone    = (int)((adcVolUp + adcNone) / 2.0f);
    
    Serial.println("--- ButtonManager Thresholds ---");
    Serial.printf("PLAY_PREV: %d\n", _threshPlayPrev);
    Serial.printf("PREV_NEXT: %d\n", _threshPrevNext);
    Serial.printf("NEXT_VOLDOWN: %d\n", _threshNextVolDown);
    Serial.printf("VOLDOWN_VOLUP: %d\n", _threshVolDownVolUp);
    Serial.printf("VOLUP_NONE: %d\n", _threshVolUpNone);
    Serial.println("--------------------------------");
}

int ButtonManager::getAveragedAdc() {
    long sum = 0;
    int minVal = 4096;
    int maxVal = -1;
    
    // Take 34 samples, discard the highest and lowest, average the remaining 32
    for (int i = 0; i < 34; i++) {
        int val = analogRead(_pin);
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
        sum += val;
        delayMicroseconds(50);
    }
    sum = sum - minVal - maxVal;
    return sum / 32;
}

ButtonId ButtonManager::decodeAdc(int adcValue) {
    if (adcValue > _threshVolUpNone)    return ButtonId::NONE;
    if (adcValue <= _threshPlayPrev)    return ButtonId::PLAY_PAUSE;
    if (adcValue <= _threshPrevNext)    return ButtonId::PREVIOUS;
    if (adcValue <= _threshNextVolDown) return ButtonId::NEXT;
    if (adcValue <= _threshVolDownVolUp)return ButtonId::VOL_DOWN;
    return ButtonId::VOL_UP;
}

void ButtonManager::loop() {
    unsigned long now = millis();
    
    _currentAdcRaw = analogRead(_pin);
    _currentAdcAvg = getAveragedAdc();
    
    ButtonId readButton = decodeAdc(_currentAdcAvg);
    
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
