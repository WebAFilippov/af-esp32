#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "GyverEncoder.h"

#include "index_html.h"
#include "saved_html.h"

// Пины
#define LED_PIN 2  // Встроенный светодиод(синий)

#define CLK 22  // Encoder left
#define DT 23   // Encoder right
#define SW 21   // Кнопка Енкодер

#define MANUAL 0
#define AUTO 1

// Состояния
bool buttonPressed = false;
bool serverMode = false;

// Объекты
WebServer server(80);
Preferences preferences;
WiFiClient espClient;
PubSubClient client(espClient);
Encoder encoder(CLK, DT, SW);

// Таймеры
unsigned long buttonPressStart = 0;
const unsigned long longPressDuration = 5000;  // 5 секунд

// MQTT
const int mqtt_port = 1883;
const char* mqtt_client_id = "mqtt-client";
const char* mqtt_topic_sub_mask = "response/#";

// Сохранённые данные Wi-Fi
String savedSSID;
String savedPassword;
String savedMqttServer;

void sendMqttCommand(const char* topic, const char* message) {
  if (client.connected()) {
    client.publish(topic, message);
  }
}


void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.print("Received MQTT message on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(messageTemp);
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String mqttServer = server.arg("mqtt_server");

  Serial.println("Saving Wi-Fi and MQTT settings:");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("MQTT Server: ");
  Serial.println(mqttServer);

  if (preferences.getString("ssid") != ssid) preferences.putString("ssid", ssid);
  if (preferences.getString("password") != password) preferences.putString("password", password);
  if (preferences.getString("mqtt_server") != mqttServer) preferences.putString("mqtt_server", mqttServer);

  server.send(200, "text/html", saved_html);
  delay(1000);
  ESP.restart();
}

bool isLongPress() {
  static bool buttonPressed = false;
  static unsigned long pressStart = 0;

  if (digitalRead(SW) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      pressStart = millis();
    } else if (millis() - pressStart >= longPressDuration) {
      return true;  // Длинное нажатие
    }
  } else {
    buttonPressed = false;
  }
  return false;
}

void handleButtonPress() {
  if (isLongPress()) {
    resetWiFiSettings();
  }
}

void connectToWiFi(const char* ssid, const char* password) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    // Проверка на превышение времени ожидания (10 секунд)
    if (millis() - startAttemptTime > 10000) {
      Serial.println("WiFi connection failed!");
      updateLED();
      return;
    }

    // Проверка долгого нажатия кнопки
    handleButtonPress();
    delay(100);  // Короткая задержка для экономии ресурсов
  }

  Serial.println("WiFi connected!");
}

void reconnectWiFi() {
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    unsigned long currentTime = millis();
    if (currentTime - startAttemptTime > 10000) {  // 10 секунд таймаут
      Serial.println("WiFi connection failed!");
      updateLED();
      return;
    }

    handleButtonPress();
    delay(500);
  }

  Serial.println("WiFi reconnected successfully.");
  updateLED();
}

void reconnectMQTT() {
  unsigned long startAttemptTime = millis();
  while (!client.connected()) {
    Serial.print("Connecting to MQTT... ");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected!");
      client.subscribe(mqtt_topic_sub_mask);
      updateLED();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again later...");

      unsigned long currentTime = millis();
      if (currentTime - startAttemptTime > 10000) {
        Serial.println("MQTT connection failed!");
        updateLED();
        return;
      }

      handleButtonPress();
      delay(500);
    }
  }
}

void resetWiFiSettings() {
  Serial.println("Resetting Wi-Fi settings...");
  preferences.clear();
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  pinMode(SW, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  encoder.setType(TYPE2);
  encoder.setTickMode(AUTO);

  preferences.begin("wifi-config", false);

  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  savedMqttServer = preferences.getString("mqtt_server", "");

  if (savedSSID != "" && savedPassword != "") {
    connectToWiFi(savedSSID.c_str(), savedPassword.c_str());
    if (savedMqttServer != "") { 
      client.setServer(savedMqttServer.c_str(), mqtt_port);
      client.setCallback(callback);
      Serial.println("MQTT server setup complete.");
    } else {
      Serial.println("MQTT server not configured.");
    }
  } else {
    serverMode = true;
    IPAddress local_IP(192, 168, 4, 100);
    IPAddress gateway(192, 168, 4, 100);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP("SMART_TABLE");
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();

    Serial.println("Access Point mode enabled.");
    updateLED();
  }
}


void updateLED() {
  static unsigned long lastBlinkTime = 0;
  static int blinkCount = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastBlinkTime >= 500) {
      lastBlinkTime = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      blinkCount++;
    }
    if (blinkCount >= 2) {
      blinkCount = 0;
    }
  } else if (!client.connected()) {
    if (millis() - lastBlinkTime >= 200) {
      lastBlinkTime = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      blinkCount++;
    }
    if (blinkCount >= 4) {
      blinkCount = 0;
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }
}

void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
  if (!client.connected()) {
    client.unsubscribe(mqtt_topic_sub_mask);
    reconnectMQTT();
  }
}

void loop() {
  if (serverMode) {
    server.handleClient();
    updateLED();
    handleButtonPress();
    return;
  }

  checkConnections();
  updateLED();
  client.loop();
  encoder.tick();
  if (encoder.isRight()) {
    sendMqttCommand("increment/volume", "+");
  }
  if (encoder.isLeft()) {
    sendMqttCommand("decrement/volume", "-");
  }
  if (encoder.isSingle()) {
    sendMqttCommand("toggle/volume", "toggle");
  }
  handleButtonPress();
}


// TP-Link_9F9A
// 11745709
// 192.168.0.199
