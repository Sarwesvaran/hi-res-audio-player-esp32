/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║          SARVS HI-FI DAP FIRMWARE v8.4              ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono9pt7b.h>
#include "Audio.h"
#include "BluetoothA2DPSink.h"

#include "DisplayImages.h"
#include "WebUI.h"

// ═══════════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════════════
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define PIN_ENC_CLK 32
#define PIN_ENC_DT 33
#define PIN_ENC_SW 4
#define PIN_BTN_MUTE 14
#define PIN_BTN_PWR 35

#define PIN_SD_CS 5
#define PIN_SPI_MOSI 23
#define PIN_SPI_MISO 19
#define PIN_SPI_SCK 18

#define PIN_I2S_BCLK 26
#define PIN_I2S_LRC 25
#define PIN_I2S_DOUT 27

// ═══════════════════════════════════════════════════════════════════
//  CONFIG CONSTANTS & OBJECTS
// ═══════════════════════════════════════════════════════════════════
#define SCREEN_W 128
#define SCREEN_H 64
#define ADDR_OLED 0x3C
#define ADDR_BD37033 0x40
#define ADDR_RDA5807 0x11

#define SRC_SD 0
#define SRC_BT 1
#define SRC_RADIO 2
#define SRC_FM 3
#define SRC_AUX 4
#define SRC_SETTINGS 5
#define SRC_COUNT 6

#define BOOT_SRC_LAST 6

#define BD_INP_DIGITAL 0x00
#define BD_INP_FM 0x01
#define BD_INP_AUX 0x02

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Audio audio;
BluetoothA2DPSink a2dp_sink;
WebServer server(80);
Preferences prefs;

enum PlayMode
{
  PM_ALL,
  PM_ONE,
  PM_SHUFFLE
};
enum OledPage
{
  PG_CAROUSEL,
  PG_SD_BROWSE,
  PG_PLAYING,
  PG_WF_STATIONS,
  PG_FM_PRESETS,
  PG_SETTINGS_MENU,
  PG_DSP_MENU,
  PG_WIFI_MENU,
  PG_WIFI_CONFIRM,
  PG_STANDBY
};

// ═══════════════════════════════════════════════════════════════════
//  GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════
bool showBootAnim = false;

int currentSrc = -1;
int menuIndex = SRC_SD;
OledPage oledPage = PG_CAROUSEL;

volatile bool reqRedraw = false;
unsigned long lastInteraction = 0;
unsigned long muteOverlayTimer = 0;

bool isStandby = false;
int playFocus = 0;
bool isEditingVol = false;
unsigned long volDebounceTimer = 0;
unsigned long pendingUnmuteTimer = 0;
unsigned long unmuteDelay = 0;

// Hardware DSP & Unified Volume (0-100)
bool dspEnabled = true;
int masterVolume = 40;
bool isMuted = false;

int bassGain = 0, midGain = 0, trebGain = 0;
int subVolume = 50;
int bassQ = 1;
int subFreq = 2;
int dspSel = 0;
bool dspEdit = false;

// Settings
int bootSourceSetting = 6;
int lastActiveSrc = SRC_SD;
float fmFreq = 98.3f;
float fmPresets[3] = {98.3f, 104.1f, 106.4f};
bool isManualTune = false;
int fmPresetIdx = 0;
PlayMode playMode = PM_ALL;

String stationNames[5] = {"Mirchi", "City", "Big FM", "Parithabangal", "Thaalam"};
String stationUrls[5] = {
    "https://radios.crabdance.com:8002/1", "https://radios.crabdance.com:8002/5",
    "https://radios.crabdance.com:8002/4", "https://cast6.my-control-panel.com/proxy/parithabangaltwo/live",
    "https://ec4.yesstreaming.net:1990/stream"};

bool isPlaying = false;
String trackTitle = "";
String trackArtist = "";
int scrollX = 0, textWidth = 0;
int stationSel = 0, stationIdx = 0, stationTop = 0;
int fmPresetSel = 0;
int settingSel = 0, settingTop = 0;
int wifiSel = 0, confirmSel = 0;

bool isBtInitialized = false;
volatile bool btPlaying = false;
bool sdMounted = false;
struct FileEntry
{
  String name;
  bool isFolder;
};
std::vector<FileEntry> fileList;
std::vector<String> playlist;
String currentPath = "/";
int browserIdx = 0, browserTop = 0, trackIdx = 0;

String lastSdPath = "/";
int lastSdTrack = 0;
int sdStableTime = 0;
unsigned long sdStableClock = 0;
bool sdIsStable = false;

String currentIP = "Not Connected";
bool webActive = false;
bool wifiDisabled = false;

volatile int16_t encRaw = 0;
int16_t encLast = 0;
bool encDown = HIGH;
unsigned long encPress = 0;
bool encLong = false;
volatile bool audioEOF = false;

// Forward Declarations
void nextTrack(bool forced);
void playTrack(int idx);
void playStation(int idx);
void bd37033_apply();
void switchSource(int newSrc);
void redrawOled();
void drawCarousel();
void drawSDBrowser();
void drawPlaying();
void drawStationList();
void drawFMPresets();
void drawSettingsMenu();
void drawDSPMenu();
void drawWifiMenu();
void drawWifiConfirm();
void drawStandby();
void stopAudio();
void stopAllSources();
void loadSettings();
void saveSettings();
void setupWebRoutes();
void openFolderSD(const String &path);

const char *srcNames[SRC_COUNT] = {"SD CARD", "BLUETOOTH", "NET RADIO", "FM TUNER", "AUX IN", "SETTINGS"};

