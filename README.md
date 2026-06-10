# ESP32 Hi-Fi Audio Player Prototype Build

A versatile, feature-rich audio playback system built on the ESP32. This project combines local SD card playback, wireless Bluetooth streaming, and Internet Radio into a single device with a custom OLED user interface. 

Designed and documented for the **Sarvs Electric** community.

## Features

* **Multi-Mode Playback:** Easily switch between SD Card Audio, Bluetooth Receiver, and Internet Radio.
* **SD Card File Explorer:** Browse folders and files directly on the OLED screen. Supports MP3, WAV, and FLAC formats.
* **Bluetooth A2DP Sink:** Stream high-quality audio directly from your phone or computer.
* **Internet Radio:** Scan for WiFi networks, enter passwords via the on-screen UI, and stream live web radio stations.
* **Persistent Memory:** Automatically saves your last played SD track, volume level, EQ settings, and WiFi credentials.
* **Custom UI:** 128x64 OLED interface featuring boot animations, scrolling track titles, progress bars, and custom menus.
* **Audio Controls:** 5-button interface for Play/Pause, Next, Previous, Volume Up/Down, and EQ preset toggling.

## Hardware Requirements

> **Crucial Hardware Note:** This project relies on the Bluetooth Classic (A2DP) profile for audio streaming. You **must** use a standard ESP32 development board. Newer variants like the ESP32-S3 do not support Bluetooth Classic and will not compile or work with this code.

* Standard ESP32 Development Board (e.g., ESP32-WROOM)
* I2S Audio DAC (e.g., PCM5102A or MAX98357A)
* MicroSD Card Module (SPI)
* 128x64 OLED Display (I2C - SSD1306)
* 5x Tactile Push Buttons

## Pin Configuration

Wire your components to the ESP32 using the following pin mappings:

| Component | Pin Function | ESP32 Pin |
| :--- | :--- | :--- |
| **MicroSD Card** | CS | GPIO 5 |
| | MOSI | GPIO 23 |
| | MISO | GPIO 19 |
| | SCK | GPIO 18 |
| **I2S DAC** | DOUT (DIN) | GPIO 27 |
| | BCLK (BCK) | GPIO 26 |
| | LRC (LRCK) | GPIO 25 |
| **OLED Display** | SDA | Default I2C SDA (Usually GPIO 21) |
| | SCL | Default I2C SCL (Usually GPIO 22) |
| **Buttons (Active Low)** | Play / Pause / Select | GPIO 4 |
| | Next Track | GPIO 32 |
| | Previous Track / Back | GPIO 15 |
| | Volume Up / EQ Mode | GPIO 33 |
| | Volume Down / Play Mode | GPIO 13 |

## Required Libraries

Install the following libraries via the Arduino Library Manager or GitHub:

* **ESP32-audioI2S** (by schreibfaul1) - For I2S audio decoding and internet streaming.
* **ESP32-A2DP** (by pschatzmann) - For Bluetooth audio receiver functionality.
* **Adafruit GFX Library** - For display graphics.
* **Adafruit SSD1306** - For the OLED display driver.

## Installation & Setup

1. Clone this repository to your local machine.
2. Ensure you have the required libraries installed in your Arduino IDE.
3. Include the `DisplayImages.h` file in the same directory as your main sketch. This file contains the necessary byte arrays for the UI icons and boot animations.
4. Select your standard ESP32 board in the Arduino IDE.
5. Compile and upload the code.
6. Insert an SD card formatted to FAT32 with your favorite audio files.

## Usage Guide

* **Main Menu:** Use the Volume Up/Down buttons to navigate between USB/SD, Bluetooth, and Internet Radio. Press Play to select.
* **SD Mode:** Browse folders using the Volume buttons. Press Play to select a file or enter a folder. Long-press Previous to exit back to the main menu. Long-press Play while a song is running to return to the file browser.
* **Bluetooth Mode:** Pair your device to `Sarvs_HiFi_Player`. Playback and volume can be controlled from both your phone and the physical buttons. Long-press Play to disconnect and exit.
* **WiFi Mode:** The device will automatically scan for networks. Use the on-screen keyboard to enter your WiFi password. Once connected, select a station from the list to begin streaming.