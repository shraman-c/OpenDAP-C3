#include <Arduino.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Preferences.h>

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================
constexpr char VERSION[] = "v1.0.0";

// Pins
constexpr uint8_t PIN_ADC = 2;
constexpr uint8_t PIN_UART_TX = 4;
constexpr uint8_t PIN_UART_RX = 5;

#include "ButtonManager.h"

// Forward declaration for ButtonManager callback
void handleButtonEvent(ButtonId btn, ButtonEvent event);

// Button Manager Instance
ButtonManager buttonManager(PIN_ADC, handleButtonEvent);

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

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void initAudio();
void printDFPlayerDiagnostics();
void initPreferences();
void saveState();
void enterDeepSleep();
void processConsole();
void processAudioEvents();

// ============================================================================
// AUDIO SUBSYSTEM
// ============================================================================
void printDFPlayerDiagnostics() {
    Serial.println("--- DFPlayer Diagnostics ---");
    int fileCount = dfPlayer.readFileCounts();
    Serial.printf("File Count: %d\n", fileCount);
    
    int volume = dfPlayer.readVolume();
    Serial.printf("Volume: %d\n", volume);
    
    int state = dfPlayer.readState();
    Serial.printf("State: %d\n", state);
    
    int currentFile = dfPlayer.readCurrentFileNumber();
    Serial.printf("Current File: %d\n", currentFile);
    
    Serial.println("----------------------------");
}

