#include <Arduino.h>
#include "Audio.h"
#include "BluetoothA2DPSink.h"
#include "SD.h"
#include "FS.h"
#include <Wire.h>
#include <DisplayImages.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>

// --- PIN DEFINITIONS ---
#define SD_CS 5
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Keypad Layout
#define BTN_PLAY 4
#define BTN_NEXT 32
#define BTN_PREV 15
#define BTN_VOLU 33
#define BTN_VOLD 13

const unsigned char *menuIcons[] = {icon_usb, icon_bt, icon_wifi};
const char *menuItems[] = {"USB AUDIO", "BLUETOOTH", "INTERNET RADIO"};
const int MENU_ITEMS_COUNT = 3;

// Using USB screen for Radio as fallback since Airplay bitmap might be missing
const unsigned char *allScreens[3] = {
    epd_bitmap_screen_USB,
    epd_bitmap_screen_Bluetooth,
    epd_bitmap_screen_Airplay};

// --- MODES ---
enum SystemMode
{
    MODE_MENU,
    MODE_SD,
    MODE_BT,
    MODE_WIFI
};
SystemMode currentMode = MODE_MENU;

// --- SD SUB-MODES ---
enum SDState
{
    SD_BROWSER,
    SD_PLAYING
};
SDState sdState = SD_BROWSER;

struct FileEntry
{
    String name;
    bool isFolder;
};
std::vector<FileEntry> fileList;
String currentPath = "/";
int browserIndex = 0;
int browserTopIndex = 0;

// --- WIFI SUB-MODES ---
enum WiFiSubMode
{
    WIFI_CHECK,
    WIFI_SCAN,
    WIFI_PASS_ENTRY,
    WIFI_CONNECTING,
    WIFI_STATION_LIST,
    WIFI_PLAYING
};
WiFiSubMode wifiState = WIFI_CHECK;

struct RadioStation
{
    const char *name;
    const char *url;
};

RadioStation stations[] = {
    {"Radio Mirchi", "https://radios.crabdance.com:8002/1"},
    {"Radio City", "https://radios.crabdance.com:8002/5"},
    {"Big FM", "https://radios.crabdance.com:8002/4"},
    {"Parithabangal", "https://cast6.my-control-panel.com/proxy/parithabangaltwo/live"},
    {"Thaalam FM", "https://ec4.yesstreaming.net:1990/stream"},
    {"Radio 1", "http://0n-80s.radionetz.de:8000/0n-70s.mp3"}};
const int STATION_COUNT = 6;

// --- PLAYBACK MODES ---
enum PlayMode
{
    PLAY_ALL,
    PLAY_ONE,
    PLAY_SHUFFLE
};
PlayMode currentPlayMode = PLAY_ALL;
String playModeNames[] = {"[ALL]", "[1]", "[SHF]"};

// --- GLOBALS ---
Audio audio;
BluetoothA2DPSink a2dp_sink;
Preferences prefs;
std::vector<String> playlist; // Used for actual playback queue

// UI Variables
int selectedItem = 0;
int topVisibleItem = 0;
int volume = 15;
int currentTrackIndex = 0;
String currentTitle = "";
String currentArtist = "";
int currentEQPreset = 0;
String eqName = "Flat";

// WiFi Globals
String ssidList[10];
int ssidCount = 0;
int selectedSSID = 0;
String targetSSID = "";
String targetPass = "";
int passCursorIndex = 0;
const char *passChars = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{}|;':,./<>?";
int charIndex = 65;
int currentStationIndex = 0;

// BT State
volatile bool btPlaying = false;
bool bt_Started = false;

// UI Scrolling
int scrollX = 0;
int textWidth = 0;

// Seek/Duration
int seekTarget = 0;
bool isSeeking = false;
unsigned long lastSeekUpdate = 0;
int stableDuration = 0;
int lastRawDuration = 0;
int durationStableCount = 0;

#define BOOT_FRAME_DELAY (42)
#define BOOT_FRAME_WIDTH (48)
#define BOOT_FRAME_HEIGHT (48)
#define BOOT_FRAME_COUNT (sizeof(boot_frames) / sizeof(boot_frames[0]))

#define LOAD_FRAME_DELAY (42)
#define LOAD_FRAME_WIDTH (32)
#define LOAD_FRAME_HEIGHT (32)
#define LOAD_FRAME_COUNT (sizeof(load_frames) / sizeof(load_frames[0]))

int boot_frame = 0;
int load_frame = 0;

byte delay_boot_menu = 24 * 2;
byte delay_load_menu = 24 * 1;

// Button Timers & States
unsigned long playBtnTimer = 0;
bool playBtnActive = false;
bool playHandled = false;

unsigned long prevBtnTimer = 0;
bool prevBtnActive = false;
bool prevHandled = false;