// ═══════════════════════════════════════════════════════════════════
//  ISR & DRIVERS
// ═══════════════════════════════════════════════════════════════════
void IRAM_ATTR encoderISR()
{
  static uint8_t ab = 0;
  ab = (ab << 2) | ((digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT));
  const int8_t lut[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  encRaw += lut[ab & 0x0F];
}

void audio_info(const char *info)
{
  String s = info;
  if (s.startsWith("StreamTitle="))
  {
    trackTitle = s.substring(12);
    if (trackTitle.startsWith("'"))
      trackTitle = trackTitle.substring(1, trackTitle.length() - 1);
    textWidth = trackTitle.length() * 11;
    scrollX = 0;
    reqRedraw = true;
  }
}
void audio_eof_mp3(const char *) { audioEOF = true; }
void avrc_meta_cb(uint8_t id, const uint8_t *text)
{
  String v = (char *)text;
  if (id == ESP_AVRC_MD_ATTR_TITLE)
  {
    trackTitle = v;
    textWidth = v.length() * 11;
    scrollX = 0;
    reqRedraw = true;
  }
  if (id == ESP_AVRC_MD_ATTR_ARTIST)
  {
    trackArtist = v;
    reqRedraw = true;
  }
}
void bt_vol_cb(int v)
{
  if (!dspEnabled)
  {
    masterVolume = ::map(v, 0, 127, 0, 100);
    reqRedraw = true;
    saveSettings();
  }
}
void bt_state_cb(esp_a2d_audio_state_t s, void *)
{
  btPlaying = (s == ESP_A2D_AUDIO_STATE_STARTED);
  isPlaying = btPlaying;
  reqRedraw = true;
}

void writeBD(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(ADDR_BD37033);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void bd37033_init()
{
  writeBD(0x01, 0x24);
  writeBD(0x03, 0xDF);
  writeBD(0x44, 0x01);
  writeBD(0x47, 0x01);
  writeBD(0x54, 128);
  writeBD(0x57, 128);
  writeBD(0x75, 0x00);
  writeBD(0x28, 128);
  writeBD(0x29, 128);
  writeBD(0x2A, 128);
  writeBD(0x2B, 128);
  writeBD(0x06, 0x00);
}

void bd37033_apply()
{
  if (!dspEnabled)
  {
    writeBD(0x20, 255);
    writeBD(0x2C, 255);
    writeBD(0x30, 255);
    audio.setVolume(::map(masterVolume, 0, 100, 0, 21));
    if (isBtInitialized)
      a2dp_sink.set_volume(::map(masterVolume, 0, 100, 0, 127));
    return;
  }

  audio.setVolume(21);
  if (isBtInitialized)
    a2dp_sink.set_volume(127);

  int hwVol = ::map(masterVolume, 0, 100, -79, 5);
  if (isMuted)
    hwVol = -79;
  int vSend = (hwVol > 0) ? (128 - hwVol) : (abs(hwVol) + 128);
  writeBD(0x20, vSend);

  uint8_t inp = BD_INP_DIGITAL;
  if (currentSrc == SRC_FM)
    inp = BD_INP_FM;
  if (currentSrc == SRC_AUX)
    inp = BD_INP_AUX;
  writeBD(0x05, inp);

  writeBD(0x51, (bassGain >= 0) ? bassGain : (abs(bassGain) | 128));
  uint8_t bassSetup = 0x10 | (bassQ & 0x03);
  writeBD(0x41, bassSetup);

  writeBD(0x54, (midGain >= 0) ? midGain : (abs(midGain) | 128));
  writeBD(0x57, (trebGain >= 0) ? trebGain : (abs(trebGain) | 128));

  uint8_t subSetup = 0x30 | (subFreq & 0x07);
  writeBD(0x02, subSetup);

  int hwSub = ::map(subVolume, 0, 100, -79, 0);
  if (isMuted)
    hwSub = -79;
  int sSend = (hwSub > 0) ? (128 - hwSub) : (abs(hwSub) + 128);
  writeBD(0x2C, sSend);
  writeBD(0x30, sSend);
}

void setVolumeFast()
{
  if (pendingUnmuteTimer > 0)
    return; 

  if (dspEnabled)
  {
    int hwVol = ::map(masterVolume, 0, 100, -79, 5);
    if (isMuted)
      hwVol = -79;
    int vSend = (hwVol > 0) ? (128 - hwVol) : (abs(hwVol) + 128);
    writeBD(0x20, vSend);
  }
  else
  {
    audio.setVolume(::map(masterVolume, 0, 100, 0, 21));
    if (isBtInitialized)
      a2dp_sink.set_volume(::map(masterVolume, 0, 100, 0, 127));
  }
}

void writeFM(uint8_t reg, uint16_t val)
{
  Wire.beginTransmission(ADDR_RDA5807);
  Wire.write(reg);
  Wire.write(val >> 8);
  Wire.write(val & 0xFF);
  Wire.endTransmission();
  delay(10);
}
void rda5807_init()
{
  writeFM(0x02, 0x0002);
  delay(50);
  writeFM(0x02, 0xC00D);
  writeFM(0x05, 0x84AF);
}
void setFMFreq(float freq)
{
  fmFreq = constrain(freq, 87.5f, 108.0f);
  uint16_t ch = (uint16_t)((fmFreq * 10.0f) - 870.0f);
  writeFM(0x03, (ch << 6) | 0x0010);
}

// ═══════════════════════════════════════════════════════════════════
//  NVS PERSISTENCE
// ═══════════════════════════════════════════════════════════════════
void loadSettings()
{
  prefs.begin("sarvs", true);
  dspEnabled = prefs.getBool("dspEn", true);
  masterVolume = prefs.getInt("vol", 40);
  bassGain = prefs.getInt("bass", 0);
  midGain = prefs.getInt("mid", 0);
  trebGain = prefs.getInt("treb", 0);
  subVolume = prefs.getInt("subv", 50);
  bassQ = prefs.getInt("bassQ", 1);
  subFreq = prefs.getInt("subF", 2);
  bootSourceSetting = prefs.getInt("bootSrc", 6);
  lastActiveSrc = prefs.getInt("lastSrc", SRC_SD);
  fmFreq = prefs.getFloat("fm", 98.3f);
  playMode = (PlayMode)prefs.getInt("pm", 0);
  lastSdPath = prefs.getString("sdPath", "/");
  lastSdTrack = prefs.getInt("sdTrk", 0);
  for (int i = 0; i < 3; i++)
    fmPresets[i] = prefs.getFloat(("fp" + String(i)).c_str(), fmPresets[i]);
  for (int i = 0; i < 5; i++)
  {
    stationNames[i] = prefs.getString(("rn" + String(i)).c_str(), stationNames[i]);
    stationUrls[i] = prefs.getString(("ru" + String(i)).c_str(), stationUrls[i]);
  }
  prefs.end();
}

void saveSettings()
{
  prefs.begin("sarvs", false);
  prefs.putBool("dspEn", dspEnabled);
  prefs.putInt("vol", masterVolume);
  prefs.putInt("bass", bassGain);
  prefs.putInt("mid", midGain);
  prefs.putInt("treb", trebGain);
  prefs.putInt("subv", subVolume);
  prefs.putInt("bassQ", bassQ);
  prefs.putInt("subF", subFreq);
  prefs.putInt("bootSrc", bootSourceSetting);
  prefs.putInt("lastSrc", currentSrc);
  prefs.putFloat("fm", fmFreq);
  prefs.putInt("pm", (int)playMode);
  prefs.putString("sdPath", currentPath);
  prefs.putInt("sdTrk", trackIdx);
  for (int i = 0; i < 3; i++)
    prefs.putFloat(("fp" + String(i)).c_str(), fmPresets[i]);
  for (int i = 0; i < 5; i++)
  {
    prefs.putString(("rn" + String(i)).c_str(), stationNames[i]);
    prefs.putString(("ru" + String(i)).c_str(), stationUrls[i]);
  }
  prefs.end();
}

// ═══════════════════════════════════════════════════════════════════
//  AUDIO ROUTING & LOGIC
// ═══════════════════════════════════════════════════════════════════
void stopAudio() { audio.stopSong(); }
void stopAllSources()
{
  stopAudio();
  if (isBtInitialized)
  {
    a2dp_sink.disconnect();
    a2dp_sink.pause();
  }
}

void switchSource(int newSrc)
{
  if (newSrc == currentSrc && !isStandby)
    return;

  if (!dspEnabled && (newSrc == SRC_FM || newSrc == SRC_AUX))
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 25);
    display.print(" Hardware DSP OFF! ");
    display.setCursor(0, 35);
    display.print(" Enable for this Src");
    display.display();
    delay(1500);
    redrawOled();
    return;
  }

  if (newSrc == SRC_RADIO && WiFi.status() != WL_CONNECTED)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 25);
    display.print("WiFi Not Connected!");
    display.setCursor(10, 35);
    display.print("Check Settings Menu");
    display.display();
    delay(1500);
    redrawOled();
    return;
  }

  if (currentSrc == SRC_BT && isBtInitialized && newSrc != SRC_BT)
  {
    a2dp_sink.disconnect();
    a2dp_sink.pause();
    display.clearDisplay();
    display.setCursor(20, 30);
    display.print("Restoring WiFi..");
    display.display();
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);

    prefs.begin("sarvs", true);
    String savedSSID = prefs.getString("ssid", "");
    String savedPWD = prefs.getString("pwd", "");
    prefs.end();

    if (savedSSID.length() > 0)
    {
      WiFi.begin(savedSSID.c_str(), savedPWD.c_str());
      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 40)
      {
        delay(250);
        tries++;
      }
      if (WiFi.status() == WL_CONNECTED)
      {
        currentIP = WiFi.localIP().toString();
        wifiDisabled = false;
        if (!webActive)
        {
          server.begin();
          webActive = true;
        }
      }
    }
  }
  else
  {
    stopAudio();
  }

  currentSrc = newSrc;
  isPlaying = false;
  isStandby = false;
  trackTitle = srcNames[newSrc];
  trackArtist = "";
  scrollX = 0;
  textWidth = 0;
  playFocus = 0;
  isEditingVol = false;
  lastInteraction = millis();

  bool autoPlayed = false;

  switch (newSrc)
  {
  case SRC_SD:
    audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    if (sdMounted)
    {
      openFolderSD(lastSdPath);
      if (fileList.size() > 0 && lastSdTrack < playlist.size())
      {
        playTrack(lastSdTrack);
        autoPlayed = true;
      }
      else
      {
        oledPage = PG_SD_BROWSE;
        redrawOled();
      }
    }
    else
    {
      trackTitle = "No SD Card!";
      oledPage = PG_CAROUSEL;
      menuIndex = 0;
      currentSrc = -1;
      redrawOled();
    }
    break;
  case SRC_BT:
    display.clearDisplay();
    display.setCursor(15, 30);
    display.print("Disabling WiFi..");
    display.display();
    if (webActive)
    {
      server.stop();
      webActive = false;
    }
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    wifiDisabled = true;
    currentIP = "OFFLINE";
    delay(300);

    if (!isBtInitialized)
    {
      i2s_pin_config_t pc = {.bck_io_num = PIN_I2S_BCLK, .ws_io_num = PIN_I2S_LRC, .data_out_num = PIN_I2S_DOUT, .data_in_num = I2S_PIN_NO_CHANGE};
      a2dp_sink.set_pin_config(pc);
      a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
      a2dp_sink.set_avrc_metadata_callback(avrc_meta_cb);
      a2dp_sink.set_on_volumechange(bt_vol_cb);
      a2dp_sink.set_on_audio_state_changed(bt_state_cb);
      a2dp_sink.start("SARVS_HiFi", false);
      isBtInitialized = true;
    }
    else
    {
      a2dp_sink.reconnect();
      a2dp_sink.play();
    }
    trackTitle = "Waiting for BT...";
    oledPage = PG_PLAYING;
    redrawOled();
    break;
  case SRC_RADIO:
    audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    oledPage = PG_WF_STATIONS;
    redrawOled();
    break;
  case SRC_FM:
    rda5807_init();
    setFMFreq(fmFreq);
    trackTitle = String(fmFreq, 1) + " MHz";
    isPlaying = true;
    oledPage = PG_PLAYING;
    redrawOled();
    break;
  case SRC_AUX:
    trackTitle = "Analog In";
    isPlaying = true;
    oledPage = PG_PLAYING;
    redrawOled();
    break;
  }

  if (!autoPlayed)
    bd37033_apply();
}

