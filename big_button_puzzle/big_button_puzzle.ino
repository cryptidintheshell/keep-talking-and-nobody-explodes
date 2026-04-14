// ═══════════════════════════════════════════════════════════════
//  KEEP TALKING AND NOBODY EXPLODES — The Button Module
// ═══════════════════════════════════════════════════════════════

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "connectionHandler.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── PINS ─────────────────────────────────────────────────────
#define buttonPin    D3
#define redLED       D4
#define yellowLED    D5
#define blueLED      D6
#define whiteLED     D7
#define buzzer       3

// ── TIMING ───────────────────────────────────────────────────
#define TAP_THRESHOLD     200
#define HOLD_THRESHOLD    500
#define CAROUSEL_INTERVAL 3000

// ── GAME CONSTANTS ───────────────────────────────────────────
const char* buttonLabels[] = { "ABORT", "DETONATE", "HOLD", "PRESS" };
#define LABEL_COUNT 4
const String PUZZLE_ID = "big_button";

// ── GAME VARIABLES ───────────────────────────────────────────
String        buttonColor    = "";
String        buttonLabel    = "";
String        stripColor     = "";
bool          isHolding      = false;
unsigned long pressTime      = 0;
int           timer          = 20; 
bool          holdingStarted = false;
bool          isFinished     = false;

// ── CAROUSEL ─────────────────────────────────────────────────
int           carouselScreen   = 0;
unsigned long lastCarouselSwap = 0;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin,   INPUT_PULLUP);
  pinMode(redLED,      OUTPUT);
  pinMode(yellowLED,   OUTPUT);
  pinMode(blueLED,     OUTPUT);
  pinMode(whiteLED,    OUTPUT);
  pinMode(buzzer,      OUTPUT);

  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();
  
  lcd.print("CONNECTING...");
  
  if (connectToMainBoard("asdf", "123456789")) {
    sendHandshake(PUZZLE_ID);
  }

  randomSeed(analogRead(A0));
  startGame();
}

void loop() {
  // If the puzzle is finished, stop all logic and wait.
  if (isFinished) return;

  int buttonState = digitalRead(buttonPin);

  // Handle Initial Press
  if (buttonState == LOW && !isHolding) {
    pressTime = millis();
    isHolding = true;
  }

  // Handle Release
  if (buttonState == HIGH && isHolding) {
    unsigned long duration = millis() - pressTime;
    isHolding = false;

    if (duration < TAP_THRESHOLD) {
      handleTap();
    } else {
      handleRelease();
    }
  }

  // UI Updates
  if (isHolding) {
    handleHolding();
  } else if (!holdingStarted) {
    handleCarousel();
  }
  yield();
}

// ═══════════════════════════════════════════════════════════════
//  LOGIC ENGINE (Priority Chain)
// ═══════════════════════════════════════════════════════════════

bool shouldTap() {
  JsonDocument doc;
  bool dataValid = getWidgets(doc);
  
  int batteryCount = (int)doc["batteriesAA"] + (int)doc["batteriesD"];
  bool hasLitCAR = false;
  bool hasLitFRK = false;
  
  if (dataValid && doc.containsKey("indicators")) {
    JsonArray indicators = doc["indicators"];
    for (JsonObject ind : indicators) {
      String label = ind["label"] | "";
      bool lit = ind["lit"] | false;
      if (lit && label == "CAR") hasLitCAR = true;
      if (lit && label == "FRK") hasLitFRK = true;
    }
  }

  if (buttonColor == "BLUE" && buttonLabel == "ABORT") return false;
  if (batteryCount > 1 && buttonLabel == "DETONATE") return true;
  if (buttonColor == "WHITE" && hasLitCAR) return false;
  if (batteryCount > 2 && hasLitFRK) return true;
  if (buttonColor == "YELLOW") return false;
  if (buttonColor == "RED" && buttonLabel == "HOLD") return true;

  return false; // Default: Hold
}

