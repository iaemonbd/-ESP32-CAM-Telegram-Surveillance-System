#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

// ------------ WiFi ------------
const char* ssid     = "your ssid";
const char* password = "your password";

// ------------ Telegram ------------
#define BOTtoken "your bot token"
#define CHAT_ID  "CHAT ID "

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// ------------ Pins ------------
#define FLASH_LED_PIN 4          // ESP32-CAM flash LED (GPIO4 controls the on-board LED)
#define TELEGRAM_BTN  12         // Button to GND -> send Telegram photo (⚠️ avoid holding low at boot on GPIO12)

// ------------ States ------------
bool flashState = false;
bool autoPic = false;
unsigned long lastAutoPic = 0;
const uint32_t autoPicInterval = 10000; // 10s
String lastPhotoPath = "None";

// ------------ Camera Settings ------------
int cameraBrightness = 0;    // -2 to 2
int cameraContrast = 0;      // -2 to 2
int cameraSaturation = 0;    // -2 to 2

// ------------ Web Server ------------
WebServer server(80);

// ------------ Bot Polling ------------
int botRequestDelay = 1000;
unsigned long lastTimeBotRan = 0;

// ------------ ESP32-CAM Pins (AI Thinker) ------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ------------ HTML (modern, no emojis) ------------
const char htmlPage[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-CAM</title>
  <style>
    body {
      font-family: Segoe UI, Roboto, Arial;
      background: #071428;
      color: #e6eef6;
      margin: 0; padding: 10px;
    }
    .card {
      max-width: 920px; margin: 0 auto;
      background: rgba(255,255,255,0.03);
      border-radius: 12px; padding: 14px;
    }
    header { display:flex; justify-content:space-between; align-items:center; margin-bottom:15px; }
    h1 { font-size: 18px; margin: 0; }
    .controls { display:flex; flex-wrap:wrap; gap:8px; margin:15px 0; }
    button {
      background:#06b6d4; border:0; padding:8px 12px; border-radius:8px;
      color:#021428; font-weight:700; cursor:pointer; transition:.2s;
    }
    button:hover { background:#0891b2; transform:translateY(-1px); }
    button.ghost { background:transparent; border:1px solid rgba(255,255,255,.06); color:#e6eef6; }
    button.ghost:hover { background:rgba(255,255,255,.08); }
    img#stream { width:100%; max-height:480px; object-fit:cover; border-radius:8px; border:1px solid rgba(255,255,255,.1); }
    pre.status {
      background: rgba(0,0,0,.2); padding:12px; border-radius:8px;
      color:#cfe9f2; white-space:pre-wrap; font-size:14px; line-height:1.4;
      overflow-x:auto; max-height:300px; margin-top:10px;
    }
    .status-container { margin-top:20px; border-top:1px solid rgba(255,255,255,.05); padding-top:15px; }
    .slider-container { margin: 10px 0; }
    .slider-container label { display: block; margin-bottom: 5px; }
    .slider-container input { width: 100%; }
    @media (max-width: 600px) {
      .controls { flex-direction:column; }
      button { width:100%; }
      header { flex-direction:column; align-items:flex-start; gap:6px; }
    }
  </style>
</head>
<body>
  <div class="card">
    <header>
      <h1>ESP32-CAM</h1>
      <div>IP: [DEVICE_IP]</div>
    </header>
    <div class="stream-container">
      <img id="stream" src="/stream" alt="Live Stream">
    </div>
    <div class="controls">
      <button onclick="sendCommand('telegramphoto')">Send Photo (Telegram)</button>
      <button onclick="sendCommand('save')">Save Photo (SD)</button>
      <button onclick="sendCommand('toggleflash')">Toggle Flash</button>
      <button id="autopicBtn" class="ghost" onclick="toggleAutoPic()">Autopic</button>
      <button onclick="updateStatus()">Status</button>
    </div>
    
    <div class="slider-container">
      <label for="brightness">Brightness: <span id="brightnessValue">0</span></label>
      <input type="range" id="brightness" min="-2" max="2" value="0" onchange="updateCameraSetting('brightness', this.value)">
    </div>
    <div class="slider-container">
      <label for="contrast">Contrast: <span id="contrastValue">0</span></label>
      <input type="range" id="contrast" min="-2" max="2" value="0" onchange="updateCameraSetting('contrast', this.value)">
    </div>
    <div class="slider-container">
      <label for="saturation">Saturation: <span id="saturationValue">0</span></label>
      <input type="range" id="saturation" min="-2" max="2" value="0" onchange="updateCameraSetting('saturation', this.value)">
    </div>
    
    <div class="status-container">
      <pre class="status" id="statusBox">Connecting to device...</pre>
    </div>
  </div>
  <script>
    async function sendCommand(endpoint) {
      try {
        const response = await fetch(`/${endpoint}`);
        const result = await response.text();
        alert(result);
        updateStatus();
      } catch (e) { alert('Error executing command'); }
    }
    
    async function updateCameraSetting(setting, value) {
      try {
        document.getElementById(setting + 'Value').innerText = value;
        await fetch(`/set_${setting}?value=${value}`);
      } catch (e) { console.error('Error updating setting'); }
    }
    
    async function updateStatus() {
      try {
        const response = await fetch('/status');
        const statusText = await response.text();
        document.getElementById('statusBox').innerText = statusText;
      } catch (e) {
        document.getElementById('statusBox').innerText = 'Error fetching status';
      }
    }
    
    async function toggleAutoPic() {
      const btn = document.getElementById('autopicBtn');
      if (btn.dataset.on === '1') {
        await sendCommand('noautopic');
        btn.dataset.on = '0';
        btn.innerText = 'Autopic';
      } else {
        await sendCommand('autopic');
        btn.dataset.on = '1';
        btn.innerText = 'Stop Autopic';
      }
    }
    
    document.addEventListener('DOMContentLoaded', () => { 
      updateStatus(); 
      setInterval(updateStatus, 8000); 
    });
  </script>
</body>
</html>
)rawliteral";

// ------------ Camera Init ------------
void configInitCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_SVGA; // 800x600
    config.jpeg_quality = 10;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    delay(1000);
    ESP.restart();
  }

  // Fix upside down issue
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);  // Flip vertically
  s->set_hmirror(s, 1); // Flip horizontally
  
  // Set initial camera settings
  s->set_brightness(s, cameraBrightness);
  s->set_contrast(s, cameraContrast);
  s->set_saturation(s, cameraSaturation);
}

