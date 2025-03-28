# ESP32 GPS Tracker with OLED Display

This project implements a GPS tracking device using an ESP32 microcontroller, NEO-6M GPS module, and SSD1306 OLED display. The device sends location data to an API endpoint and can send alert notifications when triggered.

## Features

- Real-time GPS tracking using NEO-6M module
- Location data sent to API endpoints every 2 minutes
- QR code display for quick access to child data
- Touch sensor (Pin 27) for sending alert notifications:
  - 3-second touch: Low battery alert
  - 6-second touch: SOS emergency alert
  - 10-second touch: Fall detection alert
- Multiple display pages with touch navigation

## Hardware Requirements

- ESP32 development board
- NEO-6M GPS module
- SSD1306 OLED display (128x64 pixels)
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
| 3.3V      | GPS VCC, OLED VCC |
| GND       | GPS GND, OLED GND |

## Required Libraries

- WiFi.h (built-in)
- HTTPClient.h (built-in)
- Wire.h (built-in)
- Adafruit_GFX.h
- Adafruit_SSD1306.h
- TinyGPS++.h
- qrcode.h (by ricmoo)

## Installation

1. Install the Arduino IDE
2. Add ESP32 board support to Arduino IDE
3. Install the required libraries using the Library Manager
4. Open the `esp32_gps_tracker.ino` file
5. Configure your WiFi credentials in the code
6. Upload the sketch to your ESP32

## Configuration

Before uploading, make sure to update the following in the code:

```cpp
// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

## API Endpoints

The device communicates with the following API endpoints:

- Latitude data: `http://16.170.159.206:8000/save-latitude/7fb79409-c63f-4045-be75-9e21bf089eaa/`
- Longitude data: `http://16.170.159.206:8000/save-longitude/7fb79409-c63f-4045-be75-9e21bf089eaa/`
- Notifications: `http://16.170.159.206:8000/add-notification/7fb79409-c63f-4045-be75-9e21bf089eaa/`
- QR Code URL: `http://16.170.159.206:8000/child_data/7fb79409-c63f-4045-be75-9e21bf089eaa/`

## Usage

1. Power on the device
2. Wait for WiFi connection and GPS fix
3. The device will automatically send location data every 2 minutes
4. Use touch on Pin 27 to:
   - Short touch: Navigate between display pages
   - 3-second touch: Send low battery alert
   - 6-second touch: Send SOS emergency alert
   - 10-second touch: Send fall detection alert

## Troubleshooting

- If the device fails to connect to WiFi, check your credentials
- If GPS data is not being received, check the wiring between ESP32 and NEO-6M
- For OLED display issues, verify the I2C connections and address
- If API calls fail, ensure the server is running and accessible
