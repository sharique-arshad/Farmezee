

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <HardwareSerial.h>

// ----------------- LoRa UART -----------------
HardwareSerial LoRa(1);          // UART1
const int LORA_RX_PIN    = 16;   // from LoRa TX
const int LORA_TX_PIN    = 17;   // to LoRa RX
const int LORA_BAUD      = 115200;

const int LORA_THIS_ADDR = 1;    // Base address
const int LORA_PEER_ADDR = 2;    // Receiver address
const long LORA_BAND_HZ  = 868000000;

// ----------------- BME280 -----------------
Adafruit_BME280 bme;
bool bmeOK = false;

unsigned long lastBmeAttempt = 0;
const unsigned long BME_RETRY_INTERVAL = 5000;

// ----------------- Pins -----------------
#define PIN_SOIL_AO    34
#define PIN_RAIN_DO    32
#define PIN_MQ135_AO   36

#define PIN_PUMP_SIG   25
#define PIN_PUMP_LED   26
#define PIN_BUZZER     27

// ----------------- Soil calibration -----------------
const int SOIL_DRY_VALUE = 4095;
const int SOIL_WET_VALUE = 1500;

// ----------------- Crop thresholds -----------------
enum CropType {
  CROP_RICE = 0,
  CROP_WHEAT,
  CROP_MAIZE
};

struct CropThreshold {
  int onBelow;
  int offAbove;
};

CropThreshold cropThresh[3] = {
  {70, 85},  // Rice
  {40, 60},  // Wheat
  {50, 70}   // Maize
};

CropType currentCrop = CROP_RICE;

// ----------------- Modes -----------------
enum ManualMode {
  MODE_AUTO = 0,
  MODE_MANUAL_ON,
  MODE_MANUAL_OFF
};

ManualMode manualMode = MODE_AUTO;

// ----------------- Pump state -----------------
bool pumpState = false;

// ----------------- Timers -----------------
unsigned long lastSensorSend  = 0;
unsigned long lastSerialPrint = 0;

const unsigned long SENSOR_SEND_INTERVAL  = 3000;
const unsigned long SERIAL_PRINT_INTERVAL = 2000; 
// ----------------- Prototypes -----------------
void initLoRa();
void sendLoRaCommand(const String &cmd);
void sendSensorJSON(float tempC, float hum, float presshPa,
                    int soilPct, int rain, int airRaw);
void handleLoRaInput();
void processLoRaData(const String &data);

int  readSoilPercent();
bool readRainDetected();
int  readMQ135Raw();
void updatePumpState(int soilPercent, bool rainDetected);

String cropName(CropType c);
String modeName(ManualMode m);
void shortBeep();

bool initBME280(bool verbose);
void tryReinitBME280();

// ============================== SETUP ==============================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== BASE STATION START ===");

  pinMode(PIN_RAIN_DO, INPUT);
  pinMode(PIN_PUMP_SIG, OUTPUT);
  pinMode(PIN_PUMP_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_PUMP_SIG, LOW);
  digitalWrite(PIN_PUMP_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(21, 22);
  Wire.setClock(100000);
  delay(200);

  initBME280(true);

  LoRa.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(500);

  
  LoRa.setTimeout(100);

  initLoRa();

  Serial.println("Base Station ready.");
}

// ============================== LOOP ==============================

