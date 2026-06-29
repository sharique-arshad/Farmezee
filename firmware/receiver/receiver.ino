#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// LoRa UART2
HardwareSerial LoRa(2);

#define LORA_RX_PIN    16
#define LORA_TX_PIN    17
#define LORA_BAUD      115200

#define LORA_THIS_ADDR 2
#define LORA_PEER_ADDR 1

// =====================================================
// DFPLAYER UART1
// =====================================================
HardwareSerial MP3(1);
DFRobotDFPlayerMini df;

// =====================================================
// BUTTONS
// =====================================================
#define BTN_RICE    25
#define BTN_WHEAT   26
#define BTN_MAIZE   33
#define BTN_MANUAL  32

// =====================================================
// BUZZER
// =====================================================
#define BUZZER_PIN 27

// =====================================================
// TRACKS
// =====================================================
#define TRK_RICE        1
#define TRK_WHEAT       2
#define TRK_MAIZE       3
#define TRK_AUTO        4
#define TRK_MAN_ON      5
#define TRK_MAN_OFF     6
#define TRK_START       7
#define TRK_STOP        8
#define TRK_ABORT       9
#define TRK_RAIN       10

// =====================================================
// SENSOR STRUCTURE
// =====================================================
struct SensorData {

  bool valid = false;

  float tempC = NAN;
  float hum = NAN;
  float presshPa = NAN;

  int soilPct = 0;
  int rain = 0;
  int airRaw = 0;

  bool pumpOn = false;

  String cropStr = "rice";
  String modeStr = "auto";
};

SensorData sensor;

// =====================================================
// STATES
// =====================================================
bool fieldConnected = false;

unsigned long lastPacket = 0;

const unsigned long CONNECTION_TIMEOUT = 60000;

bool lastPumpState = false;
bool lastRainState = false;

// =====================================================
// BUTTON DEBOUNCE
// =====================================================
unsigned long lastPressTime = 0;

const unsigned long DEBOUNCE_MS = 200;

// =====================================================
// BUZZER STATE
// =====================================================
bool buzzerActive = false;

unsigned long buzzerEndTime = 0;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================
void splashScreen();
void showConnecting();
void showConnected();
void updateScreen();

void handleLoRa();
void parseSensorJSON(const String &d);

void readButtons();
void sendJSONToBase(const String &json);

void playTrack(int id);

void startBeep(unsigned long durationMs);
void updateBuzzer();

String extractStringField(
  const String &json,
  const String &key
);

float extractNumberField(
  const String &json,
  const String &key,
  float defaultVal
);

// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  // ===================================================
  // BUTTONS
  // ===================================================
  pinMode(BTN_RICE, INPUT_PULLUP);
  pinMode(BTN_WHEAT, INPUT_PULLUP);
  pinMode(BTN_MAIZE, INPUT_PULLUP);
  pinMode(BTN_MANUAL, INPUT_PULLUP);

  // ===================================================
  // BUZZER
  // ===================================================
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ===================================================
  // OLED
  // ===================================================
  Wire.begin(21, 22);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println("OLED FAILED");
    while (1);
  }

  splashScreen();

  // ===================================================
  // DFPLAYER
  // ===================================================
  MP3.begin(
    9600,
    SERIAL_8N1,
    4,
    15
  );

  delay(2000);

  if (!df.begin(MP3)) {

    Serial.println("DFPLAYER FAILED");

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("MP3 FAILED");
    display.display();

  } else {

    Serial.println("DFPLAYER READY");

    df.volume(20);

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("MP3 READY");
    display.display();

    startBeep(150);

    delay(1000);
  }

  // ===================================================
  // LORA
  // ===================================================
  LoRa.begin(
    LORA_BAUD,
    SERIAL_8N1,
    LORA_RX_PIN,
    LORA_TX_PIN
  );

  delay(500);

  LoRa.setTimeout(100);

  LoRa.print("AT\r\n");
  delay(300);

  LoRa.print("AT+RESET\r\n");
  delay(800);

  LoRa.print("AT+ADDRESS=2\r\n");
  delay(300);

  LoRa.print("AT+NETWORKID=5\r\n");
  delay(300);

  LoRa.print("AT+PARAMETER=9,7,1,12\r\n"); 
  delay(300);

  LoRa.print("AT+BAND=868000000\r\n");
  delay(600);

  showConnecting();
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  handleLoRa();

  readButtons();

  updateBuzzer();

  // ===================================================
  // CONNECTION TIMEOUT
  // ===================================================
  if (
    fieldConnected &&
    millis() - lastPacket > CONNECTION_TIMEOUT
  ) {

    fieldConnected = false;
    sensor.valid = false;
    showConnecting();
  }

  // ===================================================
  // UPDATE SCREEN
  // ===================================================
  if (fieldConnected && sensor.valid) {
    updateScreen();
  }
}

