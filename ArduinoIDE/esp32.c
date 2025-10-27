#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================== WiFi Credentials ==================
const char* ssid = "realme narzo 60 5G";
const char* password = "12345678";

// Replace with your Google Apps Script Web App URL
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbybr4-bpZLHMpUz1gd8j7tG1NcFF1C0K6lbFodowfbvt5ehL_GCvgtK3Oc3XmONjJUY7w/exec";

// ================== Pin Definitions ==================
#define VOLTAGE_PIN 34      
#define CURRENT_PIN 35      
#define DHT_PIN 4           
#define DHT_TYPE DHT11      

// ================== Constants ==================
const float VREF = 3.3;           
const int ADC_RES = 4095;         
const float VOLTAGE_RATIO = 5.0;   // Voltage divider ratio (0-25V sensor)

// ACS712 (5A module → 185 mV/A)
const float ACS_SENSITIVITY = 185.0;  
float ACS_OFFSET = 0.0;               

// ================== Objects ==================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);   // I2C address 0x27, 16x2 LCD

// ================== Variables ==================
float voltage = 0.0;
float current = 0.0;
float power = 0.0;
float temperature = 0.0;
float humidity = 0.0;

// ================== Setup ==================
void setup() {
  Serial.begin(115200);
  dht.begin();
  lcd.init();
  lcd.backlight();
  delay(2000);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  // Auto-calibrate ACS712 offset
  long sum = 0;
  for (int i = 0; i < 500; i++) {
    int raw = analogRead(CURRENT_PIN);
    sum += raw;
    delay(2);
  }
  ACS_OFFSET = (float)sum / 500 * VREF / ADC_RES;

  Serial.println("==========================================");
  Serial.println("   REAL-TIME SOLAR MONITORING SYSTEM");
  Serial.println("   (ESP32 + ACS712 + Voltage Sensor + DHT11 + Google Sheets + LCD)");
  Serial.println("==========================================");
}

// ================== Read Voltage ==================
float readVoltage() {
  int raw = analogRead(VOLTAGE_PIN);
  return (raw * VREF / ADC_RES) * VOLTAGE_RATIO;
}

// ================== Read Current ==================
float readCurrent() {
  long sum = 0;
  const int samples = 200;

  for (int i = 0; i < samples; i++) {
    int raw = analogRead(CURRENT_PIN);
    float voltage = (raw * VREF / ADC_RES);
    float current = (voltage - ACS_OFFSET) * 1000.0 / ACS_SENSITIVITY;
    sum += current;
    delay(2);
  }
  return (float)sum / samples;
}

// ================== Send Data to Google Sheets ==================
void sendToGoogleSheets(float voltage, float current, float power, float temperature, float humidity) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(GOOGLE_SCRIPT_URL);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"voltage\":" + String(voltage, 2) +
                      ",\"current\":" + String(current, 3) +
                      ",\"power\":" + String(power, 2) +
                      ",\"temperature\":" + String(temperature, 1) +
                      ",\"humidity\":" + String(humidity, 1) + "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      Serial.println("✅ Data sent to Google Sheets");
    } else {
      Serial.println("❌ Error sending data");
    }
    http.end();
  }
}

// ================== Loop ==================
void loop() {
  // Read sensors
  voltage = readVoltage();
  current = readCurrent();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Calculate Power
  power = voltage * current;

  // Display on Serial Monitor
  Serial.println("-------------------------------------------------");
  Serial.print("Voltage: "); Serial.print(voltage, 2); Serial.println(" V");
  Serial.print("Current: "); Serial.print(current, 3); Serial.println(" A");
  Serial.print("Power:   "); Serial.print(power, 2); Serial.println(" W");

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("⚠ DHT11 Error: Check wiring!");
  } else {
    Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println(" °C");
    Serial.print("Humidity: "); Serial.print(humidity, 1); Serial.println(" %");
  }
  Serial.println("-------------------------------------------------");

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:"); lcd.print(voltage, 1); lcd.print("V ");
  lcd.print("I:"); lcd.print(current, 2); lcd.print("A");
  lcd.setCursor(0, 1);
  lcd.print("P:"); lcd.print(power, 1); lcd.print("W ");
  lcd.print("T:"); lcd.print(temperature, 0); lcd.print("C");

  // Send to Google Sheets
  sendToGoogleSheets(voltage, current, power, temperature, humidity);

  delay(5000); // Log every 5 seconds
}