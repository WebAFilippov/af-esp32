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

#define MANUAL 0		// нужно вызывать функцию tick() вручную
#define AUTO 1			// tick() входит во все остальные функции и опрашивается сама!


// Состояния кнопки
bool buttonPressed = false;

// Объекты для работы с веб-сервером и памятью
WebServer server(80);
Preferences preferences;

// Таймеры для кнопки
unsigned long buttonPressStart = 0;
const unsigned long longPressDuration = 5000;  // 5 секунд

// Таймеры для переподключения
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;  // 10 секунд

// Настройки MQTT
const int mqtt_port = 1883;                  // Порт, который ты задал в .env (по умолчанию 1883)
const char* mqtt_client_id = "mqtt-client";  // ID клиента MQTT

// Топик с маской
const char* mqtt_topic_sub_mask = "response/#";

WiFiClient espClient;
PubSubClient client(espClient);
Encoder encoder(CLK, DT, SW);

// Сохранённые данные Wi-Fi
String savedSSID;
String savedPassword;
String savedMqttServer;

// Callback-функция для обработки входящих сообщений
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Сообщение получено на топике: ");
  Serial.print(topic);
  Serial.print(". Сообщение: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.println(messageTemp);
}

// Функция для мигания светодиодом
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

// Функция для подключения к Wi-Fi
void connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  IPAddress localIP = WiFi.localIP();
  Serial.print("Local IP: ");
  Serial.println(localIP);
}

// Обработчик для страницы ввода Wi-Fi и MQTT
void handleRoot() {
  server.send(200, "text/html",  index_html);
}

// Обработчик для сохранения настроек
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

// Обработчик сброса настроек Wi-Fi
void resetWiFiSettings() {
  preferences.clear();
  Serial.println("WiFi settings cleared");
  blinkLED(5, 300);
  ESP.restart();
}

// Обработка нажатия кнопки
void handleButtonPress() {
  if (digitalRead(SW) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStart = millis();
    } else if (millis() - buttonPressStart >= longPressDuration) {
      resetWiFiSettings();
    }
  } else {
    buttonPressed = false;
  }
}

// Проверка состояния Wi-Fi и переподключение
void reconnectWiFi() {
  if (WiFi.getMode() != WIFI_STA || WiFi.status() == WL_CONNECTED) {
    // Устройство не в режиме станции — переподключение не требуется
    return;
  }

  if (millis() - lastReconnectAttempt >= reconnectInterval) {
    lastReconnectAttempt = millis();
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(SW, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  encoder.setType(TYPE2);  // Encoder ON
  encoder.setTickMode(AUTO); // MANUAL / AUTO - ручной или автоматический опрос энкодера функцией tick().

  // Открываем доступ к NVS
  preferences.begin("wifi-config", false);

  // Получение сохранённых данных
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  savedMqttServer = preferences.getString("mqtt_server", "");

  if (savedSSID != "" && savedPassword != "") {
    connectToWiFi(savedSSID.c_str(), savedPassword.c_str());
    if (savedMqttServer != "") {
      client.setServer(savedMqttServer.c_str(), mqtt_port);
      client.setCallback(callback);
      Serial.print("Saved MQTT Server: ");
      Serial.println(savedMqttServer);
    }
  } else {
    Serial.println("No saved WiFi credentials. Starting Access Point...");

    // Настройка нового IP-адреса для точки доступа
    IPAddress local_IP(192, 168, 4, 100);  // Стандартный диапазон ESP32
    IPAddress gateway(192, 168, 4, 100);   // Шлюз совпадает с IP
    IPAddress subnet(255, 255, 255, 0);    // Маска подсети

    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
      Serial.println("Failed to configure Access Point IP");
    }

    WiFi.softAP("SMART_TABLE");

    IPAddress IP = WiFi.softAPIP();
    Serial.print("Access Point IP: ");
    Serial.println(IP);

    // Настраиваем сервер
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();
  }
}

void loop() {
  server.handleClient();
  handleButtonPress();
  reconnectWiFi();

  // Управление светодиодом в зависимости от состояния Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);  // Включить светодиод
  } else {
    digitalWrite(LED_PIN, LOW);  // Выключить светодиод
  }

  client.loop();  // Обрабатываем входящие сообщения и отправляем пинг-сообщения  

  if (encoder.isRight() && client.connected()) client.publish("increment/volume", "+");    	
  if (encoder.isLeft() && client.connected()) client.publish("decrement/volume", "-");
  if (encoder.isSingle() && client.connected()) client.publish("toggle/volume", "toggle");
}