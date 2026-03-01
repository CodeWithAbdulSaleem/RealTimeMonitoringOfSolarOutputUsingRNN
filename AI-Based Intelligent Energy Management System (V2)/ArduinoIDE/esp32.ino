/******************************************************
 SMART SOLAR MONITORING SYSTEM - ENHANCED V2.0
 ESP32 + LCD + OLED + WiFi + Web Server + Data Logging
 ACS712 x3 + Voltage Divider + 2 Relay + DHT11
******************************************************/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <time.h>

// =================== WiFi CONFIG ===================
const char* WIFI_SSID     = "Oneplus2";      // <-- Change this
const char* WIFI_PASSWORD = "12345678";      // <-- Change this
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbybr4-bpZLHMpUz1gd8j7tG1NcFF1C0K6lbFodowfbvt5ehL_GCvgtK3Oc3XmONjJUY7w/exec";

// =================== PIN CONFIG ====================
#define PANEL_VOLT_PIN     34
#define BATTERY_VOLT_PIN   35
#define PANEL_CUR_PIN      32    // ACS712 5A
#define MOTOR_CUR_PIN      33    // ACS712 30A
#define FAN_CUR_PIN        25    // ACS712 5A
#define RELAY_MOTOR        26
#define RELAY_FAN          27
#define DHT_PIN            4
#define DHT_TYPE           DHT11

// I2C uses default GPIO 21 (SDA), GPIO 22 (SCL)

// =================== DISPLAY CONFIG ================
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_ADDR    0x3C
#define LCD_ADDR     0x27
#define LCD_COLS     16
#define LCD_ROWS     2

// =================== CONSTANTS =====================
const float VREF = 3.3;
const int   ADC_RES = 4095;
const float BATTERY_RATIO = (33.0 + 7.8) / 7.8;
const float PANEL_RATIO   = 5.0;
const float ACS_5A  = 185.0;   // mV/A
const float ACS_30A = 66.0;    // mV/A

// Smoothing factor for EMA (0.0 - 1.0, lower = smoother)
const float EMA_ALPHA = 0.15;

// Battery thresholds
const float BATT_OVERCHARGE   = 14.2;
const float BATT_HIGH         = 12.8;
const float BATT_NOMINAL      = 12.3;
const float BATT_LOW          = 11.8;
const float BATT_CRITICAL     = 11.5;

// Data logging interval (ms)
const unsigned long LOG_INTERVAL = 60000;
const int MAX_LOG_ENTRIES = 1440; // 24 hours at 1/min

// =================== OBJECTS =======================
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// =================== GLOBALS =======================
float panelOffset = 0, motorOffset = 0, fanOffset = 0;

// Smoothed sensor values
float panelVoltage = 0, batteryVoltage = 0;
float panelCurrent = 0, motorCurrent = 0, fanCurrent = 0;
float panelPower = 0, motorPower = 0, fanPower = 0, totalLoadPower = 0;
float temperature = 0, humidity = 0;

bool motorState = false, fanState = false;
bool wifiConnected = false;

// AI Decision tracking
String aiDecision = "Initializing...";
float aiMotorScore = 0;   // 0-100 fuzzy output score
float aiFanScore = 0;
float aiBattHealth = 0;   // battery health score
float aiSolarScore = 0;   // solar availability score

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastLogTime = 0;
unsigned long bootTime = 0;
int lcdPage = 0;

// Data logging circular buffer
struct LogEntry {
  unsigned long timestamp;
  float pV, pC, pP;     // panel
  float bV;              // battery
  float mC, fC;          // loads
  float temp, hum;       // environment
  bool mRelay, fRelay;   // relay states
};

LogEntry dataLog[MAX_LOG_ENTRIES];
int logIndex = 0;
int logCount = 0;