unsigned long volUBtnTimer = 0;
bool volUBtnActive = false;
bool volUHandled = false;
unsigned long volDBtnTimer = 0;
bool volDBtnActive = false;
bool volDHandled = false;

unsigned long nextBtnTimer = 0;
bool nextBtnActive = false;
bool nextHandled = false;

// --- PROTOTYPES ---
void loading_Animation();
void showBootAnimation();
void drawMenu();
void loopMenu();
void handleScrolling();
void setupSDMode();
void loopSD();
void setupBTMode();
void loopBT();
void setupWiFiMode();
void loopWiFi();
void stopBTMode();
// SD Browser Prototypes
void openFolder(String path);
void drawSDBrowser();
void playFileFromBrowser(int fileIndex);
void goBackFolder();

void playTrack(int index);
void nextTrack(bool userForced);
void prevTrack();
void setEQ(int preset);
void updateDisplay();
void loadSettings();
void saveSettings();
void saveLastPlayed(String path);
String formatTime(int seconds);

// WiFi Prototypes
void scanNetworks();
void drawWiFiScan();
void drawPassEntry();
void drawStationList();
void drawConnecting(String msg);
void playStation(int index);

// --- CALLBACKS ---
void audio_info(const char *info)
{
    Serial.print("INFO: ");
    Serial.println(info);
    String sInfo = String(info);
    if (sInfo.startsWith("StreamTitle="))
    {
        currentTitle = sInfo.substring(12);
        if (currentTitle.startsWith("'"))
            currentTitle = currentTitle.substring(1, currentTitle.length() - 1);
        textWidth = currentTitle.length() * 6;
        scrollX = 0;
    }
}
void audio_eof_mp3(const char *info) { nextTrack(false); }
void audio_showstreamtitle(const char *info)
{
    String sInfo = String(info);
    currentTitle = sInfo;
    textWidth = currentTitle.length() * 6;
    scrollX = 0;
    updateDisplay();
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text)
{
    String val = (char *)text;
    if (id == ESP_AVRC_MD_ATTR_TITLE)
        currentTitle = val;
    if (id == ESP_AVRC_MD_ATTR_ARTIST)
        currentArtist = val;
    scrollX = 0;
    textWidth = currentTitle.length() * 6;
    updateDisplay();
}

void bt_volume_changed(int vol)
{
    volume = map(vol, 0, 127, 0, 21);
    updateDisplay();
}

void bt_state_changed(esp_a2d_audio_state_t state, void *ptr)
{
    btPlaying = (state == ESP_A2D_AUDIO_STATE_STARTED);
}

// --- SETUP ---
void setup()
{
    Serial.begin(115200);

    // Buttons
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    pinMode(BTN_PREV, INPUT_PULLUP);
    pinMode(BTN_VOLU, INPUT_PULLUP);
    pinMode(BTN_VOLD, INPUT_PULLUP);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 Fail"));
        for (;;)
            ;
    }

    for (byte i = 0; i < delay_boot_menu; i++)
        showBootAnimation();

    display.clearDisplay();
    display.display();

    loadSettings();

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);

    currentMode = MODE_MENU;
    drawMenu();
}

// --- MAIN LOOP ---
void loop()
{
    switch (currentMode)
    {
    case MODE_MENU:
        loopMenu();
        break;
    case MODE_SD:
        loopSD();
        break;
    case MODE_BT:
        loopBT();
        break;
    case MODE_WIFI:
        loopWiFi();
        break;
    }

    static unsigned long lastUpdate = 0;
    // Update Playback Display
    if (currentMode == MODE_SD && sdState == SD_PLAYING && millis() - lastUpdate > 150)
    {
        updateDisplay();
        lastUpdate = millis();
    }
    if (currentMode == MODE_BT && millis() - lastUpdate > 150)
    {
        updateDisplay();
        lastUpdate = millis();
    }
    if (currentMode == MODE_WIFI && wifiState == WIFI_PLAYING && millis() - lastUpdate > 150)
    {
        updateDisplay();
        lastUpdate = millis();
    }
}

// ==========================================
//              UI & MENU SYSTEM
// ==========================================
void loading_Animation()
{
    display.clearDisplay();
    display.drawBitmap(48, 16, load_frames[load_frame], LOAD_FRAME_WIDTH, LOAD_FRAME_HEIGHT, 1);
    display.display();
    load_frame = (load_frame + 1) % LOAD_FRAME_COUNT;
    delay(LOAD_FRAME_DELAY);
}
void showBootAnimation()
{
    display.clearDisplay();
    display.drawBitmap(40, 10, boot_frames[boot_frame], BOOT_FRAME_WIDTH, BOOT_FRAME_HEIGHT, 1);
    display.display();
    boot_frame = (boot_frame + 1) % BOOT_FRAME_COUNT;
    delay(BOOT_FRAME_DELAY);
}

