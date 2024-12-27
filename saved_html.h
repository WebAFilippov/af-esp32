#ifndef SAVED_HTML_H
#define SAVED_HTML_H

const char saved_html[] PROGMEM = R"rawliteral(
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
  )rawliteral";

#endif
