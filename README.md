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
![Hardware Setup](hardware.jpg)

- **MCU**: ESP32-C3 Super Mini
- **Audio Decoder**: DFRobot DFPlayer Mini
- **Battery**: 3.7V Li-ion 402020 (400mAh)
- **Charging**: TP4056 USB-C Module
- **Regulator**: LM2950-3.3

### Basic Wiring Table
| Component | ESP32-C3 Pin |
|---|---|
| DFPlayer RX | GPIO4 |
| DFPlayer TX | GPIO5 |
| Button Ladder | GPIO1 |

*(See [Wiring_Table.md](Wiring_Table.md) for detailed power and resistor connections)*

## Firmware Compilation
The firmware is written using the Arduino Framework in PlatformIO.
```bash
pio run
```

See [Wiring Table](Wiring_Table.md) for pin connections.

## Future Improvements
- Navigation to Next/Previous folders (requires explicit folder index tracking for DFPlayer Mini).
- Enhanced debouncing filters.
- Display support (e.g., small OLED screen for track info).

## Contributing & Forking
We welcome contributions to the OpenDAP-C3 project! If you'd like to help out or make your own custom version, please follow the steps below.

### **How to fork:**
1. Click the **Fork** button at the top right of this repository to create your own copy.
2. Clone your fork locally: `git clone https://github.com/your-username/OpenDAP-C3.git`
3. Create a new branch for your feature or bugfix: `git checkout -b my-new-feature`
4. Commit your changes and push the branch to your fork.
5. Submit a Pull Request back to the main repository!

## License & Copyright
Copyright (c) 2026 Shraman C.

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