void handleScrolling()
{
    if (selectedItem < topVisibleItem)
    {
        topVisibleItem = selectedItem;
    }
    else if (selectedItem >= topVisibleItem + 3)
    {
        topVisibleItem = selectedItem - 2;
    }
}

void drawMenu()
{
    display.clearDisplay();
    display.drawBitmap(120, 0, bg_scrollbar, 8, 64, WHITE);
    int scrollY = map(selectedItem, 0, 5, 0, 56);
    display.fillRect(125, scrollY, 3, 8, WHITE);

    for (int i = 0; i < 3; i++)
    {
        int index = topVisibleItem + i;

        if (index < MENU_ITEMS_COUNT)
        {
            int yPos = i * 21;

            if (index == selectedItem)
            {
                display.drawBitmap(0, yPos, bg_item_sel, 128, 21, WHITE);
                display.drawBitmap(4, yPos + 2, menuIcons[index], 18, 16, WHITE);
                display.setTextColor(WHITE);
            }
            else
            {
                display.drawBitmap(4, yPos + 2, menuIcons[index], 18, 16, WHITE);
                display.setTextColor(WHITE);
            }

            display.setTextSize(1);
            display.setCursor(28, yPos + 6);
            display.print(menuItems[index]);
        }
    }
    display.display();
}

void drawScreen()
{
    display.clearDisplay();
    display.drawBitmap(0, 0, allScreens[selectedItem], 128, 64, WHITE);
    display.display();
}

void loopMenu()
{
    if (digitalRead(BTN_VOLU) == LOW)
    {
        selectedItem--;
        if (selectedItem < 0)
            selectedItem = MENU_ITEMS_COUNT - 1;
        handleScrolling();
        drawMenu();
        delay(200);
    }
    if (digitalRead(BTN_VOLD) == LOW)
    {
        selectedItem++;
        if (selectedItem >= MENU_ITEMS_COUNT)
            selectedItem = 0;
        handleScrolling();
        drawMenu();
        delay(200);
    }

    if (digitalRead(BTN_PLAY) == LOW)
    {
        for (byte i = 0; i < delay_load_menu; i++)
            loading_Animation();
        drawScreen();
        delay(750);

        if (selectedItem == 0)
            setupSDMode();
        else if (selectedItem == 1)
            setupBTMode();
        else if (selectedItem == 2)
            setupWiFiMode();
    }
}

// ==========================================
//           SD CARD FILE EXPLORER
// ==========================================
void setupSDMode()
{
    if (!SD.begin(SD_CS, SPI, 16000000))
    {
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("No SD Card!");
        display.display();
        delay(2000);
        currentMode = MODE_MENU;
        drawMenu();
        return;
    }

    currentMode = MODE_SD;

    // Check for Last Played Song
    prefs.begin("player", true);
    String lastPath = prefs.getString("lastPath", "");
    prefs.end();

    if (lastPath.length() > 1 && SD.exists(lastPath))
    {
        // Resume Mode
        // 1. Extract Folder
        int lastSlash = lastPath.lastIndexOf('/');
        String folder = "/";
        if (lastSlash > 0)
            folder = lastPath.substring(0, lastSlash);

        currentPath = folder;
        openFolder(currentPath); // Populates fileList

        // 2. Populate Playlist from Folder
        playlist.clear();
        int playIndex = 0;
        int counter = 0;
        String filename = lastPath.substring(lastSlash + 1);

        for (int i = 0; i < fileList.size(); i++)
        {
            // Filter audio only for playlist
            if (!fileList[i].isFolder && fileList[i].name != "[ .. BACK ]")
            {
                playlist.push_back(fileList[i].name);
                if (fileList[i].name == filename)
                {
                    playIndex = counter;
                    browserIndex = i; // Sync browser cursor to playing file
                }
                counter++;
            }
        }

        // 3. Start Playing
        sdState = SD_PLAYING;
        playTrack(playIndex);
    }
    else
    {
        // Default: Start in Browser Mode at Root
        sdState = SD_BROWSER;
        currentPath = "/";
        openFolder(currentPath);
        drawSDBrowser();
    }
}

void openFolder(String path)
{
    File root = SD.open(path);
    if (!root || !root.isDirectory())
    {
        return;
    }

    fileList.clear();

    // Add Back Button if not root
    if (path != "/")
    {
        FileEntry back;
        back.name = "[ .. BACK ]";
        back.isFolder = true;
        fileList.push_back(back);
    }

    File file = root.openNextFile();
    while (file)
    {
        String fileName = String(file.name());
        if (!fileName.startsWith("."))
        { // Skip hidden files
            bool isDir = file.isDirectory();
            if (isDir || fileName.endsWith(".mp3") || fileName.endsWith(".wav") || fileName.endsWith(".flac"))
            {
                FileEntry e;
                e.name = fileName;
                e.isFolder = isDir;
                fileList.push_back(e);
            }
        }
        file = root.openNextFile();
    }
    root.close();

    browserIndex = 0;
    browserTopIndex = 0;
}