// ------------ Update Camera Settings ------------
void updateCameraSettings() {
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, cameraBrightness);
  s->set_contrast(s, cameraContrast);
  s->set_saturation(s, cameraSaturation);
}

// ------------ SD helpers ------------
bool ensureSD() {
  if (SD_MMC.cardType() != CARD_NONE) return true;
  // mount in 1-bit mode for ESP32-CAM slot
  bool ok = SD_MMC.begin("/sdcard", true);
  if (!ok) Serial.println("SD Card mount failed");
  return ok;
}

// ------------ Save to SD ------------
String savePhotoToSD() {
  if (!ensureSD()) return "SD not mounted";

  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return "Camera capture failed";

  String dir = "/";
  if (hasTime) {
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    dir = "/" + String(dateStr);
    if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
  }

  String filename;
  if (hasTime) {
    char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H-%M-%S", &timeinfo);
    filename = String(timeStr) + ".jpg";
  } else {
    filename = "photo.jpg";
  }

  String path = dir + "/" + filename;
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    return "File open failed";
  }
  size_t written = file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  if (written != fb->len) return "Write incomplete";
  lastPhotoPath = path;
  Serial.println("Saved: " + path);
  return "Saved: " + path;
}

// ------------ Telegram: manual HTTPS multipart ------------
String sendPhotoTelegram() {
  // Flash LED indication
  for (int i = 0; i < 3; i++) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(100);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(100);
  }
  
  digitalWrite(FLASH_LED_PIN, HIGH); // Keep flash on while capturing
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(FLASH_LED_PIN, LOW); // Turn flash off after capture
  
  if (!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }

  clientTCP.setTimeout(12000);
  if (clientTCP.connected()) clientTCP.stop();
  if (!clientTCP.connect("api.telegram.org", 443)) {
    esp_camera_fb_return(fb);
    return "TLS connect failed";
  }

  const String boundary = "----ESP32CAMFormBoundary";
  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" CHAT_ID "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t totalLen = head.length() + fb->len + tail.length();

  clientTCP.printf("POST /bot%s/sendPhoto HTTP/1.1\r\n", BOTtoken);
  clientTCP.println("Host: api.telegram.org");
  clientTCP.println("Connection: close");
  clientTCP.println("Content-Type: multipart/form-data; boundary=" + boundary);
  clientTCP.println("Content-Length: " + String(totalLen));
  clientTCP.println();
  clientTCP.print(head);

  const size_t CHUNK = 1024;
  for (size_t i = 0; i < fb->len; i += CHUNK) {
    size_t n = (i + CHUNK < fb->len) ? CHUNK : (fb->len - i);
    clientTCP.write(fb->buf + i, n);
  }
  clientTCP.print(tail);

  esp_camera_fb_return(fb);

  String resp;
  unsigned long t0 = millis();
  while (clientTCP.connected() || clientTCP.available()) {
    if (clientTCP.available()) resp += (char)clientTCP.read();
    if (millis() - t0 > 12000) break;
  }
  clientTCP.stop();

  if (resp.indexOf("\"ok\":true") != -1) return "Photo sent";
  Serial.println(resp);
  return "Telegram error:\n" + resp;
}