// =====================================================
// SPLASH SCREEN
// =====================================================
void splashScreen() {

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.setCursor(5, 5);
  display.println("FARMEZEE");

  display.setTextSize(1);

  display.setCursor(10, 35);
  display.println("Smart Irrigation");

  display.setCursor(18, 50);
  display.println("Receiver Node");

  display.display();

  delay(3000);
}

// =====================================================
// CONNECTING SCREEN
// =====================================================
void showConnecting() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 15);
  display.println("Waiting for");

  display.setCursor(0, 30);
  display.println("Field Node...");

  display.display();
}

// =====================================================
// CONNECTED SCREEN
// =====================================================
void showConnected() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 20);
  display.println("LoRa Connected");

  display.display();

 
}

// =====================================================
// MAIN OLED SCREEN
// =====================================================
void updateScreen() {

  display.clearDisplay();

  display.setTextSize(1);

  // ===================================================
  // TEMP + HUM
  // ===================================================
  display.setCursor(0, 0);

  display.print("T:");
  display.print(sensor.tempC, 1);
  display.print("C ");

  display.print("H:");
  display.print(sensor.hum, 0);
  display.print("%");

  // ===================================================
  // SOIL
  // ===================================================
  display.setCursor(0, 16);

  display.print("Soil:");
  display.print(sensor.soilPct);
  display.print("%");

  // ===================================================
  // PUMP + RAIN
  // ===================================================
  display.setCursor(0, 32);

  display.print("Pump:");

  if (sensor.pumpOn)
    display.print("ON");
  else
    display.print("OFF");

  display.print(" ");

  display.print("Rain:");

  if (sensor.rain)
    display.print("Y");
  else
    display.print("N");

  // ===================================================
  // CROP + MODE
  // ===================================================
  display.setCursor(0, 48);

  display.print(sensor.cropStr);

  display.print(" | ");

  display.print(sensor.modeStr);

  display.display();

  // ===================================================
  // PUMP ALERTS
  // ===================================================
  if (sensor.pumpOn != lastPumpState) {

    if (sensor.pumpOn) {
      playTrack(TRK_START);
      startBeep(1200);
    } else {
      playTrack(TRK_STOP);
      startBeep(800);
    }

    lastPumpState = sensor.pumpOn;
  }
}

// =====================================================
// HANDLE LORA - FIXED: timeout + drain full buffer
// =====================================================
void handleLoRa() {

  while (LoRa.available()) {

    String line = LoRa.readStringUntil('\n');
    line.trim();

    Serial.println("RAW: " + line);

    if (!line.startsWith("+RCV="))
      continue;                              /
    int jsonStart = line.indexOf('{');
    int jsonEnd   = line.lastIndexOf('}');

    if (
      jsonStart < 0 ||
      jsonEnd   < 0 ||
      jsonEnd  <= jsonStart
    ) continue;

    String json = line.substring(jsonStart, jsonEnd + 1);

    Serial.println("JSON: " + json);

    lastPacket = millis();

    if (!fieldConnected) {
      fieldConnected = true;
      showConnected();
    }

    parseSensorJSON(json);
  }
}

// =====================================================
// PARSE JSON
// =====================================================
void parseSensorJSON(const String &d) {

  String typeStr =
    extractStringField(d, "\"type\":\"");

  if (typeStr != "sensor")
    return;

  sensor.valid = true;

  sensor.cropStr =
    extractStringField(d, "\"crop\":\"");

  sensor.modeStr =
    extractStringField(d, "\"mode\":\"");

  sensor.soilPct =
    extractNumberField(d, "\"soil\":", sensor.soilPct);

  sensor.rain =
    extractNumberField(d, "\"rain\":", sensor.rain);

  sensor.airRaw =
    extractNumberField(d, "\"air\":", sensor.airRaw);

  sensor.tempC =
    extractNumberField(d, "\"temp\":", sensor.tempC);

  sensor.hum =
    extractNumberField(d, "\"hum\":", sensor.hum);

  sensor.presshPa =
    extractNumberField(d, "\"press\":", sensor.presshPa);

  sensor.pumpOn =
    (bool)(extractNumberField(d, "\"pump\":", sensor.pumpOn));

  // ===================================================
  // RAIN ALERT
  // ===================================================
  if (!lastRainState && sensor.rain == 1) {
    playTrack(TRK_RAIN);
    startBeep(1500);
  }

  lastRainState = (sensor.rain == 1);
}

