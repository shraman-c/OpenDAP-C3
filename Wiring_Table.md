# Wiring Table

The following table describes the pin connections between the ESP32-C3 Super Mini and the external components.

## DFPlayer Mini
| ESP32-C3 Pin / External | DFPlayer Pin | Description |
|---|---|---|
| GPIO4 | RX | Serial Transmit from ESP32 |
| GPIO5 | TX | Serial Receive to ESP32 |
| 3.3V | VCC | Power |
| GND | GND | Ground |
| USB-C D+ | USB+ | USB Data Positive (for PC file transfer) |
| USB-C D- | USB- | USB Data Negative (for PC file transfer) |

> Note: Audio output from the DFPlayer is taken directly from the DAC_R and DAC_L pins to the 3.5mm Headphone Jack.

## Resistor Ladder Buttons
Connect a single wire from **GPIO1** to the button ladder. 
Also include a 10kΩ Pull-Up resistor from GPIO1 to 3.3V.

| Button | Resistor Value | Nominal ADC Range |
|---|---|---|
| Play/Pause | 100 Ω | 20 - 60 |
| Next | 220 Ω | 70 - 120 |
| Previous | 1 kΩ | 250 - 500 |
| Volume + | 4.7 kΩ | 900 - 1700 |
| Volume - | 10 kΩ | 1800 - 2500 |

> The ADC reads ~4095 when no button is pressed.

## Power System
| Module | Pin | Connection |
|---|---|---|
| Battery | B+ / B- | TP4056 B+ / B- |
| TP4056 | OUT+ | Slide Switch |
| Slide Switch | Output | LM2950-3.3 IN |
| LM2950-3.3 | OUT | ESP32-C3 3.3V / DFPlayer VCC |