// ------------ Telegram Commands ------------
void sendPlainKeyboard(const String& chat_id) {
  // Reply keyboard with plain text (no emojis), sends literal command texts
  String kb = "[[\"/photo\",\"/flash\"],[\"/autopic\",\"/noautopic\"],[\"/status\"]]";
  bot.sendMessageWithReplyKeyboard(chat_id, 
    "Commands:\n"
    "/photo - Send photo to Telegram\n"
    "/flash - Toggle flash LED\n"
    "/autopic - Save to SD every 10s\n"
    "/noautopic - Stop autopic\n"
    "/status - Device status\n"
    "\nLive: http://" + WiFi.localIP().toString(),
    "", kb, true);
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text    = bot.messages[i].text;
    String from    = bot.messages[i].from_name;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized", "");
      continue;
    }

    if (text == "/start") {
      sendPlainKeyboard(chat_id);
    }
    else if (text == "/photo") {
      bot.sendMessage(chat_id, "Capturing and sending photo...", "");
      String r = sendPhotoTelegram();
      bot.sendMessage(chat_id, r, "");
    }
    else if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
      bot.sendMessage(chat_id, flashState ? "Flash ON" : "Flash OFF", "");
    }
    else if (text == "/autopic") {
      if (!ensureSD()) {
        bot.sendMessage(chat_id, "SD not mounted. Check card.", "");
      } else {
        autoPic = true;
        lastAutoPic = millis();
        bot.sendMessage(chat_id, "Autopic enabled (every 10s to SD)", "");
      }
    }
    else if (text == "/noautopic") {
      autoPic = false;
      bot.sendMessage(chat_id, "Autopic disabled", "");
    }
    else if (text == "/status") {
      String msg = "IP: " + WiFi.localIP().toString() + "\n";
      msg += "Flash: " + String(flashState ? "On" : "Off") + "\n";
      if (ensureSD()) {
        msg += "SD: " + String(SD_MMC.totalBytes()/(1024*1024)) + "MB total, "
                     + String(SD_MMC.usedBytes()/(1024*1024)) + "MB used\n";
      } else {
        msg += "SD: Not mounted\n";
      }
      msg += "Autopic: " + String(autoPic ? "Enabled" : "Disabled") + "\n";
      msg += "Last photo: " + lastPhotoPath + "\n";
      msg += "Brightness: " + String(cameraBrightness) + "\n";
      msg += "Contrast: " + String(cameraContrast) + "\n";
      msg += "Saturation: " + String(cameraSaturation);
      bot.sendMessage(chat_id, msg, "");
    }
    // Handle camera settings commands
    else if (text.startsWith("/brightness")) {
      int value = text.substring(12).toInt();
      if (value >= -2 && value <= 2) {
        cameraBrightness = value;
        updateCameraSettings();
        bot.sendMessage(chat_id, "Brightness set to: " + String(value), "");
      } else {
        bot.sendMessage(chat_id, "Invalid brightness value. Use -2 to 2.", "");
      }
    }
    else if (text.startsWith("/contrast")) {
      int value = text.substring(10).toInt();
      if (value >= -2 && value <= 2) {
        cameraContrast = value;
        updateCameraSettings();
        bot.sendMessage(chat_id, "Contrast set to: " + String(value), "");
      } else {
        bot.sendMessage(chat_id, "Invalid contrast value. Use -2 to 2.", "");
      }
    }
    else if (text.startsWith("/saturation")) {
      int value = text.substring(12).toInt();
      if (value >= -2 && value <= 2) {
        cameraSaturation = value;
        updateCameraSettings();
        bot.sendMessage(chat_id, "Saturation set to: " + String(value), "");
      } else {
        bot.sendMessage(chat_id, "Invalid saturation value. Use -2 to 2.", "");
      }
    }
  }
}

// ------------ Web Handlers ------------
void handleRoot() {
  String html = FPSTR(htmlPage);
  html.replace("[DEVICE_IP]", WiFi.localIP().toString());
  server.send(200, "text/html", html);
}

void handleStream() {
  WiFiClient client = server.client();
  String head =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-cache\r\n\r\n";
  client.print(head);

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { delay(100); continue; }

    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);
    delay(50);
  }
}