void drawSDBrowser()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    String shortPath = currentPath;
    if (shortPath.length() > 20)
        shortPath = ".." + shortPath.substring(shortPath.length() - 18);
    display.print(shortPath);
    display.drawLine(0, 9, 128, 9, WHITE);

    int viewSize = 3;

    if (browserIndex < browserTopIndex)
        browserTopIndex = browserIndex;
    if (browserIndex >= browserTopIndex + viewSize)
        browserTopIndex = browserIndex - viewSize + 1;

    for (int i = 0; i < viewSize; i++)
    {
        int idx = browserTopIndex + i;
        if (idx < fileList.size())
        {
            int y = 14 + (i * 16);
            if (idx == browserIndex)
            {
                display.fillRect(0, y, 128, 14, WHITE);
                display.setTextColor(BLACK, WHITE);
            }
            else
            {
                display.setTextColor(WHITE, BLACK);
            }
            display.setCursor(4, y + 3);

            if (fileList[idx].name == "[ .. BACK ]")
            {
                display.print(fileList[idx].name);
            }
            else if (fileList[idx].isFolder)
            {
                display.print("[");
                display.print(fileList[idx].name);
                display.print("]");
            }
            else
            {
                display.print(fileList[idx].name);
            }
        }
    }
    display.display();
    display.setTextColor(WHITE);
}

void goBackFolder()
{
    if (currentPath == "/")
    {
        // Already at root
    }
    else
    {
        int lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash > 0)
        {
            currentPath = currentPath.substring(0, lastSlash);
        }
        else
        {
            currentPath = "/";
        }
        openFolder(currentPath);
        drawSDBrowser();
    }
}

void playFileFromBrowser(int index)
{
    if (index < 0 || index >= fileList.size())
        return;

    // Check for Back
    if (fileList[index].name == "[ .. BACK ]")
    {
        goBackFolder();
        return;
    }

    if (fileList[index].isFolder)
    {
        // Open Folder
        String newPath = currentPath;
        if (!newPath.endsWith("/"))
            newPath += "/";
        newPath += fileList[index].name;
        currentPath = newPath;
        openFolder(currentPath);
        drawSDBrowser();
    }
    else
    {
        // Play File - Build Contextual Playlist
        playlist.clear();
        int newTrackIndex = 0;
        int counter = 0;

        for (int i = 0; i < fileList.size(); i++)
        {
            if (!fileList[i].isFolder)
            {
                playlist.push_back(fileList[i].name);
                if (i == index)
                    newTrackIndex = counter;
                counter++;
            }
        }

        sdState = SD_PLAYING;
        playTrack(newTrackIndex);
    }
}

