# 📸 ESP32-CAM Telegram Surveillance System

![ESP32-CAM](https://img.shields.io/badge/ESP32--CAM-AI%20Thinker-blueviolet)
![Telegram Bot](https://img.shields.io/badge/Telegram-Bot-blue)
![Web Interface](https://img.shields.io/badge/Web-Interface-green)

A feature-rich surveillance system using ESP32-CAM with Telegram integration and web interface control. Capture photos, stream video, and receive images directly to your Telegram account! 🤖📱

## ✨ Features

- 📱 **Telegram Bot Control** - Send commands and receive photos
- 🌐 **Web Interface** - Modern responsive control panel
- 💾 **SD Card Storage** - Save photos with timestamp
- ⚡ **Real-time Streaming** - Live video feed
- 🔦 **Flash Control** - Onboard LED control
- 🤖 **Automation** - Scheduled photo capture
- 🎛️ **Camera Settings** - Adjust brightness, contrast, and saturation
- 📤 **Easy Sharing** - Instant Telegram photo sharing

## 🛠️ Hardware Requirements

| Component | Quantity |
|-----------|----------|
| ESP32-CAM (AI Thinker) | 1 |
| MicroSD Card (4GB+) | 1 |
| FTDI Programmer | 1 |
| Push Button | 1 |
| Jumper Wires | Several |

## 🔌 Wiring Diagram

| ESP32-CAM | Connection |
|-----------|-----------|
| GND | Button → GND |
| GPIO 12 | Button → Signal |
| 5V | FTDI 5V |
| GND | FTDI GND |
| U0R | FTDI RX |
| U0T | FTDI TX |

## 🚀 Installation & Setup

### 1. Arduino IDE Setup
1. Install ESP32 board support in Arduino IDE
2. Add these libraries:
   - UniversalTelegramBot
   - ArduinoJson
   - WiFi
   - WebServer
   - ESP32-CAM

### 2. Telegram Bot Creation
1. Message @BotFather on Telegram
2. Create a new bot with `/newbot`
3. Copy the API token
4. Find your Chat ID with @userinfobot

### 3. Code Configuration
Update these values in the code:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#define BOTtoken "YOUR_TELEGRAM_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"
```

### 4. Uploading
1. Connect FTDI programmer to ESP32-CAM
2. Select "AI Thinker ESP32-CAM" board
3. Upload the code

## 📋 Usage

### Telegram Commands
| Command | Description |
|---------|-------------|
| `/start` | Show welcome message |
| `/photo` | Capture and send photo |
| `/flash` | Toggle flash LED |
| `/autopic` | Enable auto capture (10s) |
| `/noautopic` | Disable auto capture |
| `/status` | Show device status |
| `/brightness X` | Set brightness (-2 to 2) |
| `/contrast X` | Set contrast (-2 to 2) |
| `/saturation X` | Set saturation (-2 to 2) |

### Web Interface
Access the web interface at `http://[ESP32-IP]`:
- View live stream
- Capture and save photos
- Control flash LED
- Adjust camera settings
- View system status

### Physical Button
- Press the button connected to GPIO 12 to instantly send a photo to Telegram

## 🖥️ Web Interface Preview

The web interface features a modern dark theme with:
- Live video streaming
- Control buttons for all functions
- Camera settings sliders
- System status display
- Responsive design for mobile devices

## ⚙️ Technical Details

- **Video Resolution**: 800×600 (SVGA) with PSRAM, 640×480 (VGA) without
- **Image Format**: JPEG
- **Web Server**: Async HTTP server on port 80
- **Security**: Telegram chat ID verification
- **Storage**: SD card with date-based organization

## 🔧 Troubleshooting

| Issue | Solution |
|-------|----------|
| Camera init failed | Check camera module connection |
| SD card not detected | Format SD card as FAT32 |
| Can't connect to WiFi | Verify SSID/password |
| Telegram send fails | Check bot token and chat ID |
| Upside down image | Already fixed in code |

## 📝 License

This project is open source and available under the MIT License.

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check issues page.

## 📞 Support

If you have any questions, please open an issue or contact via Telegram.

---

**⭐ Star this repo if you found it helpful!**

---

*Built with ❤️ using ESP32-CAM and Arduino*
