#include "BluetoothSerial.h"
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT11
#define LIGHTPIN 34
#define LEDPIN 2
#define BUZZERPIN 18
#define PIRPIN 27

BluetoothSerial SerialBT;
DHT dht(DHTPIN, DHTTYPE);

// AUTO = sensor-driven control
// MANUAL = phone directly controls LED and buzzer
String mode = "AUTO";

// Adjustable alert thresholds
float tempThreshold = 31.0;
int lightThreshold = 900;

// Motion arm state
bool motionArmed = true;

// Latest sensor readings
float temperature = 0.0;
float humidity = 0.0;
int lightValue = 0;
int motionDetected = 0;

// Last non-normal alert event only
String lastEvent = "No alerts yet";
String currentEvent = "NONE";

// Timing for sensor updates
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000;

// Streaming control
bool streamingEnabled = false;
unsigned long lastStreamSend = 0;
const unsigned long streamInterval = 1500;

// ---------------- HELPER: ALERT STATE ----------------
bool isAlertActive() {
  bool motionAlert = motionArmed && (motionDetected == 1);
  return (temperature >= tempThreshold) || (lightValue < lightThreshold) || motionAlert;
}

// ---------------- HELPER: ALERT REASON ----------------
String getAlertReason() {
  bool tempAlert = (temperature >= tempThreshold);
  bool lightAlert = (lightValue < lightThreshold);
  bool motionAlert = motionArmed && (motionDetected == 1);

  String reason = "";

  if (tempAlert) {
    reason += "HIGH_TEMP";
  }

  if (lightAlert) {
    if (reason.length() > 0) reason += " + ";
    reason += "LOW_LIGHT";
  }

  if (motionAlert) {
    if (reason.length() > 0) reason += " + ";
    reason += "MOTION_DETECTED";
  }

  if (reason == "") {
    reason = "NONE";
  }

  return reason;
}

// ---------------- SENSOR READING + AUTO ALERT ----------------
void readSensors() {
  if (millis() - lastSensorRead >= sensorInterval) {
    lastSensorRead = millis();

    float newHumidity = dht.readHumidity();
    float newTemperature = dht.readTemperature();
    int newLightValue = analogRead(LIGHTPIN);
    int newMotionValue = digitalRead(PIRPIN);

    if (!isnan(newHumidity) && !isnan(newTemperature)) {
      humidity = newHumidity;
      temperature = newTemperature;
    }

    lightValue = newLightValue;
    motionDetected = newMotionValue;

    // AUTO mode: ESP32 controls LED and buzzer based on thresholds + armed motion
    if (mode == "AUTO") {
      bool alertState = isAlertActive();
      String alertReason = getAlertReason();

      digitalWrite(LEDPIN, alertState ? HIGH : LOW);
      digitalWrite(BUZZERPIN, alertState ? HIGH : LOW);

      // Update only when a non-normal alert event occurs
      if (alertState) {
        currentEvent = "ALERT: " + alertReason;

        if (currentEvent != lastEvent) {
          lastEvent = currentEvent;
        }
      }
    }
  }
}

// ---------------- STATUS RESPONSE ----------------
void sendStatus() {
  bool alertState = isAlertActive();
  String alertReason = getAlertReason();

  SerialBT.print("STATUS|MODE=");
  SerialBT.print(mode);

  SerialBT.print("|STREAM=");
  SerialBT.print(streamingEnabled ? "ON" : "OFF");

  SerialBT.print("|TEMP=");
  SerialBT.print(temperature, 1);

  SerialBT.print("|HUM=");
  SerialBT.print(humidity, 1);

  SerialBT.print("|LIGHT=");
  SerialBT.print(lightValue);

  SerialBT.print("|MOTION=");
  SerialBT.print(motionDetected);

  SerialBT.print("|MARM=");
  SerialBT.print(motionArmed ? "ON" : "OFF");

  SerialBT.print("|TTH=");
  SerialBT.print(tempThreshold, 1);

  SerialBT.print("|LTH=");
  SerialBT.print(lightThreshold);

  SerialBT.print("|LED=");
  SerialBT.print(digitalRead(LEDPIN) ? "ON" : "OFF");

  SerialBT.print("|BUZZER=");
  SerialBT.print(digitalRead(BUZZERPIN) ? "ON" : "OFF");

  SerialBT.print("|ALERT=");
  SerialBT.print(alertState ? "ON" : "OFF");

  SerialBT.print("|AREASON=");
  SerialBT.print(alertReason);

  SerialBT.print("|EVENT=");
  SerialBT.println(lastEvent);
}

// ---------------- STREAMING ----------------
void sendStreamingData() {
  bool alertState = isAlertActive();
  String alertReason = getAlertReason();

  SerialBT.print("STREAM|TEMP=");
  SerialBT.print(temperature, 1);

  SerialBT.print("|HUM=");
  SerialBT.print(humidity, 1);

  SerialBT.print("|LIGHT=");
  SerialBT.print(lightValue);

  SerialBT.print("|MOTION=");
  SerialBT.print(motionDetected);

  SerialBT.print("|MARM=");
  SerialBT.print(motionArmed ? "ON" : "OFF");

  SerialBT.print("|MODE=");
  SerialBT.print(mode);

  SerialBT.print("|TTH=");
  SerialBT.print(tempThreshold, 1);

  SerialBT.print("|LTH=");
  SerialBT.print(lightThreshold);

  SerialBT.print("|ALERT=");
  SerialBT.print(alertState ? "ON" : "OFF");

  SerialBT.print("|AREASON=");
  SerialBT.print(alertReason);

  SerialBT.print("|EVENT=");
  SerialBT.println(lastEvent);
}