void loopSD()
{
    // === BROWSER MODE ===
    if (sdState == SD_BROWSER)
    {
        // Scroll Up/Down
        if (digitalRead(BTN_VOLD) == LOW)
        {
            browserIndex++;
            if (browserIndex >= fileList.size())
                browserIndex = 0;
            drawSDBrowser();
            delay(150);
        }
        if (digitalRead(BTN_VOLU) == LOW)
        {
            browserIndex--;
            if (browserIndex < 0)
                browserIndex = fileList.size() - 1;
            drawSDBrowser();
            delay(150);
        }
        // Select
        if (digitalRead(BTN_PLAY) == LOW)
        {
            if (fileList.size() > 0)
                playFileFromBrowser(browserIndex);
            delay(300);
        }

        // EXIT TO MAIN MENU (Long Press Prev)
        if (digitalRead(BTN_PREV) == LOW)
        {
            if (!prevBtnActive)
            {
                prevBtnActive = true;
                prevBtnTimer = millis();
                prevHandled = false;
            }
            if (!prevHandled && (millis() - prevBtnTimer > 1000))
            {
                // GO TO MAIN MENU
                currentMode = MODE_MENU;
                selectedItem = 0;
                topVisibleItem = 0;
                drawMenu();
                prevHandled = true;
                while (digitalRead(BTN_PREV) == LOW)
                    ; // Wait release
            }
        }
        else
        {
            prevBtnActive = false;
        }
    }

    // === PLAYING MODE ===
    else if (sdState == SD_PLAYING)
    {
        audio.loop();

        // Play/Pause & Exit to Browser
        if (digitalRead(BTN_PLAY) == LOW)
        {
            unsigned long startPress = millis();
            bool longPress = false;
            while (digitalRead(BTN_PLAY) == LOW)
            {
                if (millis() - startPress > 1000)
                {
                    longPress = true;
                    break;
                }
            }

            if (longPress)
            {
                // Exit to Browser
                audio.stopSong();
                sdState = SD_BROWSER;

                // Re-open folder to ensure freshness
                openFolder(currentPath);

                drawSDBrowser();
                while (digitalRead(BTN_PLAY) == LOW)
                    ; // Wait release
            }
            else
            {
                // Short Press - Pause/Resume
                audio.pauseResume();
            }
        }

        // NEXT / SEEK
        if (digitalRead(BTN_NEXT) == LOW)
        {
            if (!nextBtnActive)
            {
                nextBtnActive = true;
                nextBtnTimer = millis();
                nextHandled = false;
                seekTarget = audio.getAudioCurrentTime();
            }
            if (millis() - nextBtnTimer > 500)
            {
                isSeeking = true;
                nextHandled = true;
                if (millis() - lastSeekUpdate > 100)
                {
                    seekTarget += 5;
                    if (seekTarget > audio.getAudioFileDuration())
                        seekTarget = audio.getAudioFileDuration();
                    updateDisplay();
                    lastSeekUpdate = millis();
                }
            }
        }
        else
        {
            if (nextBtnActive)
            {
                if (isSeeking)
                {
                    audio.setAudioPlayPosition(seekTarget);
                    isSeeking = false;
                }
                else if (!nextHandled)
                    nextTrack(true);
                nextBtnActive = false;
            }
        }

        // PREV / SEEK
        if (digitalRead(BTN_PREV) == LOW)
        {
            if (!prevBtnActive)
            {
                prevBtnActive = true;
                prevBtnTimer = millis();
                prevHandled = false;
                seekTarget = audio.getAudioCurrentTime();
            }
            if (millis() - prevBtnTimer > 500)
            {
                isSeeking = true;
                prevHandled = true;
                if (millis() - lastSeekUpdate > 100)
                {
                    seekTarget -= 5;
                    if (seekTarget < 0)
                        seekTarget = 0;
                    updateDisplay();
                    lastSeekUpdate = millis();
                }
            }
        }
        else
        {
            if (prevBtnActive)
            {
                if (isSeeking)
                {
                    audio.setAudioPlayPosition(seekTarget);
                    isSeeking = false;
                }
                else if (!prevHandled)
                    prevTrack();
                prevBtnActive = false;
            }
        }

        // Volume & EQ

        if (digitalRead(BTN_VOLU) == LOW)
        {
            if (!volUBtnActive)
            {
                volUBtnActive = true;
                volUBtnTimer = millis();
                volUHandled = false;
            }
            if (!volUHandled && (millis() - volUBtnTimer > 1000))
            {
                currentEQPreset++;
                if (currentEQPreset > 3)
                    currentEQPreset = 0;
                setEQ(currentEQPreset);
                saveSettings();
                volUHandled = true;
            }
        }
        else
        {
            if (volUBtnActive)
            {
                if (!volUHandled)
                {
                    volume++;
                    if (volume > 21)
                        volume = 21;
                    audio.setVolume(volume);
                    saveSettings();
                    delay(50);
                }
                volUBtnActive = false;
            }
        }
        // VOL DOWN / MODE
        if (digitalRead(BTN_VOLD) == LOW)
        {
            if (!volDBtnActive)
            {
                volDBtnActive = true;
                volDBtnTimer = millis();
                volDHandled = false;
            }
            if (!volDHandled && (millis() - volDBtnTimer > 1000))
            {
                int pm = (int)currentPlayMode;
                pm++;
                if (pm > 2)
                    pm = 0;
                currentPlayMode = (PlayMode)pm;
                saveSettings();
                volDHandled = true;
            }
        }
        else
        {
            if (volDBtnActive)
            {
                if (!volDHandled)
                {
                    volume--;
                    if (volume < 0)
                        volume = 0;
                    audio.setVolume(volume);
                    saveSettings();
                    delay(50);
                }
                volDBtnActive = false;
            }
        }
    }
}

// ==========================================
//              BLUETOOTH MODE
// ==========================================
void setupBTMode()
{
    currentMode = MODE_BT;
    bt_Started = true;
    i2s_pin_config_t my_pin_config = {.bck_io_num = I2S_BCLK, .ws_io_num = I2S_LRC, .data_out_num = I2S_DOUT, .data_in_num = I2S_PIN_NO_CHANGE};
    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_on_volumechange(bt_volume_changed);
    a2dp_sink.set_on_audio_state_changed(bt_state_changed);
    a2dp_sink.start("Sarvs_HiFi_Player");
    a2dp_sink.set_volume(map(volume, 0, 21, 0, 127));
    currentTitle = "Waiting for";
    currentArtist = "Connection...";
}