void loop() {
  tryReinitBME280();

  int soilPct = readSoilPercent();
  bool rain   = readRainDetected();
  int airRaw  = readMQ135Raw();

  float tempC = NAN, hum = NAN, press = NAN;

  if (bmeOK) {
    float t   = bme.readTemperature();
    float h   = bme.readHumidity();
    float pPa = bme.readPressure();

    if (isnan(t) || isnan(h) || isnan(pPa)) {
      Serial.println("[BME280] Read failed (NaN). Marking sensor offline.");
      bmeOK = false;
    } else {
      tempC = t;
      hum   = h;
      press = pPa / 100.0f;

      
      if (millis() - lastSerialPrint >= SERIAL_PRINT_INTERVAL) {
        lastSerialPrint = millis();
        Serial.print("BME280 -> Temp: "); Serial.print(tempC, 1); Serial.print("C | ");
        Serial.print("Hum: ");  Serial.print(hum, 1);   Serial.print("% | ");
        Serial.print("Press: ");Serial.print(press, 1); Serial.println(" hPa");
      }
    }
  }

  updatePumpState(soilPct, rain);

  handleLoRaInput();

  unsigned long now = millis();
  if (now - lastSensorSend >= SENSOR_SEND_INTERVAL) {
    lastSensorSend = now;
    int rainInt = rain ? 1 : 0;
    sendSensorJSON(tempC, hum, press, soilPct, rainInt, airRaw);
  }

 
}

// ============================== BME280 HELPERS ==============================

bool initBME280(bool verbose) {
  lastBmeAttempt = millis();
  if (verbose) Serial.println("[BME280] Initializing...");

  if (bme.begin(0x76)) {
    bmeOK = true;
    if (verbose) Serial.println("[BME280] Detected at 0x76");
    return true;
  }
  if (bme.begin(0x77)) {
    bmeOK = true;
    if (verbose) Serial.println("[BME280] Detected at 0x77");
    return true;
  }

  bmeOK = false;
  if (verbose) Serial.println("[BME280] NOT found on I2C bus");
  return false;
}

void tryReinitBME280() {
  if (!bmeOK && millis() - lastBmeAttempt >= BME_RETRY_INTERVAL) {
    Serial.println("[BME280] Offline, attempting re-init...");
    initBME280(false);
  }
}

// ============================== LoRa ==============================

void initLoRa() {
  sendLoRaCommand("AT");
  delay(300);

  sendLoRaCommand("AT+RESET");
  delay(800);

  sendLoRaCommand("AT+ADDRESS=" + String(LORA_THIS_ADDR));
  delay(300);

  sendLoRaCommand("AT+NETWORKID=5");
  delay(300);

  sendLoRaCommand("AT+PARAMETER=9,7,1,12"); 
  delay(300);

  sendLoRaCommand("AT+BAND=" + String(LORA_BAND_HZ));
  delay(600);

  Serial.println("LoRa module configured (Base).");
}

void sendLoRaCommand(const String &cmd) {
  LoRa.print(cmd);
  LoRa.print("\r\n");
  Serial.print("LoRa CMD -> ");
  Serial.println(cmd);
}

void sendSensorJSON(float tempC, float hum, float presshPa,
                    int soilPct, int rain, int airRaw) {

  String json = "{";
  json += "\"type\":\"sensor\",";
  json += "\"crop\":\"" + cropName(currentCrop) + "\",";
  json += "\"soil\":" + String(soilPct) + ",";
  json += "\"rain\":" + String(rain) + ",";
  json += "\"air\":"  + String(airRaw) + ",";
  json += "\"pump\":" + String(pumpState ? 1 : 0) + ",";
  json += "\"mode\":\"" + modeName(manualMode) + "\"";

  if (!isnan(tempC))    json += ",\"temp\":"  + String(tempC, 1);
  if (!isnan(hum))      json += ",\"hum\":"   + String(hum, 1);
  if (!isnan(presshPa)) json += ",\"press\":" + String(presshPa, 1);

  json += "}";

  String cmd = "AT+SEND=";
  cmd += String(LORA_PEER_ADDR);
  cmd += ",";
  cmd += String(json.length());
  cmd += ",";
  cmd += json;

  LoRa.print(cmd);
  LoRa.print("\r\n");

  Serial.print("LoRa SEND -> ");
  Serial.println(json);
}

// ============================== HANDLE LORA INPUT ==============================