// =====================================================
// EXTRACT STRING
// =====================================================
String extractStringField(
  const String &json,
  const String &key
) {

  int p = json.indexOf(key);

  if (p < 0)
    return "";

  int s = json.indexOf('"', p + key.length() - 1);

  if (s < 0)
    return "";

  int e = json.indexOf('"', s + 1);

  if (e < 0)
    return "";

  return json.substring(s + 1, e);
}

// =====================================================
// EXTRACT NUMBER
// =====================================================
float extractNumberField(
  const String &json,
  const String &key,
  float defaultVal
) {

  int p = json.indexOf(key);

  if (p < 0)
    return defaultVal;

  int s = json.indexOf(':', p);

  if (s < 0)
    return defaultVal;

  s++;

  int e = json.indexOf(',', s);

  if (e < 0)
    e = json.indexOf('}', s);

  if (e < 0)
    return defaultVal;

  String numStr = json.substring(s, e);
  numStr.trim();

  return numStr.toFloat();
}

// =====================================================
// BUTTONS
// =====================================================
void readButtons() {

  if (millis() - lastPressTime < DEBOUNCE_MS) return;

  // ===================================================
  // RICE
  // ===================================================
  if (!digitalRead(BTN_RICE)) {

    sendJSONToBase(
      "{\"cmd\":\"select_crop\",\"crop\":\"rice\"}"
    );

    playTrack(TRK_RICE);
    startBeep(150);
    sensor.cropStr = "rice";
    lastPressTime = millis();
  }

  // ===================================================
  // WHEAT
  // ===================================================
  if (!digitalRead(BTN_WHEAT)) {

    sendJSONToBase(
      "{\"cmd\":\"select_crop\",\"crop\":\"wheat\"}"
    );

    playTrack(TRK_WHEAT);
    startBeep(150);
    sensor.cropStr = "wheat";
    lastPressTime = millis();
  }

  // ===================================================
  // MAIZE
  // ===================================================
  if (!digitalRead(BTN_MAIZE)) {

    sendJSONToBase(
      "{\"cmd\":\"select_crop\",\"crop\":\"maize\"}"
    );

    playTrack(TRK_MAIZE);
    startBeep(150);
    sensor.cropStr = "maize";
    lastPressTime = millis();
  }

  // ===================================================
  // MANUAL BUTTON
  // ===================================================
  if (!digitalRead(BTN_MANUAL)) {

    if (sensor.modeStr == "auto") {

      sendJSONToBase(
        "{\"cmd\":\"manual\",\"mode\":\"on\"}"
      );

      playTrack(TRK_MAN_ON);
      sensor.modeStr = "on";

    } else if (sensor.modeStr == "on") {

      sendJSONToBase(
        "{\"cmd\":\"manual\",\"mode\":\"off\"}"
      );

      playTrack(TRK_MAN_OFF);
      sensor.modeStr = "off";

    } else {

      sendJSONToBase(
        "{\"cmd\":\"manual\",\"mode\":\"auto\"}"
      );

      playTrack(TRK_AUTO);
      sensor.modeStr = "auto";
    }

    startBeep(150);
    lastPressTime = millis();
  }
}

// =====================================================
// SEND JSON
// =====================================================
void sendJSONToBase(const String &json) {

  String cmd = "AT+SEND=";
  cmd += String(LORA_PEER_ADDR);
  cmd += ",";
  cmd += String(json.length());
  cmd += ",";
  cmd += json;

  LoRa.print(cmd);
  LoRa.print("\r\n");

  Serial.print("SEND -> ");
  Serial.println(json);
}

// =====================================================
// PLAY AUDIO
// =====================================================
void playTrack(int id) {

  if (id > 0) {
    df.play(id);
  }
}

// =====================================================
// START BEEP
// =====================================================
void startBeep(unsigned long durationMs) {

  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerEndTime = millis() + durationMs;
}

// =====================================================
// UPDATE BUZZER
// =====================================================
void updateBuzzer() {

  if (buzzerActive && millis() >= buzzerEndTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}
