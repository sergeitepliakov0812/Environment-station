#include <Arduino.h>

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