void handleLoRaInput() {
  while (LoRa.available()) {
    String line = LoRa.readStringUntil('\n'); 
    line.trim();
    if (line.length() == 0) continue;        

    Serial.print("LoRa RAW <- ");
    Serial.println(line);

    if (!line.startsWith("+RCV=")) continue;

    int jsonStart = line.indexOf('{');
    int jsonEnd   = line.lastIndexOf('}');

    if (jsonStart < 0 || jsonEnd < 0 || jsonEnd <= jsonStart) continue;

    String data = line.substring(jsonStart, jsonEnd + 1);
    data.trim();

    Serial.print("LoRa DATA -> ");
    Serial.println(data);

    processLoRaData(data);
  }
}

// ============================== PROCESS COMMAND JSON ==============================

void processLoRaData(const String &data) {
  if (data.indexOf("\"cmd\":\"select_crop\"") >= 0) {
    if (data.indexOf("rice") >= 0) {
      currentCrop = CROP_RICE;
      Serial.println("Crop -> RICE");
      shortBeep();
    } else if (data.indexOf("wheat") >= 0) {
      currentCrop = CROP_WHEAT;
      Serial.println("Crop -> WHEAT");
      shortBeep();
    } else if (data.indexOf("maize") >= 0) {
      currentCrop = CROP_MAIZE;
      Serial.println("Crop -> MAIZE");
      shortBeep();
    }
  }

  if (data.indexOf("\"cmd\":\"manual\"") >= 0) {
    if (data.indexOf("\"mode\":\"on\"") >= 0) {
      manualMode = MODE_MANUAL_ON;
      Serial.println("Mode -> MANUAL ON");
      shortBeep();
    } else if (data.indexOf("\"mode\":\"off\"") >= 0) {
      manualMode = MODE_MANUAL_OFF;
      Serial.println("Mode -> MANUAL OFF");
      shortBeep();
    } else if (data.indexOf("\"mode\":\"auto\"") >= 0) {
      manualMode = MODE_AUTO;
      Serial.println("Mode -> AUTO");
      shortBeep();
    }
  }
}

// ============================== SENSOR READING ==============================

int readSoilPercent() {
  int raw = analogRead(PIN_SOIL_AO);
  long pct = map(raw, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

bool readRainDetected() {
  return (digitalRead(PIN_RAIN_DO) == LOW);
}

int readMQ135Raw() {
  return analogRead(PIN_MQ135_AO);
}

// ============================== PUMP CONTROL ==============================

void updatePumpState(int soilPercent, bool rainDetected) {
  bool newPumpState = pumpState;

  if (manualMode == MODE_MANUAL_ON) {
    newPumpState = true;
  } else if (manualMode == MODE_MANUAL_OFF) {
    newPumpState = false;
  } else {
    if (rainDetected) {
      newPumpState = false;
    } else {
      CropThreshold th = cropThresh[currentCrop];
      if (!pumpState && soilPercent < th.onBelow)  newPumpState = true;
      if ( pumpState && soilPercent > th.offAbove) newPumpState = false;
    }
  }

  if (newPumpState != pumpState) {
    pumpState = newPumpState;
    digitalWrite(PIN_PUMP_SIG, pumpState ? HIGH : LOW);
    digitalWrite(PIN_PUMP_LED, pumpState ? HIGH : LOW);
    Serial.print("PUMP -> ");
    Serial.println(pumpState ? "ON" : "OFF");
    shortBeep();
  }
}

// ============================== HELPERS ==============================

String cropName(CropType c) {
  switch (c) {
    case CROP_RICE:  return "rice";
    case CROP_WHEAT: return "wheat";
    case CROP_MAIZE: return "maize";
  }
  return "unknown";
}

String modeName(ManualMode m) {
  switch (m) {
    case MODE_AUTO:       return "auto";
    case MODE_MANUAL_ON:  return "on";
    case MODE_MANUAL_OFF: return "off";
  }
  return "auto";
}

void shortBeep() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(60);
  digitalWrite(PIN_BUZZER, LOW);
}