void loopBT()
{
    if (digitalRead(BTN_PLAY) == LOW)
    {
        if (!playBtnActive)
        {
            playBtnActive = true;
            playBtnTimer = millis();
            playHandled = false;
        }
        if (!playHandled && (millis() - playBtnTimer > 2000))
        {
            stopBTMode();
            currentMode = MODE_MENU;
            drawMenu();
            playHandled = true;
            while (digitalRead(BTN_PLAY) == LOW)
                delay(10);
        }
    }
    else
    {
        if (playBtnActive)
        {
            if (!playHandled)
            {
                if (btPlaying)
                    a2dp_sink.pause();
                else
                    a2dp_sink.play();
                delay(200);
            }
            playBtnActive = false;
        }
    }
    if (digitalRead(BTN_NEXT) == LOW)
    {
        a2dp_sink.next();
        delay(300);
    }
    if (digitalRead(BTN_PREV) == LOW)
    {
        a2dp_sink.previous();
        delay(300);
    }
    if (digitalRead(BTN_VOLU) == LOW)
    {
        volume++;
        if (volume > 21)
            volume = 21;
        a2dp_sink.set_volume(map(volume, 0, 21, 0, 127));
        saveSettings();
        delay(100);
    }
    if (digitalRead(BTN_VOLD) == LOW)
    {
        volume--;
        if (volume < 0)
            volume = 0;
        a2dp_sink.set_volume(map(volume, 0, 21, 0, 127));
        saveSettings();
        delay(100);
    }
}

void stopBTMode()
{
    if (bt_Started)
    {
        a2dp_sink.stop();
        a2dp_sink.end();
        bt_Started = false;
    }
}

// ==========================================
//              INTERNET RADIO MODE
// ==========================================
void setupWiFiMode()
{
    currentMode = MODE_WIFI;
    prefs.begin("player", true);
    targetSSID = prefs.getString("ssid", "");
    targetPass = prefs.getString("pass", "");
    prefs.end();

    if (targetSSID.length() > 0)
    {
        wifiState = WIFI_CONNECTING;
        drawConnecting("Connecting to\n" + targetSSID);
        WiFi.disconnect();
        WiFi.begin(targetSSID.c_str(), targetPass.c_str());

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
        {
            loading_Animation();
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            wifiState = WIFI_STATION_LIST;
            topVisibleItem = 0;
            selectedItem = 0;
            drawStationList();
        }
        else
        {
            drawConnecting("Connect Fail!");
            delay(1000);
            wifiState = WIFI_SCAN;
            scanNetworks();
        }
    }
    else
    {
        wifiState = WIFI_SCAN;
        scanNetworks();
    }
}

void scanNetworks()
{
    drawConnecting("Scanning...");
    int n = WiFi.scanNetworks();
    ssidCount = n;
    if (ssidCount > 10)
        ssidCount = 10;
    for (int i = 0; i < ssidCount; ++i)
    {
        ssidList[i] = WiFi.SSID(i);
    }
    selectedSSID = 0;
    drawWiFiScan();
}

void drawWiFiScan()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Select WiFi:");
    display.drawLine(0, 10, 128, 10, WHITE);

    int startY = 15;
    for (int i = 0; i < 3; i++)
    {
        int idx = selectedSSID + i;
        if (idx < ssidCount)
        {
            if (i == 0)
                display.setCursor(0, startY + (i * 15));
            else
                display.setCursor(5, startY + (i * 15));

            if (i == 0)
                display.print("> ");
            display.println(ssidList[idx].substring(0, 18));
        }
    }
    display.display();
}

void drawPassEntry()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Pass for: ");
    display.println(targetSSID.substring(0, 10));
    display.drawRect(0, 20, 128, 15, WHITE);
    display.setCursor(2, 24);
    String showPass = targetPass;
    if (targetPass.length() > 18)
        showPass = ".." + targetPass.substring(targetPass.length() - 18);
    display.print(showPass);
    display.print("_");
    display.setCursor(50, 45);
    display.setTextSize(2);
    display.print(passChars[charIndex]);
    display.setTextSize(1);
    display.display();
}

void drawStationList()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(25, 2);
    display.print("STATION LIST");
    display.drawLine(0, 12, 128, 12, WHITE);
    int viewSize = 3;
    if (selectedItem < topVisibleItem)
        topVisibleItem = selectedItem;
    if (selectedItem >= topVisibleItem + viewSize)
        topVisibleItem = selectedItem - viewSize + 1;

    for (int i = 0; i < viewSize; i++)
    {
        int idx = topVisibleItem + i;
        int y = 16 + (i * 16);
        if (idx <= STATION_COUNT + 1)
        {
            if (idx == selectedItem)
            {
                display.fillRect(0, y, 128, 14, WHITE);
                display.setTextColor(BLACK, WHITE);
            }
            else
            {
                display.setTextColor(WHITE, BLACK);
            }
            display.setCursor(4, y + 3);
            if (idx < STATION_COUNT)
                display.print(stations[idx].name);
            else if (idx == STATION_COUNT)
                display.print("[ Change WiFi ]");
            else
                display.print("[ Main Menu ]");
        }
    }
    display.display();
    display.setTextColor(WHITE);
}

