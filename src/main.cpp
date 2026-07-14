#include <Arduino.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Preferences.h>

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================
constexpr char VERSION[] = "v1.0.0";

// Pins
constexpr uint8_t PIN_ADC = 1;
constexpr uint8_t PIN_UART_TX = 4;
constexpr uint8_t PIN_UART_RX = 5;

// ADC Button Thresholds (Configurable, contiguous to prevent gaps)
constexpr int ADC_PLAY_MAX = 1135;
constexpr int ADC_NEXT_MAX = 1295;
constexpr int ADC_PREV_MAX = 1705;
constexpr int ADC_VOLDOWN_MAX = 2345;
constexpr int ADC_VOLUP_MAX = 3400;
// Anything > 3400 is NO_BUTTON

// Timing constants
constexpr unsigned long DEBOUNCE_DELAY_MS = 50;
constexpr unsigned long LONG_PRESS_MS = 2000;
constexpr unsigned long DOUBLE_PRESS_MS = 400;
constexpr unsigned long HOLD_REPEAT_MS = 250;

// ============================================================================
// ENUMS
// ============================================================================
enum class ButtonId {
    NONE,
    PLAY_PAUSE,
    NEXT,
    PREVIOUS,
    VOL_UP,
    VOL_DOWN,
    UNKNOWN
};

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
    DOUBLE_PRESS,
    HOLD
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini dfPlayer;
Preferences preferences;

// State Variables
int currentVolume = 15;
int currentSong = 1;
bool isPlaying = false;
bool dfPlayerOnline = false;

// Button state tracking
ButtonId lastButtonRaw = ButtonId::NONE;
ButtonId currentButton = ButtonId::NONE;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressTime = 0;
bool buttonHandledLongPress = false;
bool isHolding = false;
unsigned long lastHoldRepeat = 0;

// Double press tracking
ButtonId lastReleasedButton = ButtonId::NONE;
unsigned long lastReleaseTime = 0;

// ADC
int currentAdcRaw = 4095;
int currentAdcAvg = 4095;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void initAudio();
void initPreferences();
void saveState();
void enterDeepSleep();
void processConsole();
void processAudioEvents();
ButtonId decodeAdc(int adcValue);
int getAveragedAdc();
void handleButtonEvent(ButtonId btn, ButtonEvent event);