void doPlayPauseCommand()
{
  if (currentSrc == SRC_SD)
  {
    audio.pauseResume();
    isPlaying = !isPlaying;
  }
  else if (currentSrc == SRC_BT && isBtInitialized)
  {
    btPlaying ? a2dp_sink.pause() : a2dp_sink.play();
  }
  else if (currentSrc == SRC_FM || currentSrc == SRC_AUX)
  {
    isMuted = !isMuted;
    bd37033_apply();
    muteOverlayTimer = millis();
    reqRedraw = true;
  }
  else if (currentSrc == SRC_RADIO)
  {
    if (isPlaying)
    {
      audio.stopSong();
      isPlaying = false;
    }
    else
    {
      playStation(stationIdx);
    }
  }
}

void doNextCommand()
{
  if (currentSrc == SRC_SD)
    nextTrack(true);
  else if (currentSrc == SRC_BT && isBtInitialized)
    a2dp_sink.next();
  else if (currentSrc == SRC_RADIO)
    playStation((stationIdx + 1) % 5);
  else if (currentSrc == SRC_FM)
  {
    fmFreq += 0.1f;
    if (fmFreq > 108.0f)
      fmFreq = 87.5f;
    setFMFreq(fmFreq);
    isManualTune = true;
  }
}

void doPrevCommand()
{
  if (currentSrc == SRC_SD)
  {
    trackIdx = (trackIdx - 1 + playlist.size()) % playlist.size();
    playTrack(trackIdx);
  }
  else if (currentSrc == SRC_BT && isBtInitialized)
    a2dp_sink.previous();
  else if (currentSrc == SRC_RADIO)
    playStation((stationIdx - 1 + 5) % 5);
  else if (currentSrc == SRC_FM)
  {
    fmFreq -= 0.1f;
    if (fmFreq < 87.5f)
      fmFreq = 108.0f;
    setFMFreq(fmFreq);
    isManualTune = true;
  }
}