// =================== CALIBRATION ===================
float calibrateACS(int pin) {
  long sum = 0;
  for (int i = 0; i < 500; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return ((float)sum / 500.0) * VREF / ADC_RES;
}

// =================== SENSOR READS ==================
float readVoltage(int pin, float ratio) {
  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  float avg = (float)sum / 50.0;
  return (avg * VREF / ADC_RES) * ratio;
}

float readCurrentRaw(int pin, float offset, float sensitivity) {
  long sum = 0;
  for (int i = 0; i < 200; i++) {
    sum += analogRead(pin);
    delayMicroseconds(500);
  }
  float avgRaw = (float)sum / 200.0;
  float voltage = avgRaw * VREF / ADC_RES;
  float current = (voltage - offset) * 1000.0 / sensitivity;
  if (abs(current) < 0.05) current = 0;
  return current;
}

// Exponential Moving Average
float emaFilter(float newVal, float oldVal) {
  if (oldVal == 0) return newVal; // first reading
  return EMA_ALPHA * newVal + (1.0 - EMA_ALPHA) * oldVal;
}

void readAllSensors() {
  float rawPV = readVoltage(PANEL_VOLT_PIN, PANEL_RATIO);
  float rawBV = readVoltage(BATTERY_VOLT_PIN, BATTERY_RATIO);
  float rawPC = readCurrentRaw(PANEL_CUR_PIN, panelOffset, ACS_5A);
  float rawMC = readCurrentRaw(MOTOR_CUR_PIN, motorOffset, ACS_30A);
  float rawFC = readCurrentRaw(FAN_CUR_PIN, fanOffset, ACS_5A);

  panelVoltage   = emaFilter(rawPV, panelVoltage);
  batteryVoltage = emaFilter(rawBV, batteryVoltage);
  panelCurrent   = emaFilter(rawPC, panelCurrent);
  motorCurrent   = emaFilter(rawMC, motorCurrent);
  fanCurrent     = emaFilter(rawFC, fanCurrent);

  panelPower     = panelVoltage * panelCurrent;
  motorPower     = batteryVoltage * motorCurrent;
  fanPower       = batteryVoltage * fanCurrent;
  totalLoadPower = motorPower + fanPower;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = emaFilter(t, temperature);
  if (!isnan(h)) humidity    = emaFilter(h, humidity);
}

// =========== AI FUZZY LOGIC INFERENCE ENGINE ===========
float trapMF(float x, float a, float b, float c, float d) {
  if (x <= a || x >= d) return 0.0;
  if (x >= b && x <= c) return 1.0;
  if (x < b) return (x - a) / (b - a);
  return (d - x) / (d - c);
}

float battCritical(float v) { return trapMF(v, 0,   0,    11.0, 11.5); }
float battLow(float v)      { return trapMF(v, 11.0, 11.5, 11.8, 12.2); }
float battNormal(float v)   { return trapMF(v, 11.8, 12.2, 13.0, 13.5); }
float battHigh(float v)     { return trapMF(v, 13.0, 13.5, 14.0, 14.2); }
float battOver(float v)     { return trapMF(v, 14.0, 14.2, 15.0, 15.0); }

float solarNone(float p)   { return trapMF(p, -1, 0,  0,  5);  }
float solarLow(float p)    { return trapMF(p, 0,  5,  15, 30); }
float solarMedium(float p) { return trapMF(p, 15, 30, 50, 70); }
float solarHigh(float p)   { return trapMF(p, 50, 70, 200, 200); }

float tempCold(float t)    { return trapMF(t, -10, -10, 10, 20); }
float tempNormal(float t)  { return trapMF(t, 15,  20,  30, 35); }
float tempHot(float t)     { return trapMF(t, 30,  35,  50, 50); }

void aiFuzzyControl() {
  float bCrit = battCritical(batteryVoltage);
  float bLow = battLow(batteryVoltage);
  float bNorm = battNormal(batteryVoltage);
  float bHigh = battHigh(batteryVoltage);
  float bOver = battOver(batteryVoltage);

  float sNone = solarNone(panelPower);
  float sLow = solarLow(panelPower);
  float sMed = solarMedium(panelPower);
  float sHigh = solarHigh(panelPower);

  float tCold = tempCold(temperature);
  float tNorm = tempNormal(temperature);
  float tHot = tempHot(temperature);

  float motorScore = 0, motorWeight = 0;
  float fanScore = 0, fanWeight = 0;

  // Rule 1: Battery Critical → ALL OFF
  float r1 = bCrit;
  motorScore += r1 * 0.0; motorWeight += r1;
  fanScore += r1 * 0.0; fanWeight += r1;

  // Rule 2: Battery Overcharged → ALL OFF
  float r2 = bOver;
  motorScore += r2 * 0.0; motorWeight += r2;
  fanScore += r2 * 0.0; fanWeight += r2;

  // Rule 3: Battery Low + Solar None → ALL OFF
  float r3 = min(bLow, sNone);
  motorScore += r3 * 0.0; motorWeight += r3;
  fanScore += r3 * 0.0; fanWeight += r3;

  // Rule 4: Battery Low + Solar High → Motor OK, Fan OFF
  float r4 = min(bLow, sHigh);
  motorScore += r4 * 60.0; motorWeight += r4;
  fanScore += r4 * 20.0; fanWeight += r4;

  // Rule 5: Battery Normal + Solar High → ALL ON
  float r6 = min(bNorm, sHigh);
  motorScore += r6 * 100.0; motorWeight += r6;
  fanScore += r6 * 100.0; fanWeight += r6;

  // Rule 10: Battery High → ALL ON
  float r10 = bHigh;
  motorScore += r10 * 100.0; motorWeight += r10;
  fanScore += r10 * 100.0; fanWeight += r10;

  aiMotorScore = (motorWeight > 0.0) ? (motorScore / motorWeight) : 0.0;
  aiFanScore = (fanWeight > 0.0) ? (fanScore / fanWeight) : 0.0;
  aiBattHealth = bNorm * 70.0 + bHigh * 100.0 + bLow * 30.0 + bCrit * 0.0 + bOver * 10.0;
  aiSolarScore = sNone * 0.0 + sLow * 25.0 + sMed * 60.0 + sHigh * 100.0;

  if (aiMotorScore >= 50 && !motorState) {
    digitalWrite(RELAY_MOTOR, LOW); motorState = true;
  } else if (aiMotorScore < 30 && motorState) {
    digitalWrite(RELAY_MOTOR, HIGH); motorState = false;
  }

  if (aiFanScore >= 50 && !fanState) {
    digitalWrite(RELAY_FAN, LOW); fanState = true;
  } else if (aiFanScore < 30 && fanState) {
    digitalWrite(RELAY_FAN, HIGH); fanState = false;
  }

  if (bCrit > 0.5 || bOver > 0.5) {
    if (motorState) { digitalWrite(RELAY_MOTOR, HIGH); motorState = false; }
    if (fanState)   { digitalWrite(RELAY_FAN, HIGH);   fanState = false;   }
  }

  if (bCrit > 0.5) aiDecision = "CRITICAL: Battery Low - Loads OFF";
  else if (bOver > 0.5) aiDecision = "PROTECT: Overcharge - Loads OFF";
  else if (bHigh > 0.3) aiDecision = "OPTIMAL: Battery Healthy - Enabled";
  else aiDecision = "MONITORING: Evaluating...";
}

// =================== DISPLAYS ===================
void updateLCD() {
  lcd.clear();
  switch (lcdPage) {
    case 0:
      lcd.setCursor(0, 0); lcd.print("SolV:"); lcd.print(panelVoltage, 1); lcd.print("V");
      lcd.setCursor(0, 1); lcd.print("SolI:"); lcd.print(panelCurrent, 2); lcd.print("A");
      break;
    case 1:
      lcd.setCursor(0, 0); lcd.print("SolP:"); lcd.print(panelPower, 1); lcd.print("W");
      lcd.setCursor(0, 1); lcd.print("Batt:"); lcd.print(batteryVoltage, 1); lcd.print("V");
      break;
    case 2:
      lcd.setCursor(0, 0); lcd.print("M:"); lcd.print(motorCurrent, 1); lcd.print("A "); lcd.print(motorState?"ON":"OF");
      lcd.setCursor(0, 1); lcd.print("F:"); lcd.print(fanCurrent, 1); lcd.print("A "); lcd.print(fanState?"ON":"OF");
      break;
  }
  lcdPage = (lcdPage + 1) % 3;
}

void updateOLED() {
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0); oled.print("SOLAR MONITOR V2");
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  oled.setCursor(0, 12); oled.print("Sol:"); oled.print(panelVoltage, 1); oled.print("V "); oled.print(panelCurrent, 1); oled.print("A");
  oled.setCursor(0, 22); oled.print("Bat:"); oled.print(batteryVoltage, 1); oled.print("V");
  oled.setCursor(0, 32); oled.print("M:"); oled.print(motorCurrent, 1); oled.print("A "); oled.print(motorState?"ON":"OFF");
  oled.setCursor(0, 42); oled.print("F:"); oled.print(fanCurrent, 1); oled.print("A "); oled.print(fanState?"ON":"OFF");
  oled.setCursor(0, 55); oled.print("Pwr:"); oled.print(panelPower, 1); oled.print("W Ld:"); oled.print(totalLoadPower, 1); oled.print("W");
  oled.display();
}