void handleStreaming() {
  if (streamingEnabled && millis() - lastStreamSend >= streamInterval) {
    lastStreamSend = millis();
    sendStreamingData();
  }
}

// ---------------- COMMAND HANDLER ----------------
void handleCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "TEMP?") {
    SerialBT.print("TEMP: ");
    SerialBT.print(temperature, 1);
    SerialBT.println(" C");
  }
  else if (cmd == "HUM?") {
    SerialBT.print("HUM: ");
    SerialBT.print(humidity, 1);
    SerialBT.println(" %");
  }
  else if (cmd == "LIGHT?") {
    SerialBT.print("LIGHT: ");
    SerialBT.println(lightValue);
  }
  else if (cmd == "STATUS?") {
    sendStatus();
  }
  else if (cmd == "STREAM ON") {
    streamingEnabled = true;
    lastStreamSend = 0;
    SerialBT.println("SUCCESS: STREAMING ENABLED");
  }
  else if (cmd == "STREAM OFF") {
    streamingEnabled = false;
    SerialBT.println("SUCCESS: STREAMING DISABLED");
  }
  else if (cmd == "MODE AUTO") {
    mode = "AUTO";
    SerialBT.println("SUCCESS: MODE SET TO AUTO");
  }
  else if (cmd == "MODE MANUAL") {
    mode = "MANUAL";
    digitalWrite(LEDPIN, LOW);
    digitalWrite(BUZZERPIN, LOW);
    SerialBT.println("SUCCESS: MODE SET TO MANUAL");
  }
  else if (cmd == "MOTION ARM") {
    motionArmed = true;
    SerialBT.println("SUCCESS: MOTION ARMED");
  }
  else if (cmd == "MOTION DISARM") {
    motionArmed = false;
    SerialBT.println("SUCCESS: MOTION DISARMED");
  }
  else if (cmd.startsWith("SET TEMP ")) {
    String valueStr = cmd.substring(9);
    float newTempThreshold = valueStr.toFloat();

    if (newTempThreshold >= 0.0 && newTempThreshold <= 100.0) {
      tempThreshold = newTempThreshold;
      SerialBT.print("SUCCESS: TEMP THRESHOLD SET TO ");
      SerialBT.println(tempThreshold, 1);
    } else {
      SerialBT.println("ERROR: INVALID TEMP THRESHOLD");
    }
  }
  else if (cmd.startsWith("SET LIGHT ")) {
    String valueStr = cmd.substring(10);
    int newLightThreshold = valueStr.toInt();

    if (newLightThreshold >= 0 && newLightThreshold <= 4095) {
      lightThreshold = newLightThreshold;
      SerialBT.print("SUCCESS: LIGHT THRESHOLD SET TO ");
      SerialBT.println(lightThreshold);
    } else {
      SerialBT.println("ERROR: INVALID LIGHT THRESHOLD");
    }
  }
  else if (cmd == "LED ON") {
    if (mode == "MANUAL") {
      digitalWrite(LEDPIN, HIGH);
      SerialBT.println("SUCCESS: LED TURNED ON");
    } else {
      SerialBT.println("ERROR: SWITCH TO MANUAL MODE FIRST");
    }
  }
  else if (cmd == "LED OFF") {
    if (mode == "MANUAL") {
      digitalWrite(LEDPIN, LOW);
      SerialBT.println("SUCCESS: LED TURNED OFF");
    } else {
      SerialBT.println("ERROR: SWITCH TO MANUAL MODE FIRST");
    }
  }
  else if (cmd == "BUZZER ON") {
    if (mode == "MANUAL") {
      digitalWrite(BUZZERPIN, HIGH);
      SerialBT.println("SUCCESS: BUZZER TURNED ON");
    } else {
      SerialBT.println("ERROR: SWITCH TO MANUAL MODE FIRST");
    }
  }
  else if (cmd == "BUZZER OFF") {
    if (mode == "MANUAL") {
      digitalWrite(BUZZERPIN, LOW);
      SerialBT.println("SUCCESS: BUZZER TURNED OFF");
    } else {
      SerialBT.println("ERROR: SWITCH TO MANUAL MODE FIRST");
    }
  }
  else {
    SerialBT.println("ERROR: INVALID COMMAND");
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(LEDPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(PIRPIN, INPUT);

  digitalWrite(LEDPIN, LOW);
  digitalWrite(BUZZERPIN, LOW);

  SerialBT.begin("ESP32_Control_System");

  Serial.println("Bluetooth started");
  Serial.println("Device name: ESP32_Control_System");
}

void loop() {
  readSensors();
  handleStreaming();

  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    Serial.print("Received: ");
    Serial.println(cmd);
    handleCommand(cmd);
  }
}