void cyclePlayMode()
{
  playMode = (PlayMode)((playMode + 1) % 3);
  saveSettings();
}

void openFolderSD(const String &path)
{
  currentPath = path;
  File root = SD.open(currentPath);
  if (!root || !root.isDirectory())
    return;
  fileList.clear();
  playlist.clear();
  if (currentPath != "/")
    fileList.push_back({"[ .. BACK ]", true});
  File f = root.openNextFile();
  while (f)
  {
    String n = String(f.name());
    int slashIdx = n.lastIndexOf('/');
    if (slashIdx >= 0)
      n = n.substring(slashIdx + 1);
    if (!n.startsWith(".") && (f.isDirectory() || n.endsWith(".mp3") || n.endsWith(".flac") || n.endsWith(".wav")))
    {
      fileList.push_back({n, f.isDirectory()});
      if (!f.isDirectory())
        playlist.push_back(n);
    }
    f = root.openNextFile();
  }
  root.close();
  browserIdx = 0;
  browserTop = 0;
}

void playTrack(int idx)
{
  if (idx < 0 || idx >= playlist.size())
    return;
  trackIdx = idx;
  String fullPath = currentPath;
  if (!fullPath.endsWith("/"))
    fullPath += "/";
  fullPath += playlist[idx];

  saveSettings();

  if (dspEnabled)
    writeBD(0x20, 0xFF); 

  sdIsStable = false;
  sdStableTime = 0;
  sdStableClock = millis();
  trackTitle = playlist[idx];
  textWidth = trackTitle.length() * 11;
  scrollX = 0;
  isPlaying = true;
  reqRedraw = true;

  oledPage = PG_PLAYING;
  redrawOled();

  stopAudio();
  audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  audio.connecttoFS(SD, fullPath.c_str());
  if (!dspEnabled)
    audio.setVolume(::map(masterVolume, 0, 100, 0, 21));
  else
    audio.setVolume(21);

  pendingUnmuteTimer = millis();
  unmuteDelay = 500; // Unmute buffer
}

void nextTrack(bool forced)
{
  if (playMode == PM_ONE && !forced)
  {
    playTrack(trackIdx);
    return;
  }
  if (playMode == PM_SHUFFLE && playlist.size() > 1)
  {
    int n;
    do
    {
      n = random(0, playlist.size());
    } while (n == trackIdx);
    playTrack(n);
    return;
  }
  trackIdx = (trackIdx + 1) % playlist.size();
  playTrack(trackIdx);
}

void playStation(int idx)
{
  stationIdx = idx;
  trackTitle = "Buffering...";
  trackArtist = stationNames[idx];
  textWidth = trackTitle.length() * 11;
  scrollX = 0;

  if (dspEnabled)
    writeBD(0x20, 0xFF);

  isPlaying = true;
  oledPage = PG_PLAYING;
  redrawOled();

  stopAudio();
  audio.connecttohost(stationUrls[idx].c_str());
  if (!dspEnabled)
    audio.setVolume(::map(masterVolume, 0, 100, 0, 21));
  else
    audio.setVolume(21);

  pendingUnmuteTimer = millis();
  unmuteDelay = 1500;
}

// ═══════════════════════════════════════════════════════════════════
//  OLED DRAWING
// ═══════════════════════════════════════════════════════════════════
void drawCarousel()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.drawRect(46, 16, 36, 34, 1);
  display.drawLine(0, 11, 127, 11, 1);

  display.setTextWrap(false);
  display.setCursor(38, 2);
  display.print("MAIN MENU");

  int idx = menuIndex;
  int tw = strlen(srcNames[idx]) * 6;
  display.setCursor((SCREEN_W - tw) / 2, 53);
  display.print(srcNames[idx]);

  auto drawIcon = [](int i, int bx)
  {
    int ix = bx, iw = 32;
    if (i == SRC_BT || i == SRC_SETTINGS)
    {
      ix = bx + 2;
      iw = 28;
    }
    else if (i == SRC_RADIO)
    {
      ix = bx + 1;
      iw = 30;
    }
    else if (i == SRC_SD)
    {
      ix = bx + 5;
      iw = 22;
    }

    int iy = (i == SRC_SD) ? 25 : 18;
    int ih = (i == SRC_SD) ? 16 : 32;
    if (i == SRC_FM)
      iy = 16;

    display.drawBitmap(ix, iy, (const uint8_t *)pgm_read_ptr(&srcIcons[i]), iw, ih, WHITE);
  };

  drawIcon(idx, 48);                              // Center
  drawIcon((idx - 1 + SRC_COUNT) % SRC_COUNT, 8); // Left
  drawIcon((idx + 1) % SRC_COUNT, 88);            // Right

  display.display();
}