void initAudio() {
    dfSerial.setRxBufferSize(256);
    dfSerial.begin(9600, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
    Serial.println("Initializing DFPlayer Mini...");
    
    // Give it 2 seconds to boot before initialization
    delay(2000); 

    // Set timeout to prevent infinite blocking
    dfPlayer.setTimeOut(1000);

    // Using doReset=false because some hardware clones fail to respond to the software reset command properly
    if (!dfPlayer.begin(dfSerial, /*isACK*/ true, /*doReset*/ false)) {
        Serial.println("Error: DFPlayer Mini not found or communication error!");
        dfPlayerOnline = false;
    } else {
        Serial.println("DFPlayer Mini initialized.");
        
        // Allow time for SD card to mount after reset
        delay(1500);
        
        int fileCount = dfPlayer.readFileCounts();
        if (fileCount <= 0) {
            Serial.println("Error: No SD Card found or SD Card is empty!");
            dfPlayerOnline = false;
            return;
        }
        
        dfPlayerOnline = true;
        
        dfPlayer.volume(currentVolume);
        dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
        
        printDFPlayerDiagnostics();
        
        // Autoplay if we had a valid song
        if (currentSong > 0) {
            Serial.printf("Resuming song: %d\n", currentSong);
            dfPlayer.play(currentSong);
            delay(500); // Give it time to start
            
            int state = dfPlayer.readState();
            if (state != 1) { // 1 indicates playing
                Serial.println("Autoplay failed, retrying...");
                dfPlayer.play(currentSong);
                delay(500);
                state = dfPlayer.readState();
                if (state != 1) {
                    Serial.println("Error: Autoplay retry failed.");
                    isPlaying = false;
                } else {
                    isPlaying = true;
                }
            } else {
                isPlaying = true;
            }
        }
    }
}

void processAudioEvents() {
    if (!dfPlayerOnline) return;
    
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
    
    if (currentSong < 1) {
        currentSong = 1;
    }
    
    Serial.printf("Loaded config - Volume: %d, Song: %d\n", currentVolume, currentSong);
    
    // Load learned button voltages if they exist
    int learnedPlay = preferences.getInt("mvPlay", -1);
    if (learnedPlay != -1) buttonManager.setExpectedMv(ButtonId::PLAY_PAUSE, learnedPlay);
    
    int learnedPrev = preferences.getInt("mvPrev", -1);
    if (learnedPrev != -1) buttonManager.setExpectedMv(ButtonId::PREVIOUS, learnedPrev);
    
    int learnedNext = preferences.getInt("mvNext", -1);
    if (learnedNext != -1) buttonManager.setExpectedMv(ButtonId::NEXT, learnedNext);
    
    int learnedVolDown = preferences.getInt("mvVolDown", -1);
    if (learnedVolDown != -1) buttonManager.setExpectedMv(ButtonId::VOL_DOWN, learnedVolDown);
    
    int learnedVolUp = preferences.getInt("mvVolUp", -1);
    if (learnedVolUp != -1) buttonManager.setExpectedMv(ButtonId::VOL_UP, learnedVolUp);
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


void handleButtonEvent(ButtonId btn, ButtonEvent event) {
    if (!dfPlayerOnline) {
        if (btn != ButtonId::NONE && event == ButtonEvent::SHORT_PRESS) {
            Serial.println("Error: DFPlayer offline, ignoring button.");
        }
        return;
    }

    if (btn == ButtonId::PLAY_PAUSE) {
        if (event == ButtonEvent::SHORT_PRESS) {
            if (isPlaying) {
                dfPlayer.pause();
                isPlaying = false;
                Serial.println("Paused");
            } else {
                dfPlayer.play(currentSong); // Replaced start() for compatibility
                isPlaying = true;
                Serial.println("Playing");
            }
        } else if (event == ButtonEvent::LONG_PRESS) {
            enterDeepSleep();
        }
    } 
    else if (btn == ButtonId::NEXT) {
        if (event == ButtonEvent::SHORT_PRESS) {
            currentSong++;
            dfPlayer.play(currentSong); // Replaced next() for compatibility
            isPlaying = true;
            Serial.println("Next Song");
        } else if (event == ButtonEvent::DOUBLE_PRESS) {
            // Next Folder - pseudo implementation as DFPlayer needs folder ID
            Serial.println("Next Folder (Placeholder)");
        }
    }
    else if (btn == ButtonId::PREVIOUS) {
        if (event == ButtonEvent::SHORT_PRESS) {
            if (currentSong > 1) currentSong--;
            dfPlayer.play(currentSong); // Replaced previous() for compatibility
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



// ============================================================================
// DEBUG SUBSYSTEM
// ============================================================================
void processConsole() {
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "help") {
            Serial.println("Commands: help, status, button, learn <btn>, song, volume, play, pause, next, prev, sleep");
        } else if (cmd == "status") {
            Serial.printf("Version: %s\n", VERSION);
            Serial.printf("Board: ESP32-C3 Super Mini\n");
            Serial.printf("Heap: %lu\n", (unsigned long)ESP.getFreeHeap());
            Serial.printf("DFPlayer Online: %s\n", dfPlayerOnline ? "YES" : "NO");
            Serial.printf("Current Song: %d\n", currentSong);
            Serial.printf("Current Volume: %d\n", currentVolume);
            Serial.printf("State: %s\n", isPlaying ? "PLAYING" : "PAUSED");
        } else if (cmd == "button") {
            Serial.printf("mV_RAW = %d | mV_AVG = %d | BUTTON = %d\n", buttonManager.getCurrentMvRaw(), buttonManager.getCurrentMvAvg(), static_cast<int>(buttonManager.getCurrentButton()));
        } else if (cmd.startsWith("learn ")) {
            String btnStr = cmd.substring(6);
            btnStr.trim();
            int currentMv = buttonManager.getCurrentMvAvg();
            
            if (btnStr == "play") {
                preferences.putInt("mvPlay", currentMv);
                buttonManager.setExpectedMv(ButtonId::PLAY_PAUSE, currentMv);
                Serial.printf("Learned PLAY: %d mV\n", currentMv);
            } else if (btnStr == "prev") {
                preferences.putInt("mvPrev", currentMv);
                buttonManager.setExpectedMv(ButtonId::PREVIOUS, currentMv);
                Serial.printf("Learned PREV: %d mV\n", currentMv);
            } else if (btnStr == "next") {
                preferences.putInt("mvNext", currentMv);
                buttonManager.setExpectedMv(ButtonId::NEXT, currentMv);
                Serial.printf("Learned NEXT: %d mV\n", currentMv);
            } else if (btnStr == "voldown") {
                preferences.putInt("mvVolDown", currentMv);
                buttonManager.setExpectedMv(ButtonId::VOL_DOWN, currentMv);
                Serial.printf("Learned VOLDOWN: %d mV\n", currentMv);
            } else if (btnStr == "volup") {
                preferences.putInt("mvVolUp", currentMv);
                buttonManager.setExpectedMv(ButtonId::VOL_UP, currentMv);
                Serial.printf("Learned VOLUP: %d mV\n", currentMv);
            } else {
                Serial.println("Unknown button. Use: learn play, learn prev, learn next, learn voldown, learn volup");
            }
        } else if (cmd == "play") {
            if (dfPlayerOnline) {
                dfPlayer.play(currentSong); // Replaced start()
                isPlaying = true;
                Serial.println("Playing");
            } else {
                Serial.println("Error: DFPlayer is offline.");
            }
        } else if (cmd == "pause") {
            if (dfPlayerOnline) {
                dfPlayer.pause();
                isPlaying = false;
                Serial.println("Paused");
            } else {
                Serial.println("Error: DFPlayer is offline.");
            }
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
    
    buttonManager.begin();
    buttonManager.configureLadderSeries(10000.0f, 47.0f, 220.0f, 1000.0f, 4700.0f, 10000.0f);
    
    initPreferences();
    initAudio();
}

void loop() {
    buttonManager.loop();
    processAudioEvents();
    processConsole();
    
    // Very short delay to prevent watchdog starvation
    delay(2);
}