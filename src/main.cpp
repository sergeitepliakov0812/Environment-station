#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include "secrets.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>

#define I2C_SDA_PIN     3       // STEMMA QT SDA (Feather ESP32-S3)
#define I2C_SCL_PIN     4       // STEMMA QT SCL (Feather ESP32-S3)

#define ADC_MQ135       A0      // MQ-135 analog out  (GPIO 17)
#define MQ135_DO_PIN    10      // MQ-135 digital out (free GPIO)
#define ADC_MIC         A1      // MAX4466 analog out (GPIO 18)
#define BUZZER_PIN      11      // Active buzzer (free GPIO)

#define MQ135_ALERT_THRESHOLD  1500 // ADC value threshold for air quality alert
#define MQ135_ALERT_COUNT 3 // Number of consecutive readings above threshold to trigger alert
const unsigned long ALERT_COOLDOWN_MS = 30000; // 30 seconds cooldown between alerts
const unsigned long SENSOR_INTERVAL    = 2000;   // ms between sensor reads
const unsigned long MQ135_WARMUP_MS    = 30000;  // 30-second MQ-135 warm-up
const unsigned int  NOISE_SAMPLE_MS    = 50;     // peak-to-peak window for mic

Adafruit_AHTX0   aht; // AHT20 temperature and humidity sensor
Adafruit_BMP280  bmp; // BMP280 pressure and temperature sensor
BH1750           lightMeter; // BH1750 light sensor

unsigned long lastAlertTime = 0; // Timestamp of the last alert
int alertStreak = 0; // Counter for consecutive readings above threshold
float temperature    = 0.0f;   // °C  (averaged AHT + BMP)
float humidity       = 0.0f;   // %RH (AHT)
float pressure       = 0.0f;   // hPa (BMP)
int   airQualityRaw  = 0;      // 0–4095 ADC
bool  mq135Ready     = false;  // MQ-135 warm-up complete
float lux            = 0.0f;   // lux (BH1750)
float noiseDB        = 0.0f;   // relative dB (MAX4466)
//------------- Sensor state variables --------------
unsigned long lastSensorRead = 0; // Timestamp of the last sensor read
unsigned long lastIpPrint = 0; // Timestamp of the last IP address print
bool ahtOK = false; // AHT20 sensor initialized successfully
bool bmpOK = false; // BMP280 sensor initialized successfully
bool bh1750OK = false; // BH1750 sensor initialized successfully
bool lightOK = false; // Light sensor initialized successfully
//------------- Screen setup --------------
Adafruit_ST7789 tftScreen = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);


//------------- Neopixel setup --------------
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
//these values give a green light on neopixel
int red = 0;
int green = 255;
int blue = 0;


//------------- WiFi setup --------------
//the Wifi status
//int status = WL_IDLE_STATUS;

//Your network name and password are hidden in the secrets.h file
const char SSID[] = SECRET_SSID;
const char PASSWORD[] = SECRET_PASS;

//------------- WebServer setup --------------
// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");
//------------- Function declarations --------------
void initSerial();
void initPins();
void initLittleFS();
void initWiFi();
void initSensors();
void initWebSocket();
void initWebServer();
void initBuzzer();
void buzzerBeep(unsigned int frequency, unsigned int durationMs);
void buzzerAlert();
void buzzerReady();
bool readTempHumidityPressure();
int readAirQuality();
float readLight();
float readNoise();
bool readAllSensors();
void printAllSensors();
String getOutputStates();
void notifyClients(String state);
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
String readFile(fs::FS& fs, const char* path);
void writeFile(fs::FS& fs, const char* path, const char* message);
//------------- Setup function --------------
void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      //Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.print(" connected from ");
      Serial.println(client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      //Serial.printf("WebSocket client #%u disconnected\n", client->id());
      Serial.print("WebSocket client ");
      Serial.print(client->id());
      Serial.println(" disconnected\n");
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
//------------- LittleFS setup --------------
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS failed to setup");
  } else {
    Serial.println("LittleFS should be going now");
  }
}