void drawPlaying()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextWrap(false);

  display.setFont();
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(srcNames[currentSrc]);
  display.setCursor(84, 2);
  display.printf("Vol:%d", masterVolume);
  display.drawLine(0, 11, 127, 11, 1);

  display.setFont(&FreeMono9pt7b);
  display.setCursor(scrollX, 26);
  if (currentSrc == SRC_FM)
  {
    if (isManualTune)
      display.print("Manual Tune");
    else
      display.printf("Preset %d", fmPresetIdx + 1);
  }
  else
  {
    display.print(trackTitle);
  }

  display.setFont();
  display.setCursor(3, 35);
  if (currentSrc == SRC_SD)
  {
    int cur = audio.getAudioCurrentTime();
    int tot = audio.getAudioFileDuration();

    if (!sdIsStable)
    {
      if (tot == sdStableTime && tot > 0)
      {
        if (millis() - sdStableClock > 2000)
          sdIsStable = true;
      }
      else
      {
        sdStableTime = tot;
        sdStableClock = millis();
      }
    }

    if (!sdIsStable || tot <= 0)
    {
      display.print("Calculating...");
    }
    else
    {
      display.printf("%02d:%02d / %02d:%02d", cur / 60, cur % 60, tot / 60, tot % 60);
      int w = (cur * 120) / tot;
      display.drawLine(4, 45, 124, 45, WHITE);
      display.fillRect(4, 44, w, 3, WHITE);
    }
  }
  else if (currentSrc == SRC_BT || currentSrc == SRC_RADIO)
  {
    display.print(trackArtist.substring(0, 20));
  }
  else if (currentSrc == SRC_FM)
  {
    display.printf("%.1f MHz", fmFreq);
  }

  display.drawRect(0, 53, 128, 11, 1);

  int focusPlay = (currentSrc == SRC_AUX) ? 1 : 2;
  int focusPrev = (currentSrc == SRC_AUX) ? -1 : 1;
  int focusNext = (currentSrc == SRC_AUX) ? -1 : 3;

  if (isEditingVol)
  {
    display.fillRect(1, 54, 26, 9, WHITE);
  }
  else if (playFocus == 0)
  {
    display.drawRect(1, 54, 26, 9, WHITE);
  }

  if (!isEditingVol)
  {
    if (playFocus == focusPrev)
      display.fillRect(31, 54, 13, 9, WHITE);
    else if (playFocus == focusPlay)
      display.fillRect(59, 54, 11, 9, WHITE);
    else if (playFocus == focusNext)
      display.fillRect(84, 54, 13, 9, WHITE);
    else if (playFocus == 4 && currentSrc == SRC_SD)
      display.fillRect(107, 54, 19, 9, WHITE);
  }

  display.setTextColor(isEditingVol ? BLACK : WHITE);
  display.setCursor(2, 55);
  display.print(isEditingVol ? "+-" : "Vol");

  display.setTextColor(WHITE);
  if (currentSrc == SRC_AUX)
  {
    if (isMuted)
      display.drawBitmap(61, 55, icon_pause, 7, 7, (playFocus == focusPlay) ? BLACK : WHITE);
    else
      display.drawBitmap(62, 55, icon_play, 4, 7, (playFocus == focusPlay) ? BLACK : WHITE);
  }
  else
  {
    display.drawBitmap(33, 55, icon_prev, 9, 7, (playFocus == focusPrev) ? BLACK : WHITE);
    display.drawBitmap(86, 55, icon_next, 9, 7, (playFocus == focusNext) ? BLACK : WHITE);

    bool drawPause = (currentSrc == SRC_FM) ? isMuted : isPlaying;
    if (drawPause)
      display.drawBitmap(61, 55, icon_pause, 7, 7, (playFocus == focusPlay) ? BLACK : WHITE);
    else
      display.drawBitmap(62, 55, icon_play, 4, 7, (playFocus == focusPlay) ? BLACK : WHITE);

    if (currentSrc == SRC_SD)
    {
      display.setTextColor(playFocus == 4 ? BLACK : WHITE);
      display.setCursor(109, 55);
      const char *m[] = {"ALL", "ONE", "SHF"};
      display.print(m[playMode]);
    }
  }

  display.display();
  display.setTextWrap(true);
}

void drawListMenu(const char *title, int sel, int top, int maxItems, std::function<String(int)> getLbl)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(title);
  display.drawLine(0, 10, SCREEN_W, 10, WHITE);
  if (sel < top)
    top = sel;
  if (sel >= top + 3)
    top = sel - 2;
  for (int i = 0; i < 3; i++)
  {
    int idx = top + i;
    if (idx >= maxItems)
      break;
    int y = 13 + i * 17;
    if (idx == sel)
    {
      display.fillRect(0, y, SCREEN_W, 16, WHITE);
      display.setTextColor(BLACK);
    }
    else
      display.setTextColor(WHITE);
    display.setCursor(4, y + 4);
    display.print(getLbl(idx).substring(0, 20));
  }
  display.display();
}

void drawSDBrowser()
{
  drawListMenu("SD Browser", browserIdx, browserTop, fileList.size(), [](int i)
               { return fileList[i].name; });
}
void drawStationList()
{
  drawListMenu("Web Radio", stationSel, stationTop, 5, [](int i)
               { return stationNames[i]; });
}
void drawFMPresets()
{
  drawListMenu("FM Presets", fmPresetSel, 0, 3, [](int i)
               { return String(fmPresets[i], 1) + " MHz"; });
}
void drawSettingsMenu()
{
  const char *s[] = {"WiFi Menu", "DSP Tuning", "Reboot OS", "< Exit"};
  drawListMenu("Settings", settingSel, settingTop, 4, [&](int i)
               { return String(s[i]); });
}

void drawDSPMenu()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("DSP Tuning");
  display.drawLine(0, 10, SCREEN_W, 10, WHITE);

  const char *lbls[] = {"DSP Pwr", "Bass", "Mid", "Treble", "Bass Q", "Sub LPF", "Sub Vol", "< Back"};
  String qVals[] = {"0.5", "1.0", "1.5", "2.0"};
  String lpfVals[] = {"Flat", "55Hz", "85Hz", "120Hz", "160Hz"};

  int top = dspSel;
  if (top > 5)
    top = 5;
  for (int i = 0; i < 3; i++)
  {
    int idx = top + i;
    if (idx > 7)
      break;
    int y = 13 + i * 17;
    if (idx == dspSel)
    {
      display.fillRect(0, y, SCREEN_W, 16, WHITE);
      display.setTextColor(BLACK);
    }
    else
      display.setTextColor(WHITE);
    display.setCursor(4, y + 4);

    if (idx == 0)
    {
      display.printf("DSP: %s", dspEnabled ? "ON" : "OFF");
    }
    else if (idx < 7)
    {
      display.printf("%s: ", lbls[idx]);
      if (dspEdit && idx == dspSel)
        display.print("[ ");
      if (idx == 1)
        display.print(bassGain);
      else if (idx == 2)
        display.print(midGain);
      else if (idx == 3)
        display.print(trebGain);
      else if (idx == 4)
        display.print(qVals[bassQ]);
      else if (idx == 5)
        display.print(lpfVals[subFreq]);
      else if (idx == 6)
        display.print(subVolume);
      if (dspEdit && idx == dspSel)
        display.print(" ]");
    }
    else
      display.print(lbls[idx]);
  }
  display.display();
}

void drawWifiMenu()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("WiFi Config");
  display.drawLine(0, 10, SCREEN_W, 10, WHITE);
  String stat = (wifiDisabled) ? "Disabled" : currentIP;
  if (currentIP == "Failed")
    stat = "Connect Failed!";
  const char *lbls[] = {stat.c_str(), "Connect WiFi", "Start AP Mode", "Turn Off WiFi", "< Back"};
  int top = wifiSel;
  if (top > 2)
    top = 2;
  for (int i = 0; i < 3; i++)
  {
    int idx = top + i;
    if (idx > 4)
      break;
    int y = 13 + i * 17;
    if (idx == wifiSel)
    {
      display.fillRect(0, y, SCREEN_W, 16, WHITE);
      display.setTextColor(BLACK);
    }
    else
      display.setTextColor(WHITE);
    display.setCursor(4, y + 4);
    display.print(lbls[idx]);
  }
  display.display();
}

void drawWifiConfirm()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.print("Disable WiFi & Net?");
  display.setCursor(20, 30);
  if (confirmSel == 0)
  {
    display.setTextColor(BLACK, WHITE);
  }
  display.print(" YES ");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(80, 30);
  if (confirmSel == 1)
  {
    display.setTextColor(BLACK, WHITE);
  }
  display.print(" NO ");
  display.setTextColor(WHITE, BLACK);
  display.display();
}