// =================== SERVER & LOGGING ===================
void logData() {
  LogEntry entry;
  entry.timestamp = millis() / 1000;
  entry.pV = panelVoltage; entry.pC = panelCurrent; entry.pP = panelPower;
  entry.bV = batteryVoltage; entry.mC = motorCurrent; entry.fC = fanCurrent;
  entry.temp = temperature; entry.hum = humidity;
  entry.mRelay = motorState; entry.fRelay = fanState;
  dataLog[logIndex] = entry;
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  if (logCount < MAX_LOG_ENTRIES) logCount++;
}

String getJsonData() {
  String json = "{";
  json += "\"panelVoltage\":" + String(panelVoltage, 2) + ",";
  json += "\"panelCurrent\":" + String(panelCurrent, 2) + ",";
  json += "\"panelPower\":" + String(panelPower, 2) + ",";
  json += "\"batteryVoltage\":" + String(batteryVoltage, 2) + ",";
  json += "\"motorCurrent\":" + String(motorCurrent, 2) + ",";
  json += "\"fanCurrent\":" + String(fanCurrent, 2) + ",";
  json += "\"totalLoadPower\":" + String(totalLoadPower, 2) + ",";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"motorState\":" + String(motorState?"true":"false") + ",";
  json += "\"fanState\":" + String(fanState?"true":"false") + ",";
  json += "\"aiDecision\":\"" + aiDecision + "\"";
  json += "}";
  return json;
}

