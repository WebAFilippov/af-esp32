#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "GyverEncoder.h"

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
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi & MQTT Setup</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f3f4f6;
      color: #333;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .container {
      background: #fff;
      border-radius: 10px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
      padding: 20px;
      max-width: 400px;
      width: 100%;
    }
    h1 {
      font-size: 1.5rem;
      margin-bottom: 1rem;
      text-align: center;
      color: #1e90ff;
    }
    label {
      font-weight: bold;
      display: block;
      margin: 10px 0 5px;
    }
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 10px;
      margin-bottom: 15px;
      border: 1px solid #ccc;
      border-radius: 5px;
      box-sizing: border-box;
    }
    input[type="submit"] {
      background: #1e90ff;
      color: #fff;
      padding: 10px 15px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      width: 100%;
      font-size: 1rem;
    }
    input[type="submit"]:hover {
      background: #0077cc;
    }
    @media (max-width: 480px) {
      .container {
        padding: 15px;
      }
      h1 {
        font-size: 1.2rem;
      }
      input[type="submit"] {
        font-size: 0.9rem;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>WiFi & MQTT Setup</h1>
    <form action="/save" method="POST">
      <label for="ssid">SSID:</label>
      <input type="text" id="ssid" name="ssid" required>

      <label for="password">Password:</label>
      <input type="password" id="password" name="password" required>

      <label for="mqtt_server">MQTT Server:</label>
      <input type="text" id="mqtt_server" name="mqtt_server">

      <input type="submit" value="Save">
    </form>
  </div>
</body>
</html>
  )rawliteral");
}

// Обработчик для сохранения настроек
void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  String mqttServer = server.arg("mqtt_server");

  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("mqtt_server", mqttServer);

  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Settings Saved</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f3f4f6;
      color: #333;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .message-box {
      background: #fff;
      border-radius: 10px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
      padding: 20px;
      text-align: center;
      max-width: 400px;
      width: 100%;
    }
    h1 {
      font-size: 1.5rem;
      margin-bottom: 1rem;
      color: #1e90ff;
    }
    p {
      font-size: 1rem;
      margin-bottom: 1rem;
    }
    .loader {
      border: 5px solid #f3f3f3;
      border-top: 5px solid #1e90ff;
      border-radius: 50%;
      width: 50px;
      height: 50px;
      animation: spin 1s linear infinite;
      margin: 0 auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
  </style>
</head>
<body>
  <div class="message-box">
    <h1>Settings Saved</h1>
    <p>The device is rebooting. Please wait...</p>
    <div class="loader"></div>
  </div>
</body>
</html>
  )rawliteral");

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