void drawStandby()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  struct tm ti;
  if (getLocalTime(&ti, 10))
  {
    display.setTextSize(3);
    display.setCursor(20, 15);
    display.printf("%02d:%02d", ti.tm_hour, ti.tm_min);
    display.setTextSize(1);
    display.setCursor(30, 45);
    display.printf("%02d/%02d/%04d", ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
  }
  else
  {
    unsigned long up = millis() / 60000;
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.print("STANDBY");
    display.setTextSize(1);
    display.setCursor(20, 45);
    display.printf("Up: %lu min", up);
  }
  display.display();
}

void redrawOled()
{
  if (isStandby)
  {
    drawStandby();
    return;
  }
  switch (oledPage)
  {
  case PG_CAROUSEL:
    drawCarousel();
    break;
  case PG_PLAYING:
    drawPlaying();
    break;
  case PG_SD_BROWSE:
    drawSDBrowser();
    break;
  case PG_WF_STATIONS:
    drawStationList();
    break;
  case PG_FM_PRESETS:
    drawFMPresets();
    break;
  case PG_SETTINGS_MENU:
    drawSettingsMenu();
    break;
  case PG_DSP_MENU:
    drawDSPMenu();
    break;
  case PG_WIFI_MENU:
    drawWifiMenu();
    break;
  case PG_WIFI_CONFIRM:
    drawWifiConfirm();
    break;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  INPUT HANDLING
// ═══════════════════════════════════════════════════════════════════
void handleInputs()
{
  int16_t step = encRaw / 4;
  if (step != encLast && !isStandby)
  {
    int dir = (step < encLast) ? 1 : -1;
    encLast = step;
    lastInteraction = millis();
    switch (oledPage)
    {
    case PG_CAROUSEL:
      menuIndex = constrain(menuIndex + dir, 0, SRC_COUNT - 1);
      break;
    case PG_SD_BROWSE:
      browserIdx = constrain(browserIdx + dir, 0, fileList.size() - 1);
      break;
    case PG_WF_STATIONS:
      stationSel = constrain(stationSel + dir, 0, 4);
      break;
    case PG_FM_PRESETS:
      fmPresetSel = constrain(fmPresetSel + dir, 0, 2);
      break;
    case PG_SETTINGS_MENU:
      settingSel = constrain(settingSel + dir, 0, 3);
      break;
    case PG_WIFI_MENU:
      wifiSel = constrain(wifiSel + dir, 0, 4);
      break;
    case PG_WIFI_CONFIRM:
      confirmSel = constrain(confirmSel + dir, 0, 1);
      break;
    case PG_DSP_MENU:
      if (dspEdit)
      {
        if (dspSel == 1)
          bassGain = constrain(bassGain + dir, -15, 15);
        else if (dspSel == 2)
          midGain = constrain(midGain + dir, -15, 15);
        else if (dspSel == 3)
          trebGain = constrain(trebGain + dir, -15, 15);
        else if (dspSel == 4)
          bassQ = constrain(bassQ + dir, 0, 3);
        else if (dspSel == 5)
          subFreq = constrain(subFreq + dir, 0, 4);
        else if (dspSel == 6)
          subVolume = constrain(subVolume + dir, 0, 100);
        bd37033_apply();
      }
      else
      {
        dspSel = constrain(dspSel + dir, 0, 7);
      }
      break;
    case PG_PLAYING:
      if (isEditingVol)
      {
        masterVolume = constrain(masterVolume + dir, 0, 100);
        volDebounceTimer = millis();
      }
      else
      {
        int maxF = (currentSrc == SRC_AUX) ? 1 : (currentSrc == SRC_SD ? 4 : 3);
        playFocus = constrain(playFocus + dir, 0, maxF);
      }
      break;
    }
    redrawOled();
  }

  bool btn = digitalRead(PIN_ENC_SW);
  if (btn == LOW && encDown == HIGH)
  {
    encPress = millis();
    encLong = false;
    encDown = LOW;
  }
  if (btn == LOW && !encLong && millis() - encPress > 800)
  {
    encLong = true;
    lastInteraction = millis();
    if (!isStandby && oledPage != PG_CAROUSEL)
    {
      oledPage = PG_CAROUSEL;
      menuIndex = (currentSrc >= 0) ? currentSrc : 0;
      redrawOled();
    }
  }
  if (btn == HIGH && encDown == LOW)
  {
    encDown = HIGH;
    if (encLong)
      return;
    if (!isStandby)
    {
      lastInteraction = millis();
      switch (oledPage)
      {
      case PG_CAROUSEL:
        if (menuIndex == SRC_SETTINGS)
        {
          oledPage = PG_SETTINGS_MENU;
          redrawOled();
        }
        else if (menuIndex == currentSrc)
        {
          oledPage = PG_PLAYING;
          redrawOled();
        }
        else
          switchSource(menuIndex);
        break;
      case PG_PLAYING:
        if (isEditingVol)
        {
          isEditingVol = false;
          saveSettings();
        }
        else
        {
          if (playFocus == 0)
            isEditingVol = true;
          else if (currentSrc == SRC_AUX)
          {
            if (playFocus == 1)
              doPlayPauseCommand();
          }
          else
          {
            if (playFocus == 1)
              doPrevCommand();
            else if (playFocus == 2)
              doPlayPauseCommand();
            else if (playFocus == 3)
              doNextCommand();
            else if (playFocus == 4)
              cyclePlayMode();
          }
        }
        break;
      case PG_SD_BROWSE:
        if (fileList[browserIdx].isFolder)
        {
          String clicked = fileList[browserIdx].name;
          if (clicked == "[ .. BACK ]")
          {
            int lastSlash = currentPath.lastIndexOf('/');
            if (lastSlash > 0)
              currentPath = currentPath.substring(0, lastSlash);
            else
              currentPath = "/";
          }
          else
          {
            if (!currentPath.endsWith("/"))
              currentPath += "/";
            currentPath += clicked;
          }
          openFolderSD(currentPath);
          redrawOled();
        }
        else
        {
          playlist.clear();
          int pIdx = 0;
          for (int i = 0; i < fileList.size(); i++)
          {
            if (!fileList[i].isFolder)
            {
              playlist.push_back(fileList[i].name);
              if (i == browserIdx)
                trackIdx = pIdx;
              pIdx++;
            }
          }
          playTrack(trackIdx);
          oledPage = PG_PLAYING;
          redrawOled();
        }
        break;
      case PG_WF_STATIONS:
        playStation(stationSel);
        break;
      case PG_FM_PRESETS:
        fmPresetIdx = fmPresetSel;
        isManualTune = false;
        fmFreq = fmPresets[fmPresetSel];
        setFMFreq(fmFreq);
        oledPage = PG_PLAYING;
        redrawOled();
        break;
      case PG_DSP_MENU:
        if (dspSel == 0)
        {
          dspEnabled = !dspEnabled;
          bd37033_apply();
          saveSettings();
          redrawOled();
        }
        else if (dspSel == 7)
        {
          oledPage = PG_SETTINGS_MENU;
          saveSettings();
          redrawOled();
        }
        else
        {
          dspEdit = !dspEdit;
          redrawOled();
        }
        break;
      case PG_WIFI_CONFIRM:
        if (confirmSel == 0)
        {
          if (webActive)
          {
            server.stop();
            webActive = false;
          }
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          wifiDisabled = true;
          currentIP = "OFFLINE";
        }
        oledPage = PG_WIFI_MENU;
        redrawOled();
        break;
      case PG_WIFI_MENU:
        if (wifiSel == 1)
        {
          display.clearDisplay();
          display.setCursor(10, 30);
          display.print("Connecting...");
          display.display();
          WiFi.disconnect(true);
          delay(100);
          WiFi.mode(WIFI_STA);
          wifiDisabled = false;
          delay(100);

          prefs.begin("sarvs", true);
          String savedSSID = prefs.getString("ssid", "");
          String savedPWD = prefs.getString("pwd", "");
          prefs.end();

          if (savedSSID.length() > 0)
          {
            WiFi.begin(savedSSID.c_str(), savedPWD.c_str());
            int tries = 0;
            while (WiFi.status() != WL_CONNECTED && tries < 40)
            {
              delay(250);
              tries++;
            }
            if (WiFi.status() == WL_CONNECTED)
            {
              currentIP = WiFi.localIP().toString();
              if (!webActive)
              {
                server.begin();
                webActive = true;
              }
            }
            else
            {
              currentIP = "Failed";
            }
          }
          else
          {
            currentIP = "No SSID";
          }
          delay(1500);
          oledPage = PG_SETTINGS_MENU;
          redrawOled();
        }
        else if (wifiSel == 2)
        {
          display.clearDisplay();
          display.setCursor(10, 30);
          display.print("Starting Hotspot..");
          display.display();
          if (webActive)
          {
            server.stop();
            webActive = false;
          }
          WiFi.disconnect(true);
          delay(100);
          WiFi.mode(WIFI_AP);
          delay(100);
          WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
          WiFi.softAP("SARVS_HIFI", "12345678");
          currentIP = "192.168.4.1";
          wifiDisabled = false;
          delay(500);
          server.begin();
          webActive = true;
          delay(1000);
          oledPage = PG_SETTINGS_MENU;
          redrawOled();
        }
        else if (wifiSel == 3)
        {
          oledPage = PG_WIFI_CONFIRM;
          redrawOled();
        }
        else if (wifiSel == 4)
        {
          oledPage = PG_SETTINGS_MENU;
          redrawOled();
        }
        break;
      case PG_SETTINGS_MENU:
        if (settingSel == 0)
        {
          oledPage = PG_WIFI_MENU;
          wifiSel = 0;
          redrawOled();
        }
        else if (settingSel == 1)
        {
          oledPage = PG_DSP_MENU;
          dspSel = 0;
          dspEdit = false;
          redrawOled();
        }
        else if (settingSel == 2)
        {
          display.clearDisplay();
          display.setCursor(20, 30);
          display.print("Rebooting...");
          display.display();
          delay(500);
          ESP.restart();
        }
        else
        {
          oledPage = PG_CAROUSEL;
          redrawOled();
        }
        break;
      }
      redrawOled();
    }
  }

  static unsigned long mPress = 0;
  static bool mDown = false, mLong = false;
  bool mBtn = digitalRead(PIN_BTN_MUTE);
  if (mBtn == LOW && !mDown)
  {
    mPress = millis();
    mDown = true;
    mLong = false;
  }
  if (mBtn == LOW && mDown && !mLong && (millis() - mPress > 800) && !isStandby)
  {
    mLong = true;
    lastInteraction = millis();
    if (currentSrc == SRC_SD)
      oledPage = PG_SD_BROWSE;
    else if (currentSrc == SRC_RADIO)
      oledPage = PG_WF_STATIONS;
    else if (currentSrc == SRC_FM)
      oledPage = PG_FM_PRESETS;
    else
      oledPage = PG_SETTINGS_MENU;
    redrawOled();
  }
  if (mBtn == HIGH && mDown)
  {
    mDown = false;
    if (!mLong && !isStandby)
    {
      isMuted = !isMuted;
      bd37033_apply();
      muteOverlayTimer = millis();
      reqRedraw = true;
    }
  }

  static unsigned long pPress = 0;
  static bool pDown = false, pLong = false;
  bool pBtn = digitalRead(PIN_BTN_PWR);
  if (pBtn == LOW && !pDown)
  {
    pPress = millis();
    pDown = true;
    pLong = false;
  }
  if (pBtn == LOW && pDown && !pLong && (millis() - pPress > 1500))
  {
    pLong = true;
    isStandby = !isStandby;
    if (isStandby)
    {
      stopAllSources();
      isMuted = true;
      bd37033_apply();
    }
    else
    {
      isMuted = false;
      bd37033_apply();
      switchSource(currentSrc);
    }
    reqRedraw = true;
  }
  if (pBtn == HIGH && pDown)
    pDown = false;
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP & LOOP
// ═══════════════════════════════════════════════════════════════════
void setup()
{
  Serial.begin(115200);
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_MUTE, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), encoderISR, CHANGE);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, ADDR_OLED);

  if (showBootAnim)
  {
    for (int rep = 0; rep < 2; rep++)
    {
      for (int f = 0; f < 10; f++)
      {
        display.clearDisplay();
        display.drawBitmap(48, 16, load_frames[f], 32, 32, WHITE);
        display.display();
        delay(100);
      }
    }
  }

  loadSettings();
  bd37033_init();
  bd37033_apply();
  WiFi.mode(WIFI_STA);

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  if (SD.begin(PIN_SD_CS, SPI, 16000000))
    sdMounted = true;

  setupWebRoutes();

  int initSrc = (bootSourceSetting == BOOT_SRC_LAST) ? lastActiveSrc : bootSourceSetting;
  if (initSrc == SRC_SETTINGS || initSrc < 0 || initSrc >= SRC_COUNT)
    initSrc = SRC_SD;
  menuIndex = initSrc;
  switchSource(initSrc);

  if (initSrc != SRC_BT)
  {
    prefs.begin("sarvs", true);
    String savedSSID = prefs.getString("ssid", "");
    String savedPWD = prefs.getString("pwd", "");
    prefs.end();

    if (savedSSID.length() > 0)
    {
      WiFi.mode(WIFI_STA);
      WiFi.begin(savedSSID.c_str(), savedPWD.c_str());
      int tries = 0;
      while (WiFi.status() != WL_CONNECTED && tries < 40)
      {
        delay(250);
        tries++;
      }
      if (WiFi.status() == WL_CONNECTED)
      {
        currentIP = WiFi.localIP().toString();
        configTime(19800, 0, "pool.ntp.org");
        server.begin();
        webActive = true;
      }
      else
      {
        currentIP = "Failed";
      }
    }
  }
  lastInteraction = millis();
}