// ============================================================================
// AUDIO SUBSYSTEM
// ============================================================================
void initAudio() {
    dfSerial.begin(9600, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
    Serial.println("Initializing DFPlayer Mini...");
    
    // Give it some time to boot
    delay(1000); 

    if (!dfPlayer.begin(dfSerial, /*isACK*/ true, /*doReset*/ false)) {
        Serial.println("Error: DFPlayer Mini not found or SD card error!");
        dfPlayerOnline = false;
    } else {
        Serial.println("DFPlayer Mini initialized.");
        dfPlayerOnline = true;
        
        dfPlayer.volume(currentVolume);
        dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
        
        // Autoplay if we had a valid song
        if (currentSong > 0) {
            Serial.printf("Resuming song: %d\n", currentSong);
            dfPlayer.play(currentSong);
            isPlaying = true;
        }
    }
}

void processAudioEvents() {
    if (dfPlayer.available()) {
        uint8_t type = dfPlayer.readType();
        int value = dfPlayer.read();
        
        if (type == DFPlayerPlayFinished) {
            Serial.printf("Song %d finished.\n", value);
            currentSong = value + 1; // Simplistic approach, normally query total files
            dfPlayer.play(currentSong);
        } else if (type == DFPlayerError) {
            Serial.printf("DFPlayer Error: %d\n", value);
        }
    }
}

// ============================================================================
// PREFERENCES SUBSYSTEM
// ============================================================================
void initPreferences() {
    preferences.begin("opendap", false);
    currentVolume = preferences.getInt("volume", 15);
    currentSong = preferences.getInt("song", 1);
    Serial.printf("Loaded config - Volume: %d, Song: %d\n", currentVolume, currentSong);
}

void saveState() {
    preferences.putInt("volume", currentVolume);
    preferences.putInt("song", currentSong);
    Serial.println("State saved to Preferences.");
}

// ============================================================================
// POWER SUBSYSTEM
// ============================================================================
void enterDeepSleep() {
    Serial.println("Preparing for Deep Sleep...");
    if (isPlaying) {
        dfPlayer.pause();
        isPlaying = false;
    }
    saveState();
    
    // Allow DFPlayer to finish pause
    delay(100); 
    
    Serial.println("Entering Deep Sleep now.");
    Serial.flush();
    
    // Configure wake up on GPIO1 going low (requires proper pull-up circuit)
    // Note: ESP32-C3 can wake from deep sleep using RTC GPIOs
    // GPIO1 is an RTC GPIO.
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_ADC, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    esp_deep_sleep_start();
}

// ============================================================================
// BUTTON SUBSYSTEM
// ============================================================================
int getAveragedAdc() {
    long sum = 0;
    int minVal = 4096;
    int maxVal = -1;
    
    // Take 34 samples, discard the highest and lowest, average the remaining 32
    for (int i = 0; i < 34; i++) {
        int val = analogRead(PIN_ADC);
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
        sum += val;
        delayMicroseconds(50);
    }
    sum = sum - minVal - maxVal;
    return sum / 32;
}

ButtonId decodeAdc(int adcValue) {
    if (adcValue > ADC_VOLUP_MAX) return ButtonId::NONE;
    if (adcValue <= ADC_PLAY_MAX) return ButtonId::PLAY_PAUSE;
    if (adcValue <= ADC_NEXT_MAX) return ButtonId::NEXT;
    if (adcValue <= ADC_PREV_MAX) return ButtonId::PREVIOUS;
    if (adcValue <= ADC_VOLDOWN_MAX) return ButtonId::VOL_DOWN;
    return ButtonId::VOL_UP;
}

void handleButtonEvent(ButtonId btn, ButtonEvent event) {
    if (btn == ButtonId::PLAY_PAUSE) {
        if (event == ButtonEvent::SHORT_PRESS) {
            if (isPlaying) {
                dfPlayer.pause();
                isPlaying = false;
                Serial.println("Paused");
            } else {
                dfPlayer.start();
                isPlaying = true;
                Serial.println("Playing");
            }
        } else if (event == ButtonEvent::LONG_PRESS) {
            enterDeepSleep();
        }
    } 
    else if (btn == ButtonId::NEXT) {
        if (event == ButtonEvent::SHORT_PRESS) {
            dfPlayer.next();
            currentSong++;
            isPlaying = true;
            Serial.println("Next Song");
        } else if (event == ButtonEvent::DOUBLE_PRESS) {
            // Next Folder - pseudo implementation as DFPlayer needs folder ID
            Serial.println("Next Folder (Placeholder)");
        }
    }
    else if (btn == ButtonId::PREVIOUS) {
        if (event == ButtonEvent::SHORT_PRESS) {
            dfPlayer.previous();
            if (currentSong > 1) currentSong--;
            isPlaying = true;
            Serial.println("Previous Song");
        } else if (event == ButtonEvent::DOUBLE_PRESS) {
            Serial.println("Previous Folder (Placeholder)");
        }
    }
    else if (btn == ButtonId::VOL_UP) {
        if (event == ButtonEvent::SHORT_PRESS || event == ButtonEvent::HOLD) {
            if (currentVolume < 30) {
                currentVolume++;
                dfPlayer.volume(currentVolume);
                Serial.printf("Volume Up: %d\n", currentVolume);
            }
        }
    }
    else if (btn == ButtonId::VOL_DOWN) {
        if (event == ButtonEvent::SHORT_PRESS || event == ButtonEvent::HOLD) {
            if (currentVolume > 0) {
                currentVolume--;
                dfPlayer.volume(currentVolume);
                Serial.printf("Volume Down: %d\n", currentVolume);
            }
        }
    }
}

void processButtons() {
    unsigned long now = millis();
    
    currentAdcRaw = analogRead(PIN_ADC);
    currentAdcAvg = getAveragedAdc();
    
    ButtonId readButton = decodeAdc(currentAdcAvg);
    
    // Debounce Logic
    if (readButton != lastButtonRaw) {
        lastDebounceTime = now;
        lastButtonRaw = readButton;
    }
    
    if ((now - lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        if (readButton != currentButton) {
            // Button state changed
            if (readButton != ButtonId::NONE) {
                // Button Pressed
                buttonPressTime = now;
                buttonHandledLongPress = false;
                isHolding = false;
                
                // Immediately trigger short press for volume buttons for responsiveness
                if (readButton == ButtonId::VOL_UP || readButton == ButtonId::VOL_DOWN) {
                    handleButtonEvent(readButton, ButtonEvent::SHORT_PRESS);
                    buttonHandledLongPress = true; // Prevent triggering on release
                    isHolding = true;
                    lastHoldRepeat = now + 500; // Wait 500ms before repeating
                }
            } else {
                // Button Released
                if (!buttonHandledLongPress && currentButton != ButtonId::NONE) {
                    // Check for double press
                    if ((currentButton == ButtonId::NEXT || currentButton == ButtonId::PREVIOUS) && 
                        currentButton == lastReleasedButton && (now - lastReleaseTime) < DOUBLE_PRESS_MS) {
                        handleButtonEvent(currentButton, ButtonEvent::DOUBLE_PRESS);
                        lastReleasedButton = ButtonId::NONE; // Reset
                    } else {
                        // Register short press
                        handleButtonEvent(currentButton, ButtonEvent::SHORT_PRESS);
                        lastReleasedButton = currentButton;
                        lastReleaseTime = now;
                    }
                }
            }
            currentButton = readButton;
        } else if (currentButton != ButtonId::NONE) {
            // Button is being held
            if (currentButton == ButtonId::PLAY_PAUSE && !buttonHandledLongPress && (now - buttonPressTime) > LONG_PRESS_MS) {
                handleButtonEvent(currentButton, ButtonEvent::LONG_PRESS);
                buttonHandledLongPress = true;
            }
            
            // Continuous hold action for volume
            if ((currentButton == ButtonId::VOL_UP || currentButton == ButtonId::VOL_DOWN) && isHolding) {
                if (now >= lastHoldRepeat) {
                    handleButtonEvent(currentButton, ButtonEvent::HOLD);
                    lastHoldRepeat = now + HOLD_REPEAT_MS;
                }
            }
        }
    }
}

// ============================================================================
// DEBUG SUBSYSTEM
// ============================================================================
void processConsole() {
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "help") {
            Serial.println("Commands: help, status, button, song, volume, play, pause, next, prev, sleep");
        } else if (cmd == "status") {
            Serial.printf("Version: %s\n", VERSION);
            Serial.printf("Board: ESP32-C3 Super Mini\n");
            Serial.printf("Heap: %lu\n", (unsigned long)ESP.getFreeHeap());
            Serial.printf("DFPlayer Online: %s\n", dfPlayerOnline ? "YES" : "NO");
            Serial.printf("Current Song: %d\n", currentSong);
            Serial.printf("Current Volume: %d\n", currentVolume);
            Serial.printf("State: %s\n", isPlaying ? "PLAYING" : "PAUSED");
        } else if (cmd == "button") {
            Serial.printf("ADC_RAW = %d | ADC_AVG = %d | BUTTON = %d\n", currentAdcRaw, currentAdcAvg, static_cast<int>(currentButton));
        } else if (cmd == "play") {
            dfPlayer.start();
            isPlaying = true;
            Serial.println("Playing");
        } else if (cmd == "pause") {
            dfPlayer.pause();
            isPlaying = false;
            Serial.println("Paused");
        } else if (cmd == "next") {
            handleButtonEvent(ButtonId::NEXT, ButtonEvent::SHORT_PRESS);
        } else if (cmd == "prev") {
            handleButtonEvent(ButtonId::PREVIOUS, ButtonEvent::SHORT_PRESS);
        } else if (cmd == "sleep") {
            enterDeepSleep();
        }
    }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500); // Wait for Serial
    
    Serial.println("\n==================================");
    Serial.printf("OpenDAP-C3 %s\n", VERSION);
    Serial.println("==================================");
    
    analogReadResolution(12);
    pinMode(PIN_ADC, INPUT);
    
    initPreferences();
    initAudio();
}

void loop() {
    processButtons();
    processAudioEvents();
    processConsole();
    
    // Very short delay to prevent watchdog starvation
    delay(2);
}