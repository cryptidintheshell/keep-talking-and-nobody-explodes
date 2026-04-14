#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <vector>
#include <TM1637Display.h>    // For the countdown
#include <Wire.h>             // For I2C
#include <Adafruit_GFX.h>     // For OLED graphics
#include <Adafruit_SSD1306.h> // For OLED display
#include "html.h"

// --- Hardware Pins ---
#define CLK_7SEG 18
#define DIO_7SEG 19
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define BUZZER    5
#define START    4 // D4 pin
#define RESTART  12 // D12 pin

// --- Global Objects ---
const char* ssid = "asdf";
const char* passwd = "123456789";

int strikes = 0;
int gameTime = 900; // 15 minutes in seconds
bool isFailed = false;
bool isStart = false;

bool isDefused = false;

unsigned long lastMillis = 0;
Widgets globalWidget;

TM1637Display segDisplay(CLK_7SEG, DIO_7SEG);
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);
DynamicJsonDocument doc(1024);



std::vector<PuzzleInfo> puzzles{};



// Helper: Update OLED with Widget Data
void updateOLED(Widgets w) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);

    oled.printf("Serial Num: %s\n", w.serialNumber.c_str());
    oled.printf("batteries = AA:%d, D:%d\n", w.batteryAA, w.batteryD);
    oled.printf("IIR: %s\n", w.selectedIIR.c_str());
    oled.printf("Serial: %s\n", w.serialPort.c_str());
    oled.printf("Port: %s\n", w.portSelected.c_str());
    oled.printf("Strikes: %i\n", strikes);
    oled.display();
}

// Helper: Update 7-Segment Timer
void updateTimerDisplay() {
    int mins = gameTime / 60;
    int secs = gameTime % 60;
    segDisplay.showNumberDecEx((mins * 100) + secs, 0x40, true);
}

void setup() {
    Serial.begin(115200);
    pinMode(BUZZER, OUTPUT);
    pinMode(RESTART, INPUT_PULLUP);
    pinMode(START, INPUT_PULLUP);

    // 1. Initialize Displays
    segDisplay.setBrightness(0x0f);
    if(!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED failed"));
    }

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 10);
    oled.println("System Ready.");
    oled.println("\nPress green button");
    oled.println("to start game...");
    oled.display();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, passwd);
    Serial.print("[+] AP IP: ");
    Serial.println(WiFi.softAPIP());

    // 3. Generate Widgets & Populate JSON
    Widgets w; // Constructor handles random generation
    doc["serialNumber"] = w.serialNumber;
    doc["hasVowel"] = w.hasVowel;
    doc["batteryAA"] = w.batteryAA;
    doc["batteryD"] = w.batteryD;
    doc["selectedIIR"] = w.selectedIIR;
    doc["iirIsOn"] = w.iirIsOn;
    doc["serialPort"] = w.serialPort;
    doc["portSelected"] = w.portSelected;
    globalWidget = w;

    // 4. Web Routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", indexPage(strikes, isFailed, puzzles));
    });

    server.on("/handshake", HTTP_POST, [](AsyncWebServerRequest *req){
        if (req->hasParam("ID", false)) {
            String puzzle_id = req->getParam("ID")->value();
            puzzles.push_back(PuzzleInfo(puzzle_id));
            Serial.println("Puzzle: " + String(puzzle_id) + " connected.");
            req->send(200, "text/plain", "OK");
        }
    });

    server.on("/widgets", HTTP_GET, [](AsyncWebServerRequest *req){
        String response;
        serializeJson(doc, response);
        req->send(200, "application/json", response);
    });

    server.on("/strike", HTTP_POST, [](AsyncWebServerRequest *req){
        strikes++;
        req->send(200, "text/plain", "OK");
    });

    server.on("/finished", HTTP_POST, [](AsyncWebServerRequest *req){
        if (req->hasParam("ID", false)) {
            String puzzle_id = req->getParam("ID")->value();
            for (int i = 0; i < puzzles.size(); i++) {
                if (puzzles[i].ID == puzzle_id) {
                    puzzles[i].isFinished = true;
                    Serial.println("[+] Puzzle: " + String(puzzles[i].ID) + " is finished");
                    break; 
                } else Serial.println("[!!] Failed to find puzzle: " + String(puzzles[i].ID));
            }

            req->send(200, "text/plain", "OK");
        }
    });

    server.on("/get_strikes", HTTP_GET, [](AsyncWebServerRequest *req){
        req->send(200, "text/plain", String(strikes));
    });

    updateTimerDisplay();

    server.begin();

    while (digitalRead(START) == HIGH) {
        delay(10);
    }

    isStart = true;
    playBuzzer();
    updateOLED(globalWidget); 
    updateTimerDisplay();
}

int finishedCount = -1;

// ... (Your includes and defines remain the same) ...

void loop() {
    if (isStart && digitalRead(START) == LOW) {
        ESP.restart();
    }

    if (!isStart) return;

    // 2. Check Win Condition
    if (!isFailed && !isDefused && puzzles.size() > 0) {
        int currentFinishedCount = 0;
        for (int i = 0; i < puzzles.size(); i++) {
            if (puzzles[i].isFinished) currentFinishedCount++;
        }

        if (currentFinishedCount == puzzles.size()) {
            isDefused = true;
            // Removed for(;;); so the loop continues to check for the restart button
            oled.clearDisplay();
            oled.setTextSize(2);
            oled.setCursor(0, 10);
            oled.println("DEFUSED");
            oled.display();
            Serial.println("[+] Bomb Defused!");
        }
    }

    // 3. Check Failure Conditions
    if ((strikes >= 3 || gameTime <= 0) && !isFailed && !isDefused) {
        isFailed = true;
        oled.clearDisplay();
        oled.setTextSize(2);
        oled.setCursor(0, 10);
        oled.println("DEFUSE\nFAILED");
        oled.display();
        segDisplay.showNumberDecEx(0, 0x40, true);
        playFailed();
        // Removed for(;;);
    }

    // 4. Main Timer Logic
    if (!isFailed && !isDefused) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastMillis >= 1000) {
            lastMillis = currentMillis;
            if (gameTime > 0) {
                gameTime--;
                playBuzzer();
                updateTimerDisplay();
                // updateOLED only when something changes to save CPU time
            }
        }
    }
}

void playBuzzer() {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
}

void playFailed() {
    for (int i = 0; i != 5; i++) {
        digitalWrite(BUZZER, HIGH);
        delay(1000);
        digitalWrite(BUZZER, LOW);        
    }
}