void loop()
{
  if (webActive)
    server.handleClient();
  handleInputs();

  if (volDebounceTimer > 0 && millis() - volDebounceTimer > 150)
  {
    setVolumeFast();
    volDebounceTimer = 0;
  }

  if (pendingUnmuteTimer > 0 && millis() - pendingUnmuteTimer > unmuteDelay)
  {
    pendingUnmuteTimer = 0; // Fix applied here! Cleared before unmuting.
    setVolumeFast();
  }

  if (!isStandby && millis() - lastInteraction > 10000)
  {
    if (oledPage != PG_PLAYING && oledPage != PG_CAROUSEL && !dspEdit)
    {
      oledPage = PG_PLAYING;
      reqRedraw = true;
    }
  }

  if (!isStandby)
  {
    if (currentSrc == SRC_SD || currentSrc == SRC_RADIO)
      audio.loop();
    if (audioEOF && currentSrc == SRC_SD)
    {
      audioEOF = false;
      nextTrack(false);
    }

    if (reqRedraw)
    {
      reqRedraw = false;
      redrawOled();
    }

    if (muteOverlayTimer > 0)
    {
      if (millis() - muteOverlayTimer < 1200)
      {
        display.fillRect(34, 22, 60, 20, BLACK);
        display.drawRect(34, 22, 60, 20, WHITE);
        display.setCursor(isMuted ? 48 : 40, 28);
        display.print(isMuted ? "MUTED" : "UNMUTED");
        display.display();
      }
      else
      {
        muteOverlayTimer = 0;
        reqRedraw = true;
      }
    }

    static unsigned long lastUpdate = 0;
    if (oledPage == PG_PLAYING && muteOverlayTimer == 0 && millis() - lastUpdate > 250)
    {
      lastUpdate = millis();
      bool nd = false;
      if (textWidth > SCREEN_W)
      {
        scrollX -= 6;
        if (scrollX < -textWidth)
          scrollX = SCREEN_W;
        nd = true;
      }
      if (currentSrc == SRC_SD && (lastUpdate % 1000 < 250))
        nd = true;
      if (nd)
        redrawOled();
    }
  }
  else
  {
    static unsigned long lastClk = 0;
    if (millis() - lastClk > 60000)
    {
      redrawOled();
      lastClk = millis();
    }
  }
}

