# OpenDAP-C3 v1.0.0 Release Notes

Welcome to the first official release of the **OpenDAP-C3** portable Digital Audio Player firmware.

## What's New
- Full support for the DFRobot DFPlayer Mini via UART communication (9600 baud).
- Implemented a resilient single-ADC button ladder reading algorithm (GPIO1).
- Added multi-press button support:
  - Short Press
  - Long Press (>2s)
  - Double Press
  - Continuous Hold
- Deep Sleep implementation for power saving (Wakes on GPIO1 pull-down).
- Auto-resume functionality using ESP32 Preferences API.

## Known Issues
- Navigation to Next/Previous folder is currently stubbed as DFPlayer Mini requires explicit folder index tracking, which depends on how files are formatted on the SD card.
- ADC calibration might vary based on your specific resistor tolerances. Use the `button` console command to recalibrate if necessary.

## Getting Started
To upload this firmware, clone the repository and build using PlatformIO:
```bash
pio run -t upload
```

Refer to the Wiring Table for hardware connections.
