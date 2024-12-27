#ifndef INDEX_HTML_H
#define INDEX_HTML_H

// HTML-код в виде константной строки
const char index_html[] PROGMEM = R"rawliteral(
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
)rawliteral";

#endif
