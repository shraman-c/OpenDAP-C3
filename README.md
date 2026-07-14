# OpenDAP-C3

An open-source portable Digital Audio Player based on the ESP32-C3 Super Mini and DFPlayer Mini.

## Features
- Plays MP3 and WAV files from a microSD card
- 5-button resistor ladder control (Play/Pause, Next, Previous, Vol+, Vol-)
- Headphone output (3.5mm jack)
- Power management with deep sleep functionality
- Restores playback state (volume and last song) on boot
- Debug console over UART

## Hardware
- **MCU**: ESP32-C3 Super Mini
- **Audio Decoder**: DFRobot DFPlayer Mini
- **Battery**: 3.7V Li-ion 402020 (400mAh)
- **Charging**: TP4056 USB-C Module
- **Regulator**: LM2950-3.3

## Firmware Compilation
The firmware is written using the Arduino Framework in PlatformIO.
```bash
pio run
```

See [Wiring Table](Wiring_Table.md) for pin connections.