void handleApi() { server.send(200, "application/json", getJsonData()); }

void sendToGoogleSheets(float voltage, float current, float power, float temperature, float humidity) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setInsecure(); // Required for HTTPS endpoints on newer ESP32 cores without supplying a cert
    http.begin(GOOGLE_SCRIPT_URL);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"voltage\":" + String(voltage, 2) +
                      ",\"current\":" + String(current, 3) +
                      ",\"power\":" + String(power, 2) +
                      ",\"temperature\":" + String(temperature, 1) +
                      ",\"humidity\":" + String(humidity, 1) + "}";

    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode > 0) Serial.println("✅ Data sent to Google Sheets");
    else Serial.println("❌ Error sending data");
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_MOTOR, OUTPUT); pinMode(RELAY_FAN, OUTPUT);
  digitalWrite(RELAY_MOTOR, HIGH); digitalWrite(RELAY_FAN, HIGH);
  Wire.begin(21, 22);
  lcd.init(); lcd.backlight();
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  dht.begin();
  delay(2000);
  panelOffset = calibrateACS(PANEL_CUR_PIN);
  motorOffset = calibrateACS(MOTOR_CUR_PIN);
  fanOffset = calibrateACS(FAN_CUR_PIN);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  wifiConnected = true;
  server.on("/api/data", handleApi);
  server.begin();
}

void loop() {
  if (wifiConnected) server.handleClient();
  unsigned long now = millis();
  if (now - lastSensorRead >= 2000) {
    lastSensorRead = now;
    readAllSensors();
    aiFuzzyControl();
  }
  if (now - lastLCDUpdate >= 3000) { lastLCDUpdate = now; updateLCD(); }
  if (now - lastOLEDUpdate >= 2000) { lastOLEDUpdate = now; updateOLED(); }
  if (now - lastLogTime >= LOG_INTERVAL) { 
    lastLogTime = now; 
    logData(); 
    sendToGoogleSheets(panelVoltage, panelCurrent, panelPower, temperature, humidity);
  }
}