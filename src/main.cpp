#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>
#include <TFT_eSPI.h>          
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>

#define WIFI_SSID "T8-Arduino"
#define WIFI_PASSWORD "T8-Arduino"

#define ADC_MQ135 A0 // MQ-135 air quality sensor connected to ADC pin A0 
#define ADC_MIC A1   // MAX4466 microphone output connected to ADC pin A1
#define GPIO_PIR A2   // HC-SR501 PIR motion sensor output connected to GPIO -Digital pin A2

#define BTNPIN 0     // User button
#define LEDPIN 13    // Built-in LED





























//Libraries used in this project:
// WiFi.h              — Wi-Fi connectivity
// AsyncTCP.h          — Async TCP base layer
// ESPAsyncWebServer.h — Async web + WebSocket server
// LittleFS.h          — On-flash file system (HTML/CSS/JS)
// Arduino_JSON.h      — JSON building / parsing
// Adafruit_GFX.h      — Graphics primitives for TFT
// Adafruit_ST7789.h   — Driver for the TFT display
// Adafruit_NeoPixel.h — NeoPixel status LED
// Adafruit_AHTX0.h    — AHT20 temp + humidity sensor
// Adafruit_BMP280.h   — BMP280 pressure + temp sensor
// BH1750.h            — BH1750 light sensor (lux)
//  (MQ-135 & MAX4466 read directly via analogRead — no extra lib)

// Pin definitions:  
    // I²C SDA / SCL       — shared bus: AHTX0, BMP280, BH1750
    // ADC_MQ135           — analog in from MQ-135 AOUT pin
    // ADC_MIC             — analog in from MAX4466 OUT pin
    // GPIO_PIR            — digital in from HC-SR501 OUT pin
    // TFT_CS / DC / RST   — SPI chip-select, data/cmd, reset
    // TFT_BACKLITE        — backlight enable
    // TFT_I2C_POWER       — I²C power rail enable
    // PIN_NEOPIXEL        — on-board NeoPixel data
    // BTNPIN (GPIO 2)     — user button
    // LEDPIN (GPIO 13)    — built-in LED
//global objects and variables:
    // Sensor driver objects (AHTX0, BMP280, BH1750)
// TFT screen object (Adafruit_ST7789)
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

//Initilisation helpers

// initSerial()
//   - Start serial at 115200 baud
// Block until serial is ready (with timeout to avoid infinite block if no serial)

// initPins()
//   - Set pin modes for TFT, NeoPixel, sensors, button, LED

// initLittleFS()
//   - Call LittleFS.begin() and check for success

// initsensors()
//   - Initialize AHTX0, BMP280, BH1750 sensors

//initWiFi()
//   - Connect to Wi-Fi using credentials from secrets.h
//   - Use WiFi.begin() and wait for connection
//   - Print local IP address once connected

// initTFT()
//   - Initialize the TFT display
//   - Set rotation, fill screen, set text color/size
//  - Display a "Connecting to Wi-Fi..." message until Wi-Fi is connected

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
//  - The HTML page should establish a WebSocket connection to "/ws" and update the UI based on incoming JSON messages


// Sensor reading and broadcasting:

// readTempHumidityPressure()
//   - Read temperature and humidity from AHTX0
//   - Read temperature and pressure from BMP280
//   - Average the two temperature readings for a more stable value
//   -return true if successful, false if any sensor read failed

// readAirQuality()
//   - Read the raw ADC value from the MQ-135 sensor
//   - Implement a warm-up period (e.g. first 30 seconds after startup) where the value is ignored and mq135Ready is set to false
//   - After warm-up, set mq135Ready to true and return the ADC value
//  - This raw value can be sent to the client, which can apply its own calibration or thresholds

// readLight()
//   - Read the lux value from the BH1750 sensor
//  - Return the lux value

// readNoise()
//   - Read the raw ADC value from the MAX4466 microphone output
//   - Convert this to a relative dB value (e.g. using a logarithmic scale)
//  - Return the noise level in dB

// readAllSensors()
//   - Call the above functions to read all sensors
//   - Update the global variables with the latest readings
//   - Return true if all reads were successful, false if any failed

//Display Functions:

// drawSplashScreen()
//   - Display a splash screen with the project name and a loading animation while sensors are initializing
//   -clear the screen and show a "School Environmental Monitor" title
//   -draw wifi conection animation (e.g. rotating arcs or dots) until Wi-Fi is connected

// drawDashboard()
//   - Clear the screen and display the latest sensor readings in a clean layout
//   - Show temperature, humidity, pressure, air quality, light level, and noise level
//   -called once after boot to set up the layout, then updated with new values as they come in

// updateDisplayValues()
//  -Erase the old sensor values (e.g. by drawing filled rectangles over them)
//  -Draw the new sensor values in the same positions
//  -color-code the air quality and noise levels (e.g. green/yellow/red) based on thresholds

// updateNeoPixel()
//  - Set the NeoPixel colour based on the latest sensor readings
//  - For example, use the air quality level to determine the colour:
//    - Good (low MQ-135 value): Green

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
//  - Log the new connection (client->id())
// WS_EVT_DISCONNECT:
//  - Log the disconnection (client->id())
// WS_EVT_DATA:
//  - Call handleWebSocketMessage() with the received data
// WS_EVT_PONG:
//  - Handle pong responses if needed (e.g. for keep-alive)

//setup():
// Call all the initialization functions in the correct order:
// 1. initSerial()
// 2. initPins()
// 3. initLittleFS()
// 4. initWiFi()
// 5. initTFT()
// 6. initNeoPixel()
// 7. initWebSocket()
// 8. initWebServer()

//loop():
// Use millis() to manage timing for sensor reads and broadcasts:
// unsigned long currentMillis = millis();
// if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) { 
//   if (readAllSensors()) {
//     updateDisplayValues();
//     updateNeoPixel();
//     lastSensorRead = currentMillis;