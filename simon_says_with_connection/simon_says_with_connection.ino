#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "connectionHandler.h"

// --- Hardware Mapping ---
enum Color { YELLOW = 0, BLUE, WHITE, RED }; 
const int LED_PINS[] = {14, 12, 13, 15};  // Y, B, W, R
const int BTN_PINS[] = {0,  2,  3,  1};   // Y, B, W, R
const int BUZZER_PIN = 16;
const String PUZZLE_ID = "simon_says";

// Mapping based on Enum: { YELLOW=0, BLUE=1, WHITE=2, RED=3 }
const Color SIMON_TABLE[2][3][4] = {
    // [0] NO VOWEL
    {
        { RED,    YELLOW, WHITE,  BLUE   }, // 0 Strikes
        { WHITE,  BLUE,   YELLOW, RED    }, // 1 Strike 
        { RED,    WHITE,  BLUE,   YELLOW }  // 2 Strikes
    },
    // [1] HAS VOWEL
    {
        { WHITE,  RED,    YELLOW, BLUE   }, // 0 Strikes
        { RED,    WHITE,  BLUE,   YELLOW }, // 1 Strike 
        { BLUE,   RED,    WHITE,  YELLOW }  // 2 Strikes
    }
};

// --- Global State ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
int sequence[10]; // Increased buffer for safety
int stageCount;   // Total steps required to win
int level;        // Current step the player is on (0 to stageCount-1)
int strikes = 0;
bool serialHasVowel = false;
bool moduleDefused  = false;
unsigned long lastDebounce[4] = {0, 0, 0, 0};

void setup() {
    Wire.begin(4, 5);
    lcd.init();
    lcd.backlight();
    pinMode(BUZZER_PIN, OUTPUT);

    for (int i = 0; i < 4; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        pinMode(BTN_PINS[i], INPUT_PULLUP);
    }

    showMessage("Connecting...", "Wait for AP");
    if (connectToMainBoard("asdf", "123456789")) {
        if (sendHandshake(PUZZLE_ID)) {
            serialHasVowel = hasVowel();   
            showMessage("Connected!", serialHasVowel ? "Vowel Mode" : "Standard Mode");
            delay(2000);
        }
    }

    seedRandom();
    attractScreen(); 
    startGame();     
}

void loop() {
    if (moduleDefused) {
        showMessage("DEFUSED!", "Module complete");
        digitalWrite(LED_PINS[WHITE], HIGH);
        delay(1000);
        return;
    }

    // Sync strikes from server
    int currentStrikes = getStrikes();
    if (currentStrikes != -1) strikes = currentStrikes;

    showMessage("Pay", "Attention");
    delay(500);
    playSequence();

    if (getPlayerInput()) {
        handleSuccess();
    } else {
        handleStrike();
    }
}

// --- Logic Implementation ---

void attractScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   Simon Says");
    while (true) {
        if (scrollText("Press White to start", 1, 350)) break; 
        yield();
    }
}

bool scrollText(const char* msg, int row, int delayMs) {
    String s = String(msg) + "                ";
    for (int i = 0; i < s.length() - 16; i++) {
        if (readButton(WHITE)) return true;
        lcd.setCursor(0, row);
        lcd.print(s.substring(i, i + 16));
        for(int d = 0; d < delayMs / 10; d++) {
            if (readButton(WHITE)) return true;
            delay(10);
            yield();
        }
    }
    return false;
}

void startGame() {
    waitForRelease(WHITE);
    buzz(200);
    
    level = 0;
    stageCount = random(3, 6); // Set a random difficulty length
    for (int i = 0; i < stageCount; i++) sequence[i] = random(4);
    
    showMessage("Get ready...", "");
    delay(1000);
}

bool getPlayerInput() {
    int sIdx = min(strikes, 2);
    int tIdx = serialHasVowel ? 1 : 0;

    for (int i = 0; i <= level; i++) {
        Color expected = SIMON_TABLE[tIdx][sIdx][sequence[i]];        

        char buf[17];
        snprintf(buf, 17, "Step %d/%d", i + 1, level + 1);
        showMessage("Your turn!", buf);

        int pressed = -1;
        while (pressed == -1) {
            yield();
            for (int b = 0; b < 4; b++) {
                if (readButton(b)) { pressed = b; break; }
            }
        }

        flashLED(pressed, 250);
        waitForRelease(pressed);
        if (pressed != (int)expected) return false;
    }
    return true;
}

void handleSuccess() {
    if (level + 1 >= stageCount) {
        // Entire sequence complete
        showMessage("SOLVED!", "Syncing...");
        if (sendFinished(PUZZLE_ID)) {
            moduleDefused = true;
            showMessage("DEFUSED", "Module disarmed!");
            winEffect();
        } else {
            // Backup attempt if server was busy
            delay(1000);
            if(sendFinished(PUZZLE_ID)) {
               moduleDefused = true;
               winEffect();
            }
        }
    } else {
        // Stage complete, add next step
        level++;
        showMessage("Correct!", "");
        buzz(100); delay(50); buzz(100);
        delay(800);
    }
}

void handleStrike() {
    sendStrike();
    strikes++;
    showMessage("STRIKE!", "Incorrect color");
    for (int i = 0; i < 3; i++) {
        for(int l=0; l<4; l++) digitalWrite(LED_PINS[l], HIGH);
        buzz(100);
        for(int l=0; l<4; l++) digitalWrite(LED_PINS[l], LOW);
        delay(100);
    }

    if (strikes >= 3) {
        showMessage("FAILED!", "Restarting...");
        buzz(1000);
        delay(2000);
        strikes = 0;
        attractScreen();
        startGame();
    } else {
        delay(1000); // Let them see the strike before playing sequence again
    }
}

// --- Helper Functions ---

bool readButton(int b) {
    if (digitalRead(BTN_PINS[b]) == LOW) {
        if (millis() - lastDebounce[b] > 50) {
            lastDebounce[b] = millis();
            return true;
        }
    }
    return false;
}

void waitForRelease(int b) {
    while (digitalRead(BTN_PINS[b]) == LOW) { yield(); delay(10); }
    delay(50);
}

void flashLED(int idx, int duration) {
    digitalWrite(LED_PINS[idx], HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(LED_PINS[idx], LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

void showMessage(const char* l1, const char* l2) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(l1);
    lcd.setCursor(0, 1); lcd.print(l2);
}

void winEffect() {
    for (int i = 0; i < 12; i++) {
        digitalWrite(LED_PINS[i % 4], HIGH);
        delay(80);
        digitalWrite(LED_PINS[i % 4], LOW);
    }
}

void buzz(int ms) { digitalWrite(BUZZER_PIN, HIGH); delay(ms); digitalWrite(BUZZER_PIN, LOW); }

void playSequence() {
    for (int i = 0; i <= level; i++) {
        flashLED(sequence[i], 400);
        delay(250);
    }
}

void seedRandom() { randomSeed(micros() ^ analogRead(A0)); }