bool correctRelease() {
  int targetDigit = 1; 
  if (stripColor == "BLUE")        targetDigit = 4;
  else if (stripColor == "YELLOW") targetDigit = 5;
  else if (stripColor == "WHITE")  targetDigit = 1;

  int secondsOnes = timer % 10;
  return (secondsOnes == targetDigit);
}

// ═══════════════════════════════════════════════════════════════
//  GAMESTATE HELPERS
// ═══════════════════════════════════════════════════════════════

void startGame() {
  clearLEDs();
  holdingStarted = false;
  timer = 20; 
  
  int c = random(0, 4);
  if (c == 0) buttonColor = "RED";
  else if (c == 1) buttonColor = "BLUE";
  else if (c == 2) buttonColor = "YELLOW";
  else buttonColor = "WHITE";
  
  buttonLabel = buttonLabels[random(0, LABEL_COUNT)];
  showColor(buttonColor);
  showCarouselScreen(0);
}

void handleHolding() {
  static unsigned long lastTick = 0;
  
  if (!holdingStarted) {
    if (millis() - pressTime < HOLD_THRESHOLD) return;
    
    if (shouldTap()) {
      handleIncorrectAction("SHOULD TAP!");
      return;
    }

    holdingStarted = true;
    lastTick = millis();
    
    int r = random(0, 3);
    if (r == 0) stripColor = "BLUE";
    else if (r == 1) stripColor = "WHITE";
    else stripColor = "YELLOW";
    
    showColor(stripColor);
  }

  if (millis() - lastTick > 1000) {
    lastTick = millis();
    if (timer > 0) timer--;
    
    lcd.clear();
    lcd.print("STRIP: "); lcd.print(stripColor);
    lcd.setCursor(0, 1);
    lcd.print("TIMER DIGIT: "); lcd.print(timer % 10);
    tone(buzzer, 800, 50);
  }
}

void handleTap() {
  if (shouldTap()) handleSuccess();
  else handleIncorrectAction("SHOULD HOLD!");
}

void handleRelease() {
  if (correctRelease()) handleSuccess();
  else {
    lcd.clear();
    lcd.print("BOOM! WRONG TIME");
    handleIncorrectAction("WRONG RELEASE");
  }
}

void handleSuccess() {
  lcd.clear();
  lcd.print("MODULE SOLVED!");
  tone(buzzer, 1500, 1000);
  
  // Notify the Main Board
  if (sendFinished(PUZZLE_ID)) {
    isFinished = true;
    Serial.println("[+] Server synced. Module deactivated.");
  } else {
    Serial.println("[-] Sync failed. Retrying...");
    delay(2000);
    startGame(); // Optional: allow retry if network failed
  }
}

void handleIncorrectAction(const char* msg) {
  lcd.clear();
  lcd.print("STRIKE!");
  lcd.setCursor(0, 1);
  lcd.print(msg);
  sendStrike(); 
  delay(2000);
  isHolding = false;
  holdingStarted = false;
  startGame();
}

// ── HARDWARE HELPERS ──────────────────────────────────────────

void showColor(String color) {
  clearLEDs();
  if (color == "RED")    digitalWrite(redLED, HIGH);
  else if (color == "BLUE")   digitalWrite(blueLED, HIGH);
  else if (color == "YELLOW") digitalWrite(yellowLED, HIGH);
  else if (color == "WHITE")  digitalWrite(whiteLED, HIGH);
}

void clearLEDs() {
  digitalWrite(redLED, LOW);
  digitalWrite(yellowLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(whiteLED, LOW);
}

void handleCarousel() {
  if (millis() - lastCarouselSwap >= CAROUSEL_INTERVAL) {
    lastCarouselSwap = millis();
    carouselScreen = (carouselScreen + 1) % 2;
    showCarouselScreen(carouselScreen);
  }
}

void showCarouselScreen(int screen) {
  lcd.clear();
  if (screen == 0) {
    lcd.print("BTN: "); lcd.print(buttonColor);
    lcd.setCursor(0, 1);
    lcd.print("LBL: "); lcd.print(buttonLabel);
  } else {
    lcd.print("READY TO DEFUSE");
  }
}