void setupWebRoutes()
{
  server.on("/", HTTP_GET, []()
            { 
    server.sendHeader("Connection", "close");
    server.send_P(200, "text/html", WEB_HTML); });

  server.on("/api/wifiscan", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    server.send(200, "application/json", json); });

  server.on("/api/get", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    String j = "{ \"vol\":" + String(masterVolume) + ",\"bass\":" + String(bassGain) + ",\"mid\":" + String(midGain) + ",\"treb\":" + String(trebGain) + ",\"sub\":" + String(subVolume);
    j += ",\"bq\":" + String(bassQ) + ",\"sf\":" + String(subFreq);
    j += ",\"src\":" + String(currentSrc) + ",\"play\":" + String(isPlaying ? "true" : "false") + ",\"ip\":\"" + currentIP + "\"";
    j += ",\"title\":\"" + trackTitle + "\",\"artist\":\"" + trackArtist + "\"";
    j += ",\"rNames\":[\"" + stationNames[0] + "\",\"" + stationNames[1] + "\",\"" + stationNames[2] + "\",\"" + stationNames[3] + "\",\"" + stationNames[4] + "\"]";
    j += ",\"rUrls\":[\"" + stationUrls[0] + "\",\"" + stationUrls[1] + "\",\"" + stationUrls[2] + "\",\"" + stationUrls[3] + "\",\"" + stationUrls[4] + "\"]";
    j += ",\"fmP\":[" + String(fmPresets[0],1) + "," + String(fmPresets[1],1) + "," + String(fmPresets[2],1) + "] }";
    server.send(200, "application/json", j); });

  server.on("/api/set", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    if (server.hasArg("vol")) { masterVolume = server.arg("vol").toInt(); setVolumeFast(); saveSettings(); }
    if (server.hasArg("src")) { switchSource(server.arg("src").toInt()); }
    if (server.hasArg("bass")) { bassGain = server.arg("bass").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("mid")) { midGain = server.arg("mid").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("treb")) { trebGain = server.arg("treb").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("sub")) { subVolume = server.arg("sub").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("bq")) { bassQ = server.arg("bq").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("subf")) { subFreq = server.arg("subf").toInt(); bd37033_apply(); saveSettings(); }
    if (server.hasArg("cmd")) {
      String c = server.arg("cmd");
      if(c=="play") doPlayPauseCommand(); else if(c=="next") doNextCommand(); else if(c=="prev") doPrevCommand();
    }
    reqRedraw = true; server.send(200, "text/plain", "OK"); });

  server.on("/api/radio", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    if (server.hasArg("idx") && server.hasArg("name") && server.hasArg("url")) {
      int i = server.arg("idx").toInt(); stationNames[i] = server.arg("name"); stationUrls[i] = server.arg("url"); saveSettings();
    }
    server.send(200, "text/plain", "OK"); });

  server.on("/api/fm", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    if (server.hasArg("f1") && server.hasArg("f2") && server.hasArg("f3")) {
      fmPresets[0] = server.arg("f1").toFloat(); fmPresets[1] = server.arg("f2").toFloat(); fmPresets[2] = server.arg("f3").toFloat(); saveSettings();
    }
    server.send(200, "text/plain", "OK"); });

  server.on("/api/wifi", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    if (server.hasArg("ssid")) {
      prefs.begin("sarvs", false);
      prefs.putString("ssid", server.arg("ssid")); 
      prefs.putString("pwd", server.hasArg("pwd") ? server.arg("pwd") : "");
      prefs.end();
      server.send(200, "text/plain", "OK");
      delay(500); ESP.restart();
    } else { server.send(400, "text/plain", "Missing args"); } });

  server.on("/api/sys", HTTP_GET, []()
            {
    server.sendHeader("Connection", "close");
    if(server.arg("cmd")=="ntp") configTime(19800, 0, "pool.ntp.org");
    if(server.arg("cmd")=="reboot") ESP.restart();
    server.send(200,"text/plain","OK"); });
}