/**
LittleFS reading a file 
*/
String readFile(fs::FS& fs, const char* path) {
  Serial.print("Reading file: ");
  Serial.println(path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("readFile failed to open file");
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

/**
LittleFS writing a file
*/
void writeFile(fs::FS& fs, const char* path, const char* message) {
  Serial.print("Writing file: ");
  Serial.println(path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("writeFile failed to write to file");
    return;
  }
  if (file.print(message)) {
    Serial.println("- writeFile success");
  } else {
    Serial.println("- writeFile failed");
  }
}
//------------- Sensor reading and JSON output --------------
String getOutputStates() {
JSONVar myArray;

myArray["values"][0]["name"] = "temperature";
myArray["values"][0]["value"] = temperature;
myArray["values"][0]["unit"] = "°C";

myArray["values"][1]["name"] = "humidity";
myArray["values"][1]["value"] = humidity;
myArray["values"][1]["unit"] = "%RH";

myArray["values"][2]["name"] = "pressure";
myArray["values"][2]["value"] = pressure;
myArray["values"][2]["unit"] = "hPa";

myArray["values"][3]["name"] = "light";
myArray["values"][3]["value"] = lux;
myArray["values"][3]["unit"] = "lux";

myArray["values"][4]["name"] = "airQuality";
myArray["values"][4]["value"] = mq135Ready ? airQualityRaw : -1;
myArray["values"][4]["unit"] = "raw ADC";
myArray["values"][4]["ready"] = mq135Ready;

myArray["values"][5]["name"] = "noise";
myArray["values"][5]["value"] = noiseDB;
myArray["values"][5]["unit"] = "dB";

String jsonString = JSON.stringify(myArray);
return JSON.stringify(myArray);
}
//------------- WebSocket notification --------------
void notifyClients(String state) {
  ws.textAll(state);
}
//------------- WiFi setup --------------
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}
//------------- WebSocket setup --------------
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  Serial.println("WebSocket initialized");
}
//------------- WebServer setup --------------
void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html", false);
  });
  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println("Web server started");
}
//------------- WebSocket message handling --------------
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    //if message is states send the state of all gpio pins
    if (strcmp((char*)data, "getStates") == 0) {
      notifyClients(getOutputStates());
    } else {
      //(char*)data should be in this format "rgb(255,87,51)" when it isn't 'getStates'
      std::string rgb = (char*)data;
      byte firstComma = rgb.find(',');
      byte secondComma = rgb.find(',', firstComma + 1);
      byte lastBracket = rgb.find(')');

      red = stoi(rgb.substr(4, firstComma));
      green = stoi(rgb.substr(firstComma + 1, secondComma - firstComma - 1));
      blue = stoi(rgb.substr(secondComma + 1, lastBracket));
    }
    //Just to alternate the led on and off to show we can digitalRead and digitalWrite here
    //digitalWrite(LEDPIN, !digitalRead(LEDPIN)); //AM I AN ISSUE NEED TO TEST--------------------------------------
  }
}
 
//------------- Initialization functions --------------
void initSerial() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1000);
  Serial.println();
  Serial.println(F("=== School Environmental Monitor — Sensor Test ==="));
}
//------------- Pin setup --------------
void initPins() {
  pinMode(MQ135_DO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
 
  analogReadResolution(12);         // 12-bit ADC: 0–4095
  analogSetAttenuation(ADC_11db);   // full 0–3.3 V range
 
  Serial.println(F("[pins] OK"));
}
//------------- Sensor setup --------------
void initSensors() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
 
  // AHT20
  ahtOK = aht.begin();
  Serial.printf("[AHT20]  %s\n", ahtOK ? "OK" : "NOT FOUND — check wiring");
 
  // BMP280 (default address 0x77, some boards use 0x76)
  bmpOK = bmp.begin(0x77) || bmp.begin(0x76);
  if (bmpOK) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println(F("[BMP280] OK"));
  } else {
    Serial.println(F("[BMP280] NOT FOUND — check wiring"));
  }
 
  // BH1750
  lightOK = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.printf("[BH1750] %s\n", lightOK ? "OK" : "NOT FOUND — check wiring");
 
  // MQ-135 warm-up notice
  Serial.printf("[MQ-135] warming up for %lu s...\n", MQ135_WARMUP_MS / 1000);
}