void drawConnecting(String msg)
{
    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println(msg);
    display.display();
}

void playStation(int index)
{
    currentStationIndex = index;
    currentTitle = "Buffering...";
    currentArtist = stations[index].name;
    if (audio.isRunning())
        audio.stopSong();
    audio.connecttohost(stations[index].url);
    wifiState = WIFI_PLAYING;
    updateDisplay();
}

void loopWiFi()
{
    if (wifiState == WIFI_SCAN)
    {
        if (digitalRead(BTN_VOLD) == LOW)
        {
            selectedSSID++;
            if (selectedSSID >= ssidCount)
                selectedSSID = 0;
            drawWiFiScan();
            delay(200);
        }
        if (digitalRead(BTN_VOLU) == LOW)
        {
            selectedSSID--;
            if (selectedSSID < 0)
                selectedSSID = ssidCount - 1;
            drawWiFiScan();
            delay(200);
        }
        if (digitalRead(BTN_PLAY) == LOW)
        {
            targetSSID = ssidList[selectedSSID];
            targetPass = "";
            charIndex = 36;
            wifiState = WIFI_PASS_ENTRY;
            drawPassEntry();
            delay(300);
        }
    }
    else if (wifiState == WIFI_PASS_ENTRY)
    {
        if (digitalRead(BTN_VOLU) == LOW)
        {
            charIndex++;
            if (charIndex >= strlen(passChars))
                charIndex = 0;
            drawPassEntry();
            delay(150);
        }
        if (digitalRead(BTN_VOLD) == LOW)
        {
            charIndex--;
            if (charIndex < 0)
                charIndex = strlen(passChars) - 1;
            drawPassEntry();
            delay(150);
        }
        if (digitalRead(BTN_NEXT) == LOW)
        {
            targetPass += passChars[charIndex];
            drawPassEntry();
            delay(250);
        }
        if (digitalRead(BTN_PREV) == LOW)
        {
            if (targetPass.length() > 0)
                targetPass.remove(targetPass.length() - 1);
            drawPassEntry();
            delay(250);
        }
        if (digitalRead(BTN_PLAY) == LOW)
        {
            prefs.begin("player", false);
            prefs.putString("ssid", targetSSID);
            prefs.putString("pass", targetPass);
            prefs.end();
            setupWiFiMode();
            delay(500);
        }
    }
    else if (wifiState == WIFI_STATION_LIST)
    {
        if (digitalRead(BTN_VOLD) == LOW)
        {
            selectedItem++;
            if (selectedItem > STATION_COUNT + 1)
                selectedItem = 0;
            drawStationList();
            delay(200);
        }
        if (digitalRead(BTN_VOLU) == LOW)
        {
            selectedItem--;
            if (selectedItem < 0)
                selectedItem = STATION_COUNT + 1;
            drawStationList();
            delay(200);
        }
        if (digitalRead(BTN_PLAY) == LOW)
        {
            if (selectedItem == STATION_COUNT)
            {
                prefs.begin("player", false);
                prefs.putString("ssid", "");
                prefs.putString("pass", "");
                prefs.end();
                WiFi.disconnect();
                wifiState = WIFI_SCAN;
                scanNetworks();
            }
            else if (selectedItem == STATION_COUNT + 1)
            {
                audio.stopSong();
                currentMode = MODE_MENU;
                selectedItem = 0;
                topVisibleItem = 0;
                drawMenu();
            }
            else
            {
                playStation(selectedItem);
            }
            delay(300);
        }
    }
    else if (wifiState == WIFI_PLAYING)
    {
        audio.loop();
        if (digitalRead(BTN_VOLU) == LOW)
        {
            volume++;
            if (volume > 21)
                volume = 21;
            audio.setVolume(volume);
            delay(50);
        }
        if (digitalRead(BTN_VOLD) == LOW)
        {
            volume--;
            if (volume < 0)
                volume = 0;
            audio.setVolume(volume);
            delay(50);
        }
        if (digitalRead(BTN_PLAY) == LOW)
        {
            unsigned long startPress = millis();
            while (digitalRead(BTN_PLAY) == LOW)
            {
                if (millis() - startPress > 1500)
                {
                    audio.stopSong();
                    wifiState = WIFI_STATION_LIST;
                    selectedItem = 0;
                    drawStationList();
                    while (digitalRead(BTN_PLAY) == LOW)
                        ;
                    return;
                }
            }
            audio.setVolume(0);
        }
    }
}

// ==========================================
//              HELPERS
// ==========================================

void loadSettings()
{
    prefs.begin("player", false);
    volume = prefs.getInt("vol", 15);
    prefs.end();
}

void saveSettings()
{
    prefs.begin("player", false);
    prefs.putInt("vol", volume);
    prefs.end();
}

void saveLastPlayed(String path)
{
    prefs.begin("player", false);
    prefs.putString("lastPath", path);
    prefs.end();
}

