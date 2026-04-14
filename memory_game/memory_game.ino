#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#include "bigFont.h" // has LiquidCrystal_I2C.h inside
#include "connectionHandler.h" 

#define i2c_Address 0x3c
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SH1106G oled = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int stageVal[] = {0, 0, 0, 0, 0};
int stageBtns[5][4] = {{1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}}; 
int correctBtns[5][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}; 
int buttons[] = {D5, D4, D3, D0};

int win = false;
int isFailed = false;
int strikes = 0;

void setup() {
  delay(250);
  Serial.begin(9600);

  pinMode(D0, INPUT_PULLUP);
  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);
  pinMode(D5, INPUT_PULLUP);

  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);

  // 16x2 LCD setup
  lcd.init();
  lcd.backlight();
  lcd.clear();
  declareBigFont(); 

  // OLED setup
  oled.begin(i2c_Address, true);
  oled.display();
  delay(2000);
  oled.clearDisplay();
  oled.setRotation(1);
  oled.setTextSize(10);
  oled.setTextColor(SH110X_WHITE);

  if (connectToMainBoard("asdf", "123456789")) {
    if (sendHandshake("memory")) {
      oled.setRotation(0);
      oled.clearDisplay();
      oled.setCursor(0, 0);
      oled.setTextSize(1);

      strikes = getStrikes();
      if (strikes > 2) isFailed = true;

      oled.clearDisplay();
      oled.println("Handshake done\nStrikes: " + String(strikes));
      oled.display();

      delay(3000);
    }
  }

  oled.setTextSize(10);
  oled.clearDisplay();
  oled.setRotation(1);
  oled.display();

  randomSeed(ESP.getCycleCount());
  startNewGame();
}

void loop() {
  if (isFailed || strikes > 2) {
    isFailed = true;
    oled.setRotation(0);
    oled.clearDisplay();
    oled.setCursor(0, 5);
    oled.setTextSize(2);
    oled.println("Defusing\nFailed");
    oled.display();
    for(;;) delay(1000);
  } else if (win) {
    // Keeps the "COMPLETED" screen visible
    for(;;) delay(1000);
  }

  int pos = -1;
  for(int i = 0; i < 5; i++) {
    Serial.print("Round #");
    Serial.print(i + 1);
    Serial.println(", start!");
    updateDisplays(i);
    buzzerTunes(2);

    pos = -1;
    while(pos == -1) {
      if(digitalRead(D5) == 0) pos = 0;
      if(digitalRead(D4) == 0) pos = 1;
      if(digitalRead(D3) == 0) pos = 2;
      if(digitalRead(D0) == 0) pos = 3;
      delay(50);
    }

    if(pos == correctBtns[i][1]) {
      Serial.print("Button #");
      Serial.print(pos + 1);
      Serial.println(" is correct!");
      buzzerTunes(0);
      
      // Check if this was the final stage (Stage 5)
      if(i == 4) {
        win = true;
        
        // --- COMPLETION FEEDBACK ---
        oled.setRotation(0);      // Horizontal mode
        oled.clearDisplay();
        oled.setTextSize(2);
        oled.setCursor(10, 20);   
        oled.println("COMPLETED");
        oled.display();
        
        Serial.println("[!] Puzzle Solved! Sending finished signal...");
        
        if (sendFinished("memory")) {
          Serial.println("[+] Main board acknowledged completion.");
        } else {
          Serial.println("[-] Failed to notify main board.");
        }
        
        break; 
      }
    }
    else {
      Serial.print("Button #");
      Serial.print(pos + 1);
      Serial.println(" is wrong!");
      buzzerTunes(1);
      
      sendStrike();
      strikes = getStrikes();
      
      startNewGame(); 
      break; 
    }

    delay(2000);
  }
}

void updateDisplays(int currentStage) {
  lcd.clear();
  for(int i = 0; i < 4; i++) {
    printNumber(stageBtns[currentStage][i], i * 4);
  }
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print(stageVal[currentStage]);
  oled.display();
}

void startNewGame() {
  lcd.clear();
  oled.clearDisplay();
  generateStageVal();
  generateStageBtns();
  generateCorrectBtns();
}

void generateStageVal() {
  for(int i = 0; i < 5; i++) {
    stageVal[i] = random(1, 5);
  }
}

void generateStageBtns() {
  int rstStageBtns[5][4] = {{1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}, {1, 2, 3, 4}};
  memcpy(stageBtns, rstStageBtns, sizeof(rstStageBtns));

  for(int i = 0; i < 5; i++) {
    for (int j = 3; j > 0; --j) {
      int randNum = random(0, j + 1);
      int tempNum = stageBtns[i][j];
      stageBtns[i][j] = stageBtns[i][randNum];
      stageBtns[i][randNum] = tempNum;
    }
  }
}

