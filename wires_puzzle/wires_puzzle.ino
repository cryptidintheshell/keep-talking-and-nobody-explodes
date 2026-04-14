#include <Adafruit_NeoPixel.h>
#include "connectionHandler.h"

Adafruit_NeoPixel pixels(8, D8, NEO_GRB + NEO_KHZ800);

int pins[] = {D0, D1, D2, D3, D4, D5};

// last digit check if even or odd
int lastDigit = -1;

// Color: 0 = Unused, 1 = Grn, 2 = Red, 3 = Yel, 4 = Blu, 5 = Wht
int wireColors[6][3] = {{0, 0, 0}, {0, 255, 0}, {255, 0, 0}, {255, 255, 0}, {0, 0, 255}, {255, 255, 255}};
String colorDictionary[] = {"Unused", "Green", "Red", "Yellow", "Blue", "White"};
int wires[] = {0, 0, 0, 0, 0, 0};
int tamperedWires[] = {0, 0, 0, 0, 0, 0};
int wireAmount = 0;
int correctWire = -1;
int strikes = 0;

bool isOdd = false;
bool puzzleSolved = false; // Track win state

void setup() {
  Serial.begin(9600);
  pinMode(D0, INPUT_PULLUP);
  pinMode(D1, INPUT_PULLUP);
  pinMode(D2, INPUT_PULLUP);
  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);
  pinMode(D5, INPUT_PULLUP);

  pinMode(D6, OUTPUT); // Buzzer
  pinMode(D7, OUTPUT); // Status LED

  pixels.begin();
  pixels.show();

  // Handshake with main board
  if (connectToMainBoard("asdf", "123456789")) {
    if (sendHandshake("wires")) {
      Serial.println("[+] Handshake successful");
    }
  }

  randomSeed(analogRead(A0) + ESP.getCycleCount());
  
  // Determine serial number logic from main board
  isOdd = checkLastDigit(); 
  
  startNewGame();
}

void loop() {
  if (puzzleSolved) {
    // Keep the system idle after winning
    delay(1000);
    return;
  }

  int wireCut = checkWires();

  if (wireCut != -1) {
    if (wireCut == correctWire) {
      Serial.println("Correct wire cut!");
      buzzerTunes(0);
      puzzleSolved = true;
      
      // --- ADDED COMPLETION LOGIC ---
      Serial.println("***********************");
      Serial.println("* PUZZLE COMPLETE   *");
      Serial.println("***********************");
      
      if (sendFinished("wires")) {
        Serial.println("[+] Main board notified of completion.");
      } else {
        Serial.println("[-] Failed to send completion signal.");
      }
      // ------------------------------
    } else {
      Serial.println("Wrong wire cut! Strike added.");
      buzzerTunes(1);
      sendStrike();
      strikes = getStrikes();
      
      if (strikes >= 3) {
        debugText(3); // Bomb explodes
        while(1) delay(1000); 
      } else {
        startNewGame(); // Restart with new wire config
      }
    }
  }
  delay(100);
}

void startNewGame() {
  wireAmount = random(3, 7); // 3 to 6 wires
  
  // Reset wires
  for (int i = 0; i < 6; i++) {
    wires[i] = 0;
    tamperedWires[i] = 0;
  }

  // Generate random colors for the amount of wires chosen
  for (int i = 0; i < wireAmount; i++) {
    wires[i] = random(1, 6);
  }

  updateLEDs();
  solvePuzzle();
  debugText(0);
  debugText(2);
}

void updateLEDs() {
  pixels.clear();
  for (int i = 0; i < wireAmount; i++) {
    pixels.setPixelColor(i, pixels.Color(wireColors[wires[i]][0], wireColors[wires[i]][1], wireColors[wires[i]][2]));
  }
  pixels.show();
}

int checkWires() {
  for (int i = 0; i < wireAmount; i++) {
    // If pin is HIGH, wire is cut (using INPUT_PULLUP)
    if (digitalRead(pins[i]) == HIGH && tamperedWires[i] == 0) {
      tamperedWires[i] = 1;
      return i;
    }
  }
  return -1;
}

void solvePuzzle() {
  int redCount = 0, blueCount = 0, yellowCount = 0, whiteCount = 0;
  for (int i = 0; i < wireAmount; i++) {
    if (wires[i] == 2) redCount++;
    if (wires[i] == 4) blueCount++;
    if (wires[i] == 3) yellowCount++;
    if (wires[i] == 5) whiteCount++;
  }

  if (wireAmount == 3) {
    if (redCount == 0) correctWire = 1;
    else if (wires[2] == 5) correctWire = 2;
    else if (blueCount > 1) {
        // Find last blue wire
        for(int i = 2; i >= 0; i--) if(wires[i] == 4) { correctWire = i; break; }
    }
    else correctWire = 2;
  } 
  else if (wireAmount == 4) {
    if (redCount > 1 && isOdd) {
        for(int i = 3; i >= 0; i--) if(wires[i] == 2) { correctWire = i; break; }
    }
    else if (wires[3] == 3 && redCount == 0) correctWire = 0;
    else if (blueCount == 1) correctWire = 0;
    else if (yellowCount > 1) correctWire = 3;
    else correctWire = 1;
  }
  else if (wireAmount == 5) {
    if (wires[4] == 1 && isOdd) correctWire = 3;
    else if (redCount == 1 && yellowCount > 1) correctWire = 0;
    else if (blueCount == 0) correctWire = 1;
    else correctWire = 3;
  }
  else if (wireAmount == 6) {
    if (yellowCount == 0 && isOdd) correctWire = 2;
    else if (yellowCount == 1 && whiteCount > 1) correctWire = 3;
    else if (redCount == 0) correctWire = 5;
    else correctWire = 3;
  }
}

void debugText(int type) {
  switch(type) {
    case 0:
      Serial.print("Wires: ");
      for(int i=0; i<wireAmount; i++) {
        Serial.print(colorDictionary[wires[i]] + " ");
      }
      Serial.println();
      break;
    case 2:
      Serial.print("Correct Wire Index: ");
      Serial.println(correctWire);
      break;
    case 3:
      Serial.println("BOOM! Bomb exploded...");
      break;
  }
}

void buzzerTunes(int tunes) {
  switch(tunes) {
    case 0: // Correct
      digitalWrite(D7, HIGH);
      tone(D6, 500, 100); delay(150);
      tone(D6, 1000, 100); delay(150);
      tone(D6, 1250, 475); delay(475);
      digitalWrite(D7, LOW);
      break;
    case 1: // Wrong
      digitalWrite(D7, HIGH);
      tone(D6, 500, 100); delay(150);
      tone(D6, 125, 475); delay(475);
      digitalWrite(D7, LOW);
      break;
  }
}