void playTrack(int index)
{
    if (index >= 0 && index < playlist.size())
    {
        currentTrackIndex = index;
        // In browser mode, we need full path
        String fullPath = currentPath;
        if (!fullPath.endsWith("/"))
            fullPath += "/";
        fullPath += playlist[index];

        audio.connecttoFS(SD, fullPath.c_str());
        currentTitle = playlist[index];
        // Strip extension for display
        int dotIdx = currentTitle.lastIndexOf('.');
        if (dotIdx > 0)
            currentTitle = currentTitle.substring(0, dotIdx);

        textWidth = currentTitle.length() * 6;
        scrollX = 0;
        lastRawDuration = 0;
        durationStableCount = 0;
        stableDuration = 0;

        saveLastPlayed(fullPath);
    }
}

void nextTrack(bool userForced)
{
    if (currentPlayMode == PLAY_SHUFFLE)
    {
        int newTrack = currentTrackIndex;
        while (newTrack == currentTrackIndex && playlist.size() > 1)
        {
            newTrack = random(0, playlist.size());
        }
        currentTrackIndex = newTrack;
        playTrack(currentTrackIndex);
        return;
    }
    currentTrackIndex++;
    if (currentTrackIndex >= playlist.size())
        currentTrackIndex = 0;
    playTrack(currentTrackIndex);
}

void prevTrack()
{
    currentTrackIndex--;
    if (currentTrackIndex < 0)
        currentTrackIndex = playlist.size() - 1;
    playTrack(currentTrackIndex);
}

void setEQ(int preset)
{
    currentEQPreset = preset;
    switch (preset)
    {
    case 0:
        audio.setTone(0, 0, 0);
        eqName = "Flat";
        break;
    case 1:
        audio.setTone(6, 0, 0);
        eqName = "Bass";
        break;
    case 2:
        audio.setTone(4, -2, 4);
        eqName = "Rock";
        break;
    case 3:
        audio.setTone(-4, 4, -2);
        eqName = "Voc";
        break;
    }
}

void updateDisplay()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextWrap(false);

    // HEADER
    display.setCursor(0, 0);
    display.print("V:");
    display.print(volume);

    if (currentMode == MODE_SD)
    {
        display.setCursor(30, 0);
        display.print("[");
        display.print(eqName);
        display.print("]");
        display.setCursor(80, 0);
        display.print(playModeNames[currentPlayMode]);
    }
    else if (currentMode == MODE_WIFI)
    {
        display.setCursor(90, 0);
        display.print("[WEB]");
    }
    else
    {
        display.setCursor(90, 0);
        display.print("[BT]");
    }
    display.drawLine(0, 9, 128, 9, WHITE);

    // TITLE SCROLL
    if (textWidth > SCREEN_WIDTH)
    {
        display.setCursor(scrollX, 15);
        display.print(currentTitle);
        scrollX -= 3;
        if (scrollX < -textWidth)
            scrollX = SCREEN_WIDTH;
    }
    else
    {
        display.setCursor(0, 15);
        display.print(currentTitle);
    }

    // BODY
    display.setCursor(0, 28);
    if (currentMode == MODE_SD)
    {
        int cur = isSeeking ? seekTarget : audio.getAudioCurrentTime();
        int rawTotal = audio.getAudioFileDuration();
        if (rawTotal > 0 && rawTotal == lastRawDuration)
            durationStableCount++;
        else
        {
            durationStableCount = 0;
            lastRawDuration = rawTotal;
        }
        if (durationStableCount > 10)
            stableDuration = rawTotal;

        display.print(formatTime(cur));
        display.print("/");
        if (stableDuration > 0)
            display.print(formatTime(stableDuration));
        else
            display.print("--:--");

        display.drawRect(0, 40, 128, 6, WHITE);
        if (stableDuration > 0)
        {
            int w = (cur / (float)stableDuration) * 128;
            if (w > 128)
                w = 128;
            display.fillRect(0, 40, w, 6, WHITE);
        }
        display.setCursor(0, 52);
        if (isSeeking)
            display.print("Seeking...");
        else if (audio.isRunning())
            display.print("Playing");
        else
            display.print("Paused");
    }
    else if (currentMode == MODE_WIFI)
    {
        display.println(currentArtist.substring(0, 20));
        display.setCursor(0, 45);
        if (audio.isRunning())
            display.print("LIVE STREAM");
        else
            display.print("BUFFERING...");
    }
    else
    { // BT
        display.println(currentArtist.substring(0, 20));
        display.setCursor(0, 45);
        if (btPlaying)
            display.print("PLAYING");
        else
            display.print("PAUSED");
    }
    display.display();
}

String formatTime(int seconds)
{
    int m = seconds / 60;
    int s = seconds % 60;
    char buf[10];
    sprintf(buf, "%02d:%02d", m, s);
    return String(buf);
}