void generateCorrectBtns() {
  for(int i = 0; i < 5; i++) {
    switch(i) {
      case 0: // Stage #1
        switch(stageVal[i]) {
          case 1: correctBtns[0][0] = stageBtns[0][1]; correctBtns[0][1] = 1; break;
          case 2: correctBtns[0][0] = stageBtns[0][1]; correctBtns[0][1] = 1; break;
          case 3: correctBtns[0][0] = stageBtns[0][2]; correctBtns[0][1] = 2; break;
          case 4: correctBtns[0][0] = stageBtns[0][3]; correctBtns[0][1] = 3; break;
        }
        break;
      case 1: // Stage #2
        switch(stageVal[i]) {
          case 1: correctBtns[1][0] = 4; correctBtns[1][1] = getBtnPos(4, 1); break;
          case 2: correctBtns[1][0] = stageBtns[1][correctBtns[0][1]]; correctBtns[1][1] = correctBtns[0][1]; break;
          case 3: correctBtns[1][0] = stageBtns[1][0]; correctBtns[1][1] = 0; break;
          case 4: correctBtns[1][0] = stageBtns[1][correctBtns[0][1]]; correctBtns[1][1] = correctBtns[0][1]; break;
        }
        break;
      case 2: // Stage #3
        switch(stageVal[i]) {
          case 1: correctBtns[2][0] = correctBtns[1][0]; correctBtns[2][1] = getBtnPos(correctBtns[1][0], 2); break;
          case 2: correctBtns[2][0] = correctBtns[0][0]; correctBtns[2][1] = getBtnPos(correctBtns[0][0], 2); break;
          case 3: correctBtns[2][0] = stageBtns[2][2]; correctBtns[2][1] = 2; break;
          case 4: correctBtns[2][0] = 4; correctBtns[2][1] = getBtnPos(4, 2); break;
        }
        break;
      case 3: // Stage #4
        switch(stageVal[i]) {
          case 1: correctBtns[3][0] = stageBtns[3][correctBtns[0][1]]; correctBtns[3][1] = correctBtns[0][1]; break;
          case 2: correctBtns[3][0] = stageBtns[3][0]; correctBtns[3][1] = 0; break;
          case 3: correctBtns[3][0] = stageBtns[3][correctBtns[1][1]]; correctBtns[3][1] = correctBtns[1][1]; break;
          case 4: correctBtns[3][0] = stageBtns[3][correctBtns[1][1]]; correctBtns[3][1] = correctBtns[1][1]; break;
        }
        break;
      case 4: // Stage #5
        switch(stageVal[i]) {
          case 1: correctBtns[4][0] = correctBtns[0][0]; correctBtns[4][1] = getBtnPos(correctBtns[0][0], 4); break;
          case 2: correctBtns[4][0] = correctBtns[1][0]; correctBtns[4][1] = getBtnPos(correctBtns[1][0], 4); break;
          case 3: correctBtns[4][0] = correctBtns[3][0]; correctBtns[4][1] = getBtnPos(correctBtns[3][0], 4); break;
          case 4: correctBtns[4][0] = correctBtns[2][0]; correctBtns[4][1] = getBtnPos(correctBtns[2][0], 4); break;
        }
        break;
    }
  }
}

int getBtnPos(int value, int stage) {
  for (int i = 0; i < 4; i++) {
    if (stageBtns[stage][i] == value) return i;
  }
  return 0;
}

void buzzerTunes(int tunes) {
  switch(tunes) {
    case 0: // Correct
      digitalWrite(D7, HIGH); tone(D6, 500, 100); delay(100);
      tone(D6, 1000, 100); delay(100);
      tone(D6, 750, 100); delay(100);
      tone(D6, 1250, 475); delay(475);
      digitalWrite(D7, LOW);
      break;
    case 1: // Wrong
      digitalWrite(D7, HIGH); tone(D6, 500, 100); delay(100);
      tone(D6, 250, 100); delay(100);
      tone(D6, 500, 100); delay(100);
      tone(D6, 125, 475); delay(475);
      digitalWrite(D7, LOW);
      break;
    case 2: // Round Start
      digitalWrite(D7, HIGH); tone(D6, 600, 425); delay(475);
      tone(D6, 600, 50); delay(100);
      tone(D6, 600, 50); delay(100);
      tone(D6, 600, 100); delay(100);
      digitalWrite(D7, LOW);
      break;
  }
}