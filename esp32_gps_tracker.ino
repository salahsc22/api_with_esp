/*
 * ESP32 GPS Tracker with SSD1306 OLED Display
 * 
 * Features:
 * - GPS location tracking using NEO-6M
 * - OLED display with QR code
 * - Touch sensor for alert notifications
 * - API integration for location and alerts
 * - WiFiManager for easy WiFi setup
 * - SIM800L GPRS connectivity as backup
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>
#include <qrcode.h>
#include <time.h>
#include <WiFiManager.h>  // WiFiManager library for easy WiFi configuration
#include <TinyGsmClient.h>  // For SIM800L GPRS communication

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// GPS settings
#define GPS_RX 16
#define GPS_TX 17
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// Touch pin
#define TOUCH_PIN 27
#define TOUCH_THRESHOLD 40
unsigned long touchStartTime = 0;
bool isTouching = false;
// Static alert flags with correct scope
static bool alert3sSent = false;
static bool alert6sSent = false;
static bool alert10sSent = false;

// SIM800L settings
#define SIM800L_RX 4
#define SIM800L_TX 2
#define SIM800L_RESET 5  // Optional reset pin
HardwareSerial simSerial(2); // Use second hardware serial port
TinyGsm modem(simSerial);
TinyGsmClient gsmClient(modem);

// Network settings
#define APN "internet"  // Your mobile APN
#define GPRS_USER ""    // GPRS username, often empty
#define GPRS_PASS ""    // GPRS password, often empty
bool useGprs = false;    // Flag to determine if we're using GPRS or WiFi
bool networkConnected = false; // Flag to indicate if any network is connected

// Connectivity mode
enum ConnectionMode {
  WIFI_MODE,
  GPRS_MODE,
  NO_CONNECTION
};
ConnectionMode currentConnectionMode = NO_CONNECTION;

// WiFi configuration
#define WIFI_AP_NAME "ESP32_GPS_Tracker"  // Name of the WiFi access point
#define WIFI_AP_PASSWORD "12345678"  // Password for the WiFi access point (min 8 characters)
#define CONFIG_PORTAL_TIMEOUT 180  // Timeout for the configuration portal in seconds
bool shouldSaveConfig = false;  // Flag to indicate if config should be saved

// API endpoints
const char* latitudeApiUrl = "http://16.170.159.206:8000/save-latitude/abd1c198-aff9-496f-8b65-c2f43f907343/";
const char* longitudeApiUrl = "http://16.170.159.206:8000/save-longitude/abd1c198-aff9-496f-8b65-c2f43f907343/";
const char* notificationApiUrl = "http://16.170.159.206:8000/add-notification/abd1c198-aff9-496f-8b65-c2f43f907343/";
const char* qrCodeUrl = "https://shorturl.at/RyOh0";  // Updated to use the shorturl

// API endpoints domain and paths (for GPRS)
const char* apiDomain = "16.170.159.206";
const int apiPort = 8000;
const char* latitudePath = "/save-latitude/abd1c198-aff9-496f-8b65-c2f43f907343/";
const char* longitudePath = "/save-longitude/abd1c198-aff9-496f-8b65-c2f43f907343/";
const char* notificationPath = "/add-notification/abd1c198-aff9-496f-8b65-c2f43f907343/";

// Time settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200; // GMT+2 (adjust based on your timezone)
const int daylightOffset_sec = 3600;

// Display pages
enum DisplayPage {
  GPS_PAGE,
  QR_CODE_PAGE,
  STATUS_PAGE,
  NUM_PAGES
};
DisplayPage currentPage = GPS_PAGE;
DisplayPage previousPage = GPS_PAGE;  // Track previous page to avoid unnecessary refreshes

// Display refresh control
unsigned long lastPageRefreshTime = 0;
const unsigned long pageRefreshInterval = 1000;  // 1 second refresh interval
const unsigned long touchRefreshInterval = 250;  // More frequent refresh during touch
bool forceDisplayRefresh = false;

// Variables for GPS data
float latitude = 0.0;
float longitude = 0.0;
bool gpsValid = false;
bool lastSendSuccess = false;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 120000; // 2 minutes in milliseconds

// WiFi reconnection variables
unsigned long lastWiFiCheckTime = 0;
const unsigned long wifiReconnectInterval = 30000; // Check WiFi every 30 seconds
const int maxReconnectAttempts = 3;
bool wifiReconnecting = false;

// Function prototypes
void connectToWifi();
void sendGpsData();
void sendNotification(const char* title, const char* message);
void displayGpsPage();
void displayQrCodePage();
void displayStatusPage();
void handleTouch();
void checkGps();
String getCurrentDateTime();
void resetWiFiSettings();
void checkWiFiConnection();
bool attemptWiFiReconnect();

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  
  // Initialize GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  
  // Connect to WiFi using WiFiManager
  connectToWifi();
  
  // Initialize time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Display startup message
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("GPS Tracker Ready");
  display.println("Touch Pin 27 to");
  display.println("send alerts");
  display.display();
  delay(2000);
}

void loop() {
  // Check GPS data
  checkGps();
  
  // Handle touch sensor
  handleTouch();
  
  // Check and maintain WiFi connection
  checkWiFiConnection();
  
  // Send GPS data every 2 minutes
  if (millis() - lastSendTime > sendInterval && gpsValid) {
    sendGpsData();
    lastSendTime = millis();
  }
  
  // Only update display at defined intervals or when explicitly needed
  unsigned long currentMillis = millis();
  if (forceDisplayRefresh || 
      (currentMillis - lastPageRefreshTime > 
       (isTouching ? touchRefreshInterval : pageRefreshInterval))) {
    
    // Update display based on current page
    switch (currentPage) {
      case GPS_PAGE:
        displayGpsPage();
        break;
      case QR_CODE_PAGE:
        displayQrCodePage();
        break;
      case STATUS_PAGE:
        displayStatusPage();
        break;
      default:
        break;
    }
    
    lastPageRefreshTime = currentMillis;
    forceDisplayRefresh = false;
    previousPage = currentPage;
  }
  
  delay(50);  // Shorter delay for more responsive operation
}

void connectToWifi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Setup...");
  display.println("Connect to:");
  display.println(WIFI_AP_NAME);
  display.println("to configure");
  display.display();
  
  // Callback for when configuration is needed
  WiFiManager wm;
  
  // Set callback for saving configuration
  wm.setSaveConfigCallback([]() {
    Serial.println("Configuration should be saved");
    shouldSaveConfig = true;
  });
  
  // Set timeout
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
  
  // Set AP name and password
  bool res = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
  
  if (res) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.println(WiFi.localIP().toString());
    display.display();
    Serial.println("WiFi Connected!");
    Serial.println(WiFi.localIP());
    delay(1000);
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connection Failed");
    display.println("Continuing offline");
    display.println("Reset to try again");
    display.display();
    Serial.println("Failed to connect to WiFi");
    delay(2000);
  }
}

void checkGps() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        gpsValid = true;
        Serial.print("Latitude: ");
        Serial.println(latitude, 6);
        Serial.print("Longitude: ");
        Serial.println(longitude, 6);
      } else {
        gpsValid = false;
      }
    }
  }
  
  // If no GPS data is received after 5 seconds, show error
  static unsigned long gpsCheckTimer = millis();
  if (millis() - gpsCheckTimer > 5000) {
    gpsCheckTimer = millis();
    if (gps.charsProcessed() < 10) {
      Serial.println("No GPS data received. Check wiring.");
    }
  }
}

void sendGpsData() {
  // No need for WiFi check here as checkWiFiConnection will handle reconnection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send GPS data.");
    lastSendSuccess = false;
    return;
  }
  
  HTTPClient http;
  bool success = true;
  
  // Send latitude
  http.begin(latitudeApiUrl);
  http.addHeader("Content-Type", "application/json");
  String latPayload = "{\"latitude\":" + String(latitude, 6) + "}";
  int latHttpResponseCode = http.POST(latPayload);
  
  if (latHttpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Latitude HTTP Response code: " + String(latHttpResponseCode));
    Serial.println("Response: " + response);
  } else {
    Serial.print("Latitude Error on sending POST: ");
    Serial.println(latHttpResponseCode);
    success = false;
  }
  http.end();
  
  // Send longitude
  http.begin(longitudeApiUrl);
  http.addHeader("Content-Type", "application/json");
  String lngPayload = "{\"longitude\":" + String(longitude, 6) + "}";
  int lngHttpResponseCode = http.POST(lngPayload);
  
  if (lngHttpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Longitude HTTP Response code: " + String(lngHttpResponseCode));
    Serial.println("Response: " + response);
  } else {
    Serial.print("Longitude Error on sending POST: ");
    Serial.println(lngHttpResponseCode);
    success = false;
  }
  http.end();
  
  lastSendSuccess = success;
}

void sendNotification(const char* title, const char* message) {
  // No need for WiFi check here as checkWiFiConnection will handle reconnection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send notification.");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Alert Failed:");
    display.println("No WiFi Connection");
    display.println(title);
    display.display();
    delay(2000);
    return;
  }
  
  HTTPClient http;
  http.begin(notificationApiUrl);
  http.addHeader("Content-Type", "application/json");
  
  String dateTime = getCurrentDateTime();
  String payload = "{\"title\":\"" + String(title) + "\",\"message\":\"" + String(message) + "\",\"delivered_at\":\"" + dateTime + "\"}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Notification HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Alert Sent:");
    display.println(title);
    display.println(message);
    display.display();
    delay(2000);
  } else {
    Serial.print("Error on sending notification: ");
    Serial.println(httpResponseCode);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Alert Failed:");
    display.println(title);
    display.display();
    delay(2000);
  }
  http.end();
}

String getCurrentDateTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "2023-01-01T00:00:00";
  }
  
  char dateTime[25];
  strftime(dateTime, sizeof(dateTime), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(dateTime);
}

void handleTouch() {
  int touchValue = touchRead(TOUCH_PIN);
  static unsigned long lastTouchCheckTime = 0;
  static int touchDebounceCount = 0;
  
  // Check if touch is detected (lower value means touched)
  if (touchValue < TOUCH_THRESHOLD) {
    if (!isTouching) {
      // Touch just started
      touchStartTime = millis();
      isTouching = true;
      alert3sSent = false;  // Reset alert flags at start of new touch
      alert6sSent = false;
      alert10sSent = false;
      Serial.println("Touch started");
      forceDisplayRefresh = true;
    } else {
      // Continuing touch - check duration
      unsigned long touchDuration = millis() - touchStartTime;
      
      // Only refresh the display at a controlled rate during touch
      if (millis() - lastTouchCheckTime > touchRefreshInterval) {
        lastTouchCheckTime = millis();
        
        // Show touch progress on display (for all pages)
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Touch detected:");
        
        // Draw progress bar
        int barWidth = map(touchDuration, 0, 10000, 0, SCREEN_WIDTH - 2);
        if (barWidth > SCREEN_WIDTH - 2) barWidth = SCREEN_WIDTH - 2;
        
        display.drawRect(0, 15, SCREEN_WIDTH, 10, SSD1306_WHITE);
        display.fillRect(1, 16, barWidth, 8, SSD1306_WHITE);
        
        // Show message about current alert threshold
        display.setCursor(0, 30);
        if (touchDuration < 3000) {
          display.println("Hold for alerts...");
        } else if (touchDuration < 6000) {
          display.println("Low Battery Alert");
          display.println("Keep holding for SOS");
        } else if (touchDuration < 10000) {
          display.println("SOS Alert");
          display.println("Keep holding for Fall");
        } else {
          display.println("Fall Alert");
        }
        
        display.display();
      }
      
      // Handle alerts based on touch duration
      // 3-5 seconds - Low battery
      if (touchDuration > 3000 && !alert3sSent && touchDuration < 6000) {
        sendNotification("Low battery", "Battery is low");
        alert3sSent = true;
        Serial.println("Low battery alert sent");
        forceDisplayRefresh = true;
      } 
      
      // 6-8 seconds - SOS
      if (touchDuration > 6000 && !alert6sSent && touchDuration < 10000) {
        sendNotification("SOS", "Help I am in emergency");
        alert6sSent = true;
        Serial.println("SOS alert sent");
        forceDisplayRefresh = true;
      }
      
      // 10+ seconds - falling detected
      if (touchDuration > 10000 && !alert10sSent) {
        sendNotification("Falling detected", "Your child has been fall");
        alert10sSent = true;
        Serial.println("Fall alert sent");
        forceDisplayRefresh = true;
      }
    }
  } else {
    // No touch detected, but was touching before
    if (isTouching) {
      // Add better debouncing to avoid accidental releases
      if (touchDebounceCount < 5) {
        touchDebounceCount++;
        delay(10);
        return;
      }
      
      touchValue = touchRead(TOUCH_PIN);
      if (touchValue >= TOUCH_THRESHOLD) {  // Confirm release
        unsigned long touchDuration = millis() - touchStartTime;
        
        if (touchDuration < 3000) {
          // Short touch (less than 3 seconds) - change page
          DisplayPage oldPage = currentPage;
          currentPage = (DisplayPage)((currentPage + 1) % NUM_PAGES);
          
          // Only force refresh if page changed
          if (oldPage != currentPage) {
            Serial.print("Changed to page: ");
            Serial.println(currentPage);
            forceDisplayRefresh = true;
          }
        }
        
        // Reset touch state
        isTouching = false;
        touchDebounceCount = 0;
        delay(300); // Additional debounce
        forceDisplayRefresh = true;
      } else {
        // False release detected, reset debounce counter
        touchDebounceCount = 0;
      }
    }
  }
}

void displayGpsPage() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("GPS Data:");
  
  if (gpsValid) {
    display.print("Lat: ");
    display.println(latitude, 6);
    display.print("Lng: ");
    display.println(longitude, 6);
    
    // Show last send status
    display.println();
    display.print("Last Send: ");
    display.println(lastSendSuccess ? "OK" : "Failed");
    
    // Show time until next send
    unsigned long timeToNextSend = 0;
    if (millis() - lastSendTime < sendInterval) {
      timeToNextSend = (sendInterval - (millis() - lastSendTime)) / 1000;
    }
    display.print("Next in: ");
    display.print(timeToNextSend);
    display.println("s");
  } else {
    display.println("Waiting for GPS fix...");
  }
  
  display.display();
}

void displayQrCodePage() {
  // Only actually render the QR code if this is a new page
  // since QR code generation is relatively expensive
  static bool qrCodeGenerated = false;
  
  if (previousPage != QR_CODE_PAGE || !qrCodeGenerated || forceDisplayRefresh) {
    display.clearDisplay();
    
    // Use the updated shorturl for the QR code
    const char* urlToDisplay = qrCodeUrl;
    
    // Create smallest possible QR code with lowest error correction
    QRCode qrcode;
    // Version 2 should be sufficient for the shorter URL
    uint8_t qrcodeData[qrcode_getBufferSize(2)];
    qrcode_initText(&qrcode, qrcodeData, 2, ECC_LOW, urlToDisplay);
    
    // Calculate maximum possible scale that fits the display
    // Leave a 4-pixel quiet zone (white border) around the QR code
    int quietZone = 4;
    int maxSize = min(SCREEN_WIDTH, SCREEN_HEIGHT) - (quietZone * 2);
    int scale = maxSize / qrcode.size;
    if (scale < 1) scale = 1;
    
    // Center the QR code on the display (including quiet zone)
    int xOffset = (SCREEN_WIDTH - (qrcode.size * scale)) / 2;
    int yOffset = (SCREEN_HEIGHT - (qrcode.size * scale)) / 2;
    
    // Draw white background with quiet zone
    display.fillRect(
      xOffset - quietZone, 
      yOffset - quietZone, 
      qrcode.size * scale + (quietZone * 2), 
      qrcode.size * scale + (quietZone * 2), 
      SSD1306_WHITE
    );
    
    // Draw QR code (black modules on white background)
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          // Black module
          display.fillRect(x * scale + xOffset, y * scale + yOffset, scale, scale, SSD1306_BLACK);
        }
      }
    }
    
    // Add "SCAN ME" text below QR
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor((SCREEN_WIDTH - 42) / 2, SCREEN_HEIGHT - 8);
    display.println("SCAN ME");
    
    display.display();
    
    // Set flag to avoid regenerating the same QR code repeatedly
    qrCodeGenerated = true;
    
    // Print debug info to serial only once when QR code is generated
    Serial.println("QR Code displayed:");
    Serial.println(urlToDisplay);
    Serial.print("QR Size: ");
    Serial.println(qrcode.size);
    Serial.print("Scale: ");
    Serial.println(scale);
  }
}

void displayStatusPage() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Status Page");
  display.println();
  
  // WiFi status
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  
  // GPS status
  display.print("GPS: ");
  display.println(gpsValid ? "Fixed" : "No Fix");
  
  // Touch instructions
  display.println();
  display.println("Touch 3s: Low Batt");
  display.println("Touch 6s: SOS");
  display.println("Touch 10s: Fall");
  
  display.display();
}

void resetWiFiSettings() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Resetting WiFi...");
  display.display();
  
  WiFiManager wm;
  wm.resetSettings();
  
  display.println("WiFi Reset Done");
  display.println("Restarting...");
  display.display();
  
  delay(1000);
  ESP.restart();
}

void checkWiFiConnection() {
  // Only check WiFi connection status at defined intervals
  if (millis() - lastWiFiCheckTime > wifiReconnectInterval) {
    lastWiFiCheckTime = millis();
    
    if (WiFi.status() != WL_CONNECTED && !wifiReconnecting) {
      Serial.println("WiFi connection lost. Attempting to reconnect...");
      
      // Set reconnecting flag to avoid multiple reconnection attempts
      wifiReconnecting = true;
      
      // Attempt to reconnect
      bool reconnected = attemptWiFiReconnect();
      
      // Reset reconnecting flag
      wifiReconnecting = false;
      
      if (reconnected) {
        Serial.println("WiFi reconnected successfully");
        Serial.println(WiFi.localIP());
        forceDisplayRefresh = true;
      } else {
        Serial.println("WiFi reconnection failed");
        forceDisplayRefresh = true;
      }
    }
  }
}

bool attemptWiFiReconnect() {
  int attempts = 0;
  
  // First try to reconnect using saved credentials
  WiFi.reconnect();
  
  // Show reconnection status on display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Reconnecting...");
  display.display();
  
  // Try several times with a short delay
  while (WiFi.status() != WL_CONNECTED && attempts < maxReconnectAttempts) {
    delay(1000);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Successfully reconnected
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Reconnected");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(1000);
    return true;
  } else {
    // If simple reconnect failed, try using WiFiManager for a more thorough reconnection
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Reconnect failed");
    display.println("Using saved config");
    display.display();
    
    // Try to connect with WiFiManager but don't start the config portal
    WiFiManager wm;
    wm.setConfigPortalTimeout(10); // Very short timeout
    bool result = wm.autoConnect(); // Use saved credentials
    
    if (result) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("WiFi Reconnected");
      display.println(WiFi.localIP().toString());
      display.display();
      delay(1000);
      return true;
    } else {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("WiFi Reconnect Failed");
      display.println("Will try again later");
      display.display();
      delay(1000);
      return false;
    }
  }
}
