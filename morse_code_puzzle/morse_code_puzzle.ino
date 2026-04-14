#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "connectionHandler.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char *alphabet[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",    
  ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",  
  "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."         
};

char *codes[] = {"shell", "code", "breaker", "secret", "hide", "broken", "lies", "cold", "hills", "lock"};
char *replies[] = {"open", "free", "crack", "found", "seek", "damaged", "truth", "stone", "ground", "forbid"};

const int numOptions = 10;
int codeIndex = 0;
int menuIndex = 0;
int attemptsLeft = 3;

String randomCodeSelected = ""; 
String correctReply = ""; 

bool isCorrectReply = false;
bool isConfirmed = false;
bool menuActive = false;

const int ledLight = 6;
const int navButton = 7;    
const int selectButton = 5; 
const int replayButton = 3; 
const int BUZZER_PIN = 4; 
const int indicatorLED = 18; 

void setup() {
  Serial.begin(115200);
  pinMode(ledLight, OUTPUT);
  pinMode(indicatorLED, OUTPUT);

  pinMode(navButton, INPUT_PULLUP);
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(replayButton, INPUT_PULLUP);
  
  delay(500);
  Wire.begin(8, 9);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;);
  }

  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  bool isConnected = connectToMainBoard("asdf", "123456789");
  if (isConnected) {
  	display.clearDisplay();
  	display.setCursor(0, 0);
  	display.println("Connected to \nmain board");
  	display.display();
  	delay(2000);
   
  	sendHandshake("morse");
    int strikes = getStrikes();

    display.clearDisplay();
    display.println("strikes: " + String(strikes));
    display.display();
    delay(2000);

    if (strikes > 0)
      attemptsLeft -= getStrikes();
  }

  randomSeed(analogRead(0));  
  codeIndex = random(0, numOptions); 
  randomCodeSelected = codes[codeIndex];
  correctReply = replies[codeIndex];

  showStartScreen();
}

void loop() {
  if (isCorrectReply) {
    displayEnding("Mission\nSuccess!");
    sendFinished("morse");
    for (;;);
  } else if (attemptsLeft < 1) {
    displayEnding("Connection\nTerminated");
    playFailed();
    for (;;);
  }

  if (digitalRead(replayButton) == LOW) {
    delay(200);
    blinkSecretCode(randomCodeSelected);
    if (menuActive) updateMenuDisplay(); 
    else showStartScreen();
    while(digitalRead(replayButton) == LOW);
  }

  if (digitalRead(navButton) == LOW) {
    delay(200);
    if (!menuActive) {
      menuActive = true;
      menuIndex = 0;
    } else {
      menuIndex = (menuIndex + 1) % numOptions;
    }
    isConfirmed = false; 
    updateMenuDisplay();
    while(digitalRead(navButton) == LOW);
  }

  if (digitalRead(selectButton) == LOW && menuActive) {
    delay(200);
    if (!isConfirmed) {
      isConfirmed = true;
      updateMenuDisplay();
    } else {
      processReply(replies[menuIndex]);
      isConfirmed = false;
      if (!isCorrectReply) updateMenuDisplay();
    }
    while(digitalRead(selectButton) == LOW);
  }
}

void playCode(int letterIndex) {
  if (letterIndex < 0 || letterIndex >= 26) return;
  String sequence = alphabet[letterIndex];
  for (int i = 0; i < sequence.length(); i++) {
    digitalWrite(ledLight, HIGH);
    delay(sequence[i] == '.' ? 150 : 400);
    digitalWrite(ledLight, LOW);
    delay(250);
  }
}

void blinkSecretCode(String word) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("LISTENING...");
  display.display();

  for (int i = 0; i < word.length(); i++) {
    int letterIdx = word[i] - 'a';
    playCode(letterIdx);
    delay(600); 
  }
}

void showStartScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("Press Menu to");
  display.println("begin link...");
  display.display();
}

void updateMenuDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Reply (attempts: ");
  display.print(attemptsLeft);
  display.print(")");

  display.setCursor(0, 15);
  display.setTextSize(2);
  display.print(isConfirmed ? "> " : "  ");
  display.print(replies[menuIndex]);
  display.display();
}

void processReply(String selected) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Transmitting...");
  display.display();

  for (int i = 0; i < selected.length(); i++) {
    int letterIdx = selected[i] - 'a';
    playCode(letterIdx);
    delay(400); 
  }

  if (selected == correctReply) {
    playCorrect();
    isCorrectReply = true;

  } else {
    playWrong();
    sendStrike(); // send strike to main board
    attemptsLeft = 3 - getStrikes();

    display.clearDisplay();
    display.setCursor(0, 5);
    display.setTextSize(1);
    display.println("From [??]: \nHey, you sent the \nwrong code");
    display.display();
    delay(4000);
  }
}

void displayEnding(String msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void playCorrect() {
  tone(BUZZER_PIN, 1000, 100); 
  delay(150);
  tone(BUZZER_PIN, 1500, 200);
}

void playWrong() {
  tone(BUZZER_PIN, 200, 500);
}

void playFailed() {
  for(int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 150, 400);
    delay(500);
  }
  tone(BUZZER_PIN, 100, 1000);
}