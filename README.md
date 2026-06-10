# 🎵 SARVS Hi-Fi DAP (Version 1)

An audiophile-grade, ESP32-based Digital Audio Player (DAP) featuring Hardware DSP, lossless SD playback, Bluetooth A2DP, Internet Radio, FM Tuner, and a stunning Glassmorphism Web UI.

[![ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/)
[![C++](https://img.shields.io/badge/Language-C++-green.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

---

## 🚀 What's New in Version 1?
Earlier was a great proof-of-concept, but Version 1 is a complete rewrite aimed at true high-fidelity audio and premium user experience:

* **Hardware DSP Volume Control:** Removed digital software attenuation. Volume is now controlled in the analog domain via the **BD37033FV DSP chip**, ensuring 0% loss in bit-depth, even at low volumes.
* **Lossless Audio Support:** Fully optimized `ESP32-audioI2S` buffer management to support high-bitrate **FLAC** and **WAV** files directly from the SD card.
* **Responsive Web UI:** A complete Tailwind CSS + Vanilla JS Web Interface stored entirely in PROGMEM. Control EQ, switch inputs, scan for WiFi, and manage radio presets from your phone.
* **Smart Resource Management:** WiFi is automatically disabled when switching to Bluetooth mode to guarantee 100% stable, stutter-free A2DP streaming.
* **Pop/Click Elimination:** Implemented asynchronous I2S buffer shielding and hardware hard-muting during track transitions to ensure dead-silent background noise when loading files.
* **Overhauled OLED UI:** Redesigned 128x64 display using custom Lopaka pixel-perfect icons and `FreeMono` typography.

---

## ✨ Key Features
* 💾 **SD Card Player:** Reads MP3, FLAC, and WAV. Saves last played track and folder.
* 🔵 **Bluetooth Receiver:** Stream audio from your phone directly to the DAC.
* 🌐 **Internet Radio:** Stream custom URLs directly from the web.
* 📻 **FM Radio:** Hardware RDA5807 module integration with saveable presets.
* 🔌 **AUX In:** Hardware analog bypass.
* 🎛️ **Hardware EQ:** Bass, Mid, Treble (-15dB to +15dB), Bass Q-Factor, and dedicated Subwoofer Output with adjustable Low-Pass Filter (55Hz - 160Hz).

---

## 🛠 Hardware Requirements
* **Microcontroller:** ESP32 (WROOM or WROVER)
* **DAC:** PCM5102A (I2S 32-bit DAC)
* **DSP / Pre-Amp:** BD37033FV (I2C Hardware Audio Processor)
* **FM Module:** RDA5807 (I2C)
* **Display:** 0.96" OLED SSD1306 (128x64 I2C)
* **Storage:** MicroSD Card Module (SPI)
* **Input:** standard 5-pin Rotary Encoder (KY-040) + 2 Push Buttons

### Pin Configuration
| Component | ESP32 Pin | Function |
| :--- | :--- | :--- |
| **I2S DAC (PCM5102)** | 26 | BCLK |
| | 25 | LRC / WSEL |
| | 27 | DOUT |
| **I2C Bus (OLED, DSP, FM)**| 21 | SDA |
| | 22 | SCL |
| **SPI SD Card** | 23 | MOSI |
| | 19 | MISO |
| | 18 | SCK |
| | 5 | CS |
| **Rotary Encoder** | 32 | CLK |
| | 33 | DT |
| | 4 | SW (Button) |
| **Hardware Buttons** | 14 | Mute |
| | 35 | Power / Standby |

---

## 💻 Software & Installation

### Required Arduino Libraries
You must install the following libraries via the Library Manager or GitHub to compile this code:
1. `ESP32-audioI2S` by schreibfaul1 (Handles SD & Net Radio)
2. `ESP32-A2DP` by pschatzmann (Handles Bluetooth)
3. `Adafruit GFX Library`
4. `Adafruit SSD1306`

### Compilation Notes
* **Partition Scheme:** This firmware requires a lot of program memory due to the Bluetooth stack and WebServer. In your Arduino IDE / PlatformIO, set the Partition Scheme to **Huge APP (3MB No OTA/1MB SPIFFS)**.
* **Web UI:** You do not need to upload SPIFFS data. The entire Tailwind CSS Web UI is minified and embedded directly into `WebUI.h` using `PROGMEM`.

---

## 📱 The Web UI (SARVS OS)
When connected to your home WiFi, the ESP32 hosts a beautiful, responsive web interface. 
* **If it has no saved WiFi:** The DAP will boot into **Hotspot Mode** (AP Mode).
* Connect your phone to `SARVS_HIFI` (Password: `12345678`).
* Go to `http://192.168.4.1` to access the Web UI.
* Use the **System Tab** to scan for your home router, enter your password, and reboot. The DAP will permanently remember your network.

---

## 🧠 DSP Architecture Notes (For Audiophiles)
The ESP32 is a powerful microcontroller, but using digital software attenuation (lowering the volume by dividing I2S samples) reduces dynamic range and introduces quantization noise. 

In V2, the `ESP32-audioI2S` library is locked to maximum volume (`21`). The pure I2S signal is converted to analog by the PCM5102 DAC. The analog signal is then routed into the **BD37033FV**. The ESP32 acts merely as the "brain," sending I2C commands to the BD37033FV to adjust volume, EQ, and Subwoofer cutoff in the purely analog domain, preserving a 100% clean signal path.

---

## 📺 YouTube Demo & Build Guide
Watch the full breakdown, audiophile testing, and hardware explanation on YouTube!
👉 **[Watch on Sarvs Electric](https://www.youtube.com/watch?v=cMnML-cIFSs)**

---

## 📄 License
This project is open-source under the MIT License. Feel free to fork, modify, and build your own custom Audio Players!