void handleTelegramPhoto() {
  String res = sendPhotoTelegram();
  server.send(200, "text/plain", res);
}

void handleSave() {
  String res = savePhotoToSD();
  server.send(200, "text/plain", res);
}

void handleToggleFlash() {
  flashState = !flashState;
  digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW);
  server.send(200, "text/plain", flashState ? "Flash ON" : "Flash OFF");
}

void handleStatus() {
  String msg = "IP: " + WiFi.localIP().toString() + "\n";
  msg += "Flash: " + String(flashState ? "On" : "Off") + "\n";
  if (ensureSD()) {
    msg += "SD: " + String(SD_MMC.totalBytes()/(1024*1024)) + "MB total, "
               + String(SD_MMC.usedBytes()/(1024*1024)) + "MB used\n";
  } else {
    msg += "SD: Not mounted\n";
  }
  msg += "Autopic: " + String(autoPic ? "Enabled" : "Disabled") + "\n";
  msg += "Last photo: " + lastPhotoPath + "\n";
  msg += "Brightness: " + String(cameraBrightness) + "\n";
  msg += "Contrast: " + String(cameraContrast) + "\n";
  msg += "Saturation: " + String(cameraSaturation);
  server.send(200, "text/plain", msg);
}

void handleSetBrightness() {
  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();
    if (value >= -2 && value <= 2) {
      cameraBrightness = value;
      updateCameraSettings();
      server.send(200, "text/plain", "Brightness set to: " + String(value));
    } else {
      server.send(400, "text/plain", "Invalid value. Use -2 to 2.");
    }
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

void handleSetContrast() {
  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();
    if (value >= -2 && value <= 2) {
      cameraContrast = value;
      updateCameraSettings();
      server.send(200, "text/plain", "Contrast set to: " + String(value));
    } else {
      server.send(400, "text/plain", "Invalid value. Use -2 to 2.");
    }
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

void handleSetSaturation() {
  if (server.hasArg("value")) {
    int value = server.arg("value").toInt();
    if (value >= -2 && value <= 2) {
      cameraSaturation = value;
      updateCameraSettings();
      server.send(200, "text/plain", "Saturation set to: " + String(value));
    } else {
      server.send(400, "text/plain", "Invalid value. Use -2 to 2.");
    }
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

// ------------ Camera Server Start ------------
void startCameraServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/telegramphoto", HTTP_GET, handleTelegramPhoto);
  server.on("/save", HTTP_GET, handleSave);
  server.on("/toggleflash", HTTP_GET, handleToggleFlash);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/autopic", HTTP_GET, [](){ autoPic = true;  server.send(200, "text/plain", "Autopic enabled"); });
  server.on("/noautopic", HTTP_GET, [](){ autoPic = false; server.send(200, "text/plain", "Autopic disabled"); });
  server.on("/set_brightness", HTTP_GET, handleSetBrightness);
  server.on("/set_contrast", HTTP_GET, handleSetContrast);
  server.on("/set_saturation", HTTP_GET, handleSetSaturation);
  server.begin();
}

// ------------ Setup ------------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(TELEGRAM_BTN, INPUT_PULLUP);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

  // TLS (no root cert for simplicity)
  clientTCP.setInsecure();

  // Time (GMT+6 Bangladesh)
  configTime(6 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Camera
  configInitCamera();

  // SD Card (mount if present; functions will retry if not)
  ensureSD();

  startCameraServer();
  Serial.println("Web server started");
}

// ------------ Loop ------------
void loop() {
  server.handleClient();

  // Telegram polling
  if (millis() - lastTimeBotRan > botRequestDelay) {
    int newMsgs = bot.getUpdates(bot.last_message_received + 1);
    while (newMsgs) {
      handleNewMessages(newMsgs);
      newMsgs = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // AutoPic -> save to SD
  if (autoPic && (millis() - lastAutoPic > autoPicInterval)) {
    savePhotoToSD();
    lastAutoPic = millis();
  }

  // Physical button -> send to Telegram with visual feedback
  if (digitalRead(TELEGRAM_BTN) == LOW) {
    delay(40); // debounce
    if (digitalRead(TELEGRAM_BTN) == LOW) {
      // Flash LED to indicate button press
      digitalWrite(FLASH_LED_PIN, HIGH);
      delay(100);
      digitalWrite(FLASH_LED_PIN, LOW);
      
      sendPhotoTelegram();
      
      // Wait for button release
      while (digitalRead(TELEGRAM_BTN) == LOW) delay(10);
    }
  }
}