// Buzzer alert

  void buzzerBeep(unsigned int frequency, unsigned int durationMs) {
    tone(BUZZER_PIN, frequency, durationMs);
    delay(durationMs + 50);  // Wait for the tone to finish plus a small gap
    noTone(BUZZER_PIN);
  }

  void initBuzzer() {
    buzzerBeep(1000, 80); 
    delay(60);
    buzzerBeep(1500, 80);
    Serial.println(F("[Buzzer] OK"));
  }

  void buzzerAlert() {
  for (uint8_t i = 0; i < 3; i++) {
    // Sweep up
    for (int freq = 800; freq <= 2400; freq += 80) {
      tone(BUZZER_PIN, freq);
      delay(8);
    }
    // Sweep down
    for (int freq = 2400; freq >= 800; freq -= 80) {
      tone(BUZZER_PIN, freq);
      delay(8);
    }
  }
  noTone(BUZZER_PIN);
}
// Buzzer ready sound
 void buzzerReady() {
  // Ascending power-up sweep, like a system spinning up
  for (int freq = 300; freq <= 1800; freq += 50) {
    tone(BUZZER_PIN, freq);
    delay(4);
  }
  noTone(BUZZER_PIN);
  delay(70);

  // Crisp two-tone "system online" confirmation
  buzzerBeep(1400, 55);  delay(25);
  buzzerBeep(1800, 140);
}

// Returns true if at least one source succeeded.
bool readTempHumidityPressure() {
  float ahtTemp = NAN, ahtHum = NAN;
  float bmpTemp = NAN, bmpPres = NAN;
 
  if (ahtOK) {
    sensors_event_t hEvent, tEvent;
    if (aht.getEvent(&hEvent, &tEvent)) {
      ahtTemp = tEvent.temperature;
      ahtHum  = hEvent.relative_humidity;
    }
  }

  if (bmpOK) {
    bmpTemp = bmp.readTemperature();
    bmpPres = bmp.readPressure() / 100.0f;   // Pa → hPa
  }
 
  // Average temperature from whichever sensors responded
  if (!isnan(ahtTemp) && !isnan(bmpTemp)) {
    temperature = (ahtTemp + bmpTemp) / 2.0f;
  } else if (!isnan(ahtTemp)) {
    temperature = ahtTemp;
  } else if (!isnan(bmpTemp)) {
    temperature = bmpTemp;
  } else {
    return false;
  }
 
  if (!isnan(ahtHum))  humidity  = ahtHum;
  if (!isnan(bmpPres)) pressure  = bmpPres;
  return true;
}
 
// Returns raw ADC value; sets mq135Ready after warm-up period.
int readAirQuality() {
  if (!mq135Ready) {
    if (millis() >= MQ135_WARMUP_MS) {
      mq135Ready = true;
      Serial.println(F("[MQ-135] warm-up complete — readings now valid"));
    } else {
      return -1;   // still warming up
    }
  }
  airQualityRaw = analogRead(ADC_MQ135);
  return airQualityRaw;
}
 
// Returns lux value from BH1750; -1 on failure.
float readLight() {
  if (!lightOK) return -1.0f;
  if (lightMeter.measurementReady()) {
    lux = lightMeter.readLightLevel();
  }
  return lux;
}
 
// Samples MAX4466 over NOISE_SAMPLE_MS and converts peak-to-peak to relative dB.
float readNoise() {
  unsigned long start = millis();
  int sampleMax = 0, sampleMin = 4095;
 
  while (millis() - start < NOISE_SAMPLE_MS) {
    int s = analogRead(ADC_MIC);
    if (s > sampleMax) sampleMax = s;
    if (s < sampleMin) sampleMin = s;
  }
 // Calculate peak-to-peak value
  int peakToPeak = sampleMax - sampleMin;
  // Map peak-to-peak (noise floor ~30 to max 4095) onto a 0–100 dB relative scale
  if (peakToPeak < 1) peakToPeak = 1;
  noiseDB = 20.0f * log10f((float)peakToPeak / 4095.0f * 100.0f + 1.0f);
  return noiseDB;
}
 
