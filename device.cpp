#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "GyverEncoder.h"

#include "index_html.h"
#include "saved_html.h"

// Пины
#define LED_PIN 2
#define CLK 22
#define DT 23
#define SW 21

#define MANUAL 0
#define AUTO 1

// Переменные состояния
bool serverMode = false;

// Объекты
WebServer server(80);
Preferences preferences;
WiFiClient espClient;
PubSubClient client(espClient);
Encoder encoder(CLK, DT, SW);

// MQTT
const int mqtt_port = 1883;
const char* mqtt_client_id = "mqtt-client";
const char* mqtt_topic_sub_mask = "#";

// Сохранённые данные Wi-Fi
String savedSSID;
String savedPassword;
String savedMqttServer;

// Статические IP-адреса для AP Mode
IPAddress local_IP(192, 168, 4, 100);
IPAddress gateway(192, 168, 4, 100);
IPAddress subnet(255, 255, 255, 0);

// Функция отправки MQTT-команды
void sendMqttCommand(const char* topic, const char* message) {
  if (client.connected()) {
    client.publish(topic, message);
  }
}

// MQTT коллбэк
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

// Обработчики сервера
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String mqttServer = server.arg("mqtt_server");

  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("mqtt_server", mqttServer);

  server.send(200, "text/html", saved_html);
  delay(1000);
  ESP.restart();
}

// Задачи FreeRTOS
void TaskWiFiReconnect(void* pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 5000 / portTICK_PERIOD_MS;  // Проверка раз в 5 секунд

  while (1) {
    if (WiFi.status() != WL_CONNECTED && !serverMode) {
      Serial.println("Reconnecting WiFi...");
      WiFi.reconnect();  // Используем reconnect вместо begin для ускорения
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void TaskMQTTReconnect(void* pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 500 / portTICK_PERIOD_MS;

  while (1) {
    if (!client.connected()) {
      Serial.print("Connecting to MQTT... ");
      if (client.connect(mqtt_client_id)) {
        Serial.println("connected!");
        client.subscribe(mqtt_topic_sub_mask);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again later...");
      }
    }

    client.loop();  // Обязательный вызов для поддержки MQTT
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void TaskEncoder(void* pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 1 / portTICK_PERIOD_MS;  // Проверка каждые 1 мс

  while (1) {
    encoder.tick();

    if (encoder.isRight()) {
      Serial.println("send +");
      sendMqttCommand("increment/volume", "+");
    }
    if (encoder.isLeft()) {
      Serial.println("send -");
      sendMqttCommand("decrement/volume", "-");
    }
    if (encoder.isSingle()) {
      Serial.println("send toggle");
      sendMqttCommand("toggle/volume", "toggle");
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);  // Жёсткая привязка к интервалу
  }
}

void TaskWebServer(void* pvParameters) {
  while (1) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void TaskLED(void* pvParameters) {
  while (1) {
    if (serverMode) {  // Режим сервера
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Мигание 1 раз в 3 секунды
      vTaskDelay(3000 / portTICK_PERIOD_MS);
    } 
    else if (WiFi.status() != WL_CONNECTED) {  // Нет подключения к Wi-Fi
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Мигание 1 раз в секунду
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
    else if (!client.connected()) {  // Нет подключения к MQTT
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Мигание 1 раз в 500 мс
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } 
    else {  // Подключен к MQTT
      digitalWrite(LED_PIN, HIGH); // Постоянный свет
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

void TaskButton(void* pvParameters) {
  while (1) {
    if (digitalRead(SW) == LOW) {
      unsigned long pressStart = millis();
      while (digitalRead(SW) == LOW) {
        if (millis() - pressStart > 5000) {
          preferences.clear();
          WiFi.softAPConfig(local_IP, gateway, subnet);
          WiFi.softAP("SMART_TABLE");
          server.on("/", handleRoot);
          server.on("/save", handleSave);
          server.begin();
          serverMode = true;
          break;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);  // Увеличен интервал для кнопки
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SW, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  preferences.begin("wifi-config", false);

  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  savedMqttServer = preferences.getString("mqtt_server", "");

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  client.setServer(savedMqttServer.c_str(), mqtt_port);
  client.setCallback(callback);

  encoder.setType(TYPE2);
  encoder.setTickMode(AUTO);

  if (savedSSID == "" || savedPassword == "") {
    serverMode = true;
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP("SMART_TABLE");
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();
  }

  xTaskCreate(TaskWiFiReconnect, "WiFi Reconnect", 2048, NULL, 1, NULL);  // Низкий приоритет
  xTaskCreate(TaskMQTTReconnect, "MQTT Reconnect", 2048, NULL, 2, NULL);  // Средний приоритет
  xTaskCreate(TaskEncoder, "Encoder", 2048, NULL, 3, NULL);               // Высокий приоритет
  xTaskCreate(TaskWebServer, "Web Server", 2048, NULL, 1, NULL);          // Низкий приоритет
  xTaskCreate(TaskLED, "LED Control", 2048, NULL, 0, NULL);               // Самый низкий приоритет
  xTaskCreate(TaskButton, "Button Control", 2048, NULL, 2, NULL);         // Средний приоритет
}

void loop() {
  vTaskDelete(NULL);
}



// TP-Link_9F9A
// 11745709
// 192.168.0.199
