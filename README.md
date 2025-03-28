# ESP32 GPS Tracker with OLED Display

This project implements a GPS tracking device using an ESP32 microcontroller, NEO-6M GPS module, SSD1306 OLED display, and SIM800L GPRS module. The device sends location data to an API endpoint and can send alert notifications when triggered. It features dual connectivity with WiFi and GPRS backup.

## Features

- Real-time GPS tracking using NEO-6M module
- Location data sent to API endpoints every 2 minutes
- QR code display for quick access to child data
- Touch sensor (Pin 27) for sending alert notifications:
  - 3-second touch: Low battery alert
  - 6-second touch: SOS emergency alert
  - 10-second touch: Fall detection alert
- Multiple display pages with touch navigation
- WiFiManager for easy WiFi configuration
- SIM800L GPRS connectivity as backup
- Automatic network failover between WiFi and GPRS
- NTP time synchronization

## Hardware Requirements

- ESP32 development board
- NEO-6M GPS module
- SSD1306 OLED display (128x64 pixels)
- SIM800L GPRS module
- Connecting wires
- Power source (battery or USB)

## Wiring

| ESP32 Pin | Connected To |
|-----------|-------------|
| 16 (RX2)  | GPS TX      |
| 17 (TX2)  | GPS RX      |
| 21 (SDA)  | OLED SDA    |
| 22 (SCL)  | OLED SCL    |
| 27        | Touch Sensor|
| 4         | SIM800L TX  |
| 2         | SIM800L RX  |
| 5         | SIM800L RST |
| 3.3V      | GPS VCC, OLED VCC |
| GND       | GPS GND, OLED GND, SIM800L GND |

## Required Libraries

- WiFi.h (built-in)
- HTTPClient.h (built-in)
- Wire.h (built-in)
- Adafruit_GFX.h
- Adafruit_SSD1306.h
- TinyGPS++.h
- qrcode.h (by ricmoo)
- WiFiManager.h
- TinyGsmClient.h

## Installation

1. Install the Arduino IDE
2. Add ESP32 board support to Arduino IDE
3. Install the required libraries using the Library Manager
4. Open the `ESP32_GPS_Tracker.ino` file
5. Configure your WiFi credentials using WiFiManager (no need to modify code)
6. Configure your SIM800L APN settings in the code:
```cpp
#define APN "internet"  // Your mobile APN
#define GPRS_USER ""    // GPRS username, often empty
#define GPRS_PASS ""    // GPRS password, often empty
```
7. Upload the sketch to your ESP32
8. Connect to the WiFi access point "ESP32_GPS_Tracker" and configure your WiFi settings

## API Endpoints

The device communicates with the following API endpoints:

- Latitude data: `http://16.170.159.206:8000/save-latitude/abd1c198-aff9-496f-8b65-c2f43f907343/`
- Longitude data: `http://16.170.159.206:8000/save-longitude/abd1c198-aff9-496f-8b65-c2f43f907343/`
- Notifications: `http://16.170.159.206:8000/add-notification/abd1c198-aff9-496f-8b65-c2f43f907343/`
- QR Code URL: `https://shorturl.at/RyOh0`

## Usage

1. Power on the device
2. Wait for WiFi connection and GPS fix
3. If WiFi fails, device will automatically switch to GPRS
4. The device will automatically send location data every 2 minutes
5. Use touch on Pin 27 to:
   - Short touch: Navigate between display pages
   - 3-second touch: Send low battery alert
   - 6-second touch: Send SOS emergency alert
   - 10-second touch: Send fall detection alert

## Display Pages

The device has three display pages:
1. GPS Page: Shows current coordinates and status
2. QR Code Page: Displays QR code linking to child data
3. Status Page: Shows network status and system information

## Troubleshooting

- If the device fails to connect to WiFi, check your credentials
- If GPS data is not being received, check the wiring between ESP32 and NEO-6M
- For OLED display issues, verify the I2C connections and address
- If API calls fail, ensure the server is running and accessible
- If GPRS connection fails, check SIM card and APN settings
- For WiFiManager issues, reset WiFi settings using the device menu