// Calls all readers; returns true if core sensors (temp/humidity) succeeded.
bool readAllSensors() {
  bool ok = readTempHumidityPressure();
  readAirQuality();
  readLight();
  readNoise();
  return ok;
}
// Prints all sensor readings to Serial in a human-readable format.
void printAllSensors() {
  Serial.println(F("──────────────────────────────────"));
 
  // Temperature
  Serial.printf("Temp     : %.2f °C\n", temperature);
 
  // Humidity
  Serial.printf("Humidity : %.2f %%RH\n", humidity);
 
  // Pressure
  Serial.printf("Pressure : %.2f hPa\n", pressure);
 
  // Light
  if (lightOK) {
    Serial.printf("Light    : %.2f lx\n", lux);
  } else {
    Serial.println(F("Light    : sensor error"));
  }
 
  // Air quality
  if (!mq135Ready) {
    unsigned long remaining = (MQ135_WARMUP_MS - millis()) / 1000;
    Serial.printf("MQ-135   : warming up (%lu s left)\n", remaining);
  } else {
   bool aboveThreshold = (airQualityRaw > MQ135_ALERT_THRESHOLD); // Check if the reading is above the threshold
    if (aboveThreshold) alertStreak++; else alertStreak = 0; // Increment or reset the alert streak counter
  bool alert = (alertStreak >= MQ135_ALERT_COUNT); // Determine if we should trigger an alert based on the streak
    Serial.printf("MQ-135   : raw %4d | DO: %s\n", 
                  airQualityRaw,
                  alert ? "ALERT" : "clear");
    if (alert && (millis() - lastAlertTime >= ALERT_COOLDOWN_MS)) { // Check cooldown
  lastAlertTime = millis(); // Update last alert time
  buzzerAlert();
  }
}
  // Noise
  Serial.printf("Noise    : %.1f dB (relative)\n", noiseDB);  
} 
//------------- Setup and loop --------------
void setup() {
initSerial();
  initPins();
  initLittleFS();
  initWiFi();
  initSensors();
  initWebSocket();
  initWebServer();
  initBuzzer();
  Serial.println(F("[setup] complete — entering main loop"));
  Serial.println();
  buzzerReady();
}
 
void loop() {
 unsigned long now = millis();

  ws.cleanupClients();

  if (now - lastIpPrint >= 5000) {
    lastIpPrint = now;
    Serial.print("[IP] ");
    Serial.println(WiFi.localIP());
  }

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readAllSensors();
    printAllSensors();
    notifyClients(getOutputStates());
  }
}

// Libraries used in this project:
// WiFi.h              — Wi-Fi connectivity
// AsyncTCP.h          — Async TCP base layer
// ESPAsyncWebServer.h — Async web + WebSocket server
// LittleFS.h          — On-flash file system (HTML/CSS/JS)
// Arduino_JSON.h      — JSON building / parsing
// Adafruit_AHTX0.h    — AHT20 temp + humidity sensor
// Adafruit_BMP280.h   — BMP280 pressure + temp sensor
// BH1750.h            — BH1750 light sensor (lux)
//  (MQ-135 & MAX4466 read directly via analogRead — no extra lib)

// Pin definitions:  
//     I²C SDA / SCL       — shared bus: AHTX0, BMP280, BH1750
//     ADC_MQ135           — analog in from MQ-135 AOUT pin
//     ADC_MIC             — analog in from MAX4466 OUT pin
//     GPIO_PIR            — digital in from HC-SR501 OUT pin
//     PIN_NEOPIXEL        — on-board NeoPixel data
//     BTNPIN (GPIO 2)     — user button
//     LEDPIN (GPIO 13)    — built-in LED
// global objects and variables:
//     Sensor driver objects (AHTX0, BMP280, BH1750)
// NeoPixel object (1 pixel)
// AsyncWebServer object on port 80
// AsyncWebSocket object on path "/ws"

// Struct or plain variables to hold latest sensor readings:
//   float temperature    (°C, averaged from AHT + BMP)
//   float humidity       (% RH, from AHT)
//   float pressure       (hPa, from BMP)
//   int   airQualityRaw  (0–4095 ADC, from MQ-135)
//   float lux            (lux, from BH1750)
//   float noiseDB        (relative dB, from MAX4466)

// Timing variables:
//   unsigned long lastSensorRead   (millis timestamp)
//   unsigned long lastBroadcast    (millis timestamp)
//   const long SENSOR_INTERVAL     (e.g. 2000 ms)
//   const long BROADCAST_INTERVAL  (e.g. 5000 ms)

// NeoPixel colour state:
//   int red, green, blue

// MQ-135 warm-up flag:
//   bool mq135Ready

// Initialization helpers

// initSerial()
//   - Start serial at 115200 baud
// Block until serial is ready (with timeout to avoid infinite block if no serial)

// initPins()
//   - Set pin modes for sensors, button, LED

// initLittleFS()
//   - Call LittleFS.begin() and check for success

// initSensors()
//   - Initialize AHTX0, BMP280, BH1750 sensors

// initWiFi()
//   - Connect to Wi-Fi using credentials from secrets.h
//   - Use WiFi.begin() and wait for connection
//   - Print local IP address once connected

// initNeoPixel()
//   - Initialize the NeoPixel object
//   - Set initial colour (e.g. blue for startup)
//   - Call show() to update the LED

// initwebsocket()
//   - Set up AsyncWebSocket event handlers for connect/disconnect/message
//   - Add the WebSocket to the AsyncWebServer

// initWebServer()
//   - Define routes for serving HTML/CSS/JS files from LittleFS
//   - Start the AsyncWebServer
//   - The HTML page should establish a WebSocket connection to "/ws" and update the UI based on incoming JSON messages

// Sensor reading and broadcasting:

// readTempHumidityPressure()
//   - Read temperature and humidity from AHTX0
//   - Read temperature and pressure from BMP280
//   - Average the two temperature readings for a more stable value
//   - return true if successful, false if any sensor read failed

// readAirQuality()
//   - Read the raw ADC value from the MQ-135 sensor
//   - Implement a warm-up period (e.g. first 30 seconds after startup) where the value is ignored and mq135Ready is set to false
//   - After warm-up, set mq135Ready to true and return the ADC value
//   - This raw value can be sent to the client, which can apply its own calibration or thresholds

// readLight()
//   - Read the lux value from the BH1750 sensor
//   - Return the lux value

// readNoise()
//   - Read the raw ADC value from the MAX4466 microphone output
//   - Convert this to a relative dB value (e.g. using a logarithmic scale)
//   - Return the noise level in dB

// readAllSensors()
//   - Call the above functions to read all sensors
//   - Update the global variables with the latest readings
//   - Return true if all reads were successful, false if any failed

// Display Functions:

// drawSplashScreen()
//   - Display a splash screen with the project name and a loading animation while sensors are initializing
//   - clear the screen and show a "School Environmental Monitor" title
//   - draw wifi conection animation (e.g. rotating arcs or dots) until Wi-Fi is connected

// drawDashboard()
//   - Clear the screen and display the latest sensor readings in a clean layout
//   - Show temperature, humidity, pressure, air quality, light level, and noise level
//   - called once after boot to set up the layout, then updated with new values as they come in

// updateDisplayValues()
//   - Erase the old sensor values (e.g. by drawing filled rectangles over them)
//   - Draw the new sensor values in the same positions
//   - color-code the air quality and noise levels (e.g. green/yellow/red) based on thresholds

// updateNeoPixel()
//   - Set the NeoPixel colour based on the latest sensor readings
//   - For example, use the air quality level to determine the colour:
//     - Good (low MQ-135 value): Green

// Websocket and Network Functions:

// builsdSensorJSON()
//   - Create a JSON object containing the latest sensor readings
//   - Convert the JSON object to a string and return it
//   - This string will be sent to the client via WebSocket

// notifyClients()
//   - Call buildSensorJSON() to get the latest sensor data as a JSON string
//   - Use ws.textAll() to send this string to all connected WebSocket clients
//   - This function can be called after each sensor read or on a timer to keep clients

// handleWebSocketMessage()
//   - Parse incoming messages from clients (if needed)
//   - For example, clients could send commands to change the display mode or request specific data
//   - This function will be set as the event handler for WebSocket messages
//   - Any unrecognized messages can be ignored or logged for debugging

// onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
// switch on EventType:
// WS_EVT_CONNECT:
//   - Log the new connection (client->id())
// WS_EVT_DISCONNECT:
//   - Log the disconnection (client->id())
// WS_EVT_DATA:
//   - Call handleWebSocketMessage() with the received data
// WS_EVT_PONG:
//   - Handle pong responses if needed (e.g. for keep-alive)

// setup():
// Call all the initialization functions in the correct order:
// 1. initSerial()
// 2. initPins()
// 3. initLittleFS()
// 4. initWiFi()
// 5. initNeoPixel()
// 6. initWebSocket()
// 7. initWebServer()

// loop():
// Use millis() to manage timing for sensor reads and broadcasts:
// unsigned long currentMillis = millis();
// if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
//   if (readAllSensors()) {
//     lastSensorRead = currentMillis;
