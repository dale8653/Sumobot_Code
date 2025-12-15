#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BluetoothSerial.h"

// ==============================
// OLED Settings
// ==============================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ==============================
// Bluetooth
// ==============================
BluetoothSerial BT;

// ==============================
// L298N Motor Pins (4 motors total)
// LEFT motors: IN1, IN2
// RIGHT motors: IN3, IN4
// ==============================
#define IN1 25
#define IN2 33
#define IN3 18
#define IN4 19
#define ENA 27
#define ENB 23

// ==============================
// Ultrasonic Pins
// ==============================
#define TRIG 4
#define ECHO 5

// ==============================
// IR Edge Sensors
// ==============================
#define IR_FRONT 34
#define IR_BACK 35

// ==============================
// SPST Mode Switch
// ==============================
#define MODE_SWITCH 15   // HIGH = MANUAL, LOW = AUTO

// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);
  BT.begin("SUMOBOT_BT");   // Bluetooth name

  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("SUMOBOT INIT");
  display.println("Ultrasonic + 2 IR Edge");
  display.display();
  delay(1500);
  display.clearDisplay();

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Sensors
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_FRONT, INPUT);
  pinMode(IR_BACK, INPUT);

  // SPST switch
  pinMode(MODE_SWITCH, INPUT_PULLDOWN);

  Serial.println("Sumobot Ready.");
}

// ==============================
// OLED DISPLAY FUNCTION
// ==============================
void showOLED(long distance, int irFront, int irBack, String mode, String motorDir) {
  display.clearDisplay();

  display.setTextSize(2);
  String distStr = "DIST: " + String(distance) + "cm";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(distStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 0);
  display.println(distStr);

  display.setTextSize(1);
  String irStr = "FRONT: " + String(irFront == LOW ? "EDGE" : "OK")
               + "  BACK: " + String(irBack == LOW ? "EDGE" : "OK");
  display.getTextBounds(irStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 22);
  display.println(irStr);

  String modeStr = "MODE: " + mode;
  display.getTextBounds(modeStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 35);
  display.println(modeStr);

  display.setTextSize(2);
  String arrow = "-";
  if (motorDir == "FORWARD") arrow = "^";
  else if (motorDir == "BACKWARD") arrow = "v";
  else if (motorDir == "LEFT") arrow = "<";
  else if (motorDir == "RIGHT") arrow = ">";

  display.getTextBounds(arrow, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 50);
  display.println(arrow);

  display.display();
}

// ==============================
// MOTOR CONTROL
// ==============================
void motorSpeed() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left_turn() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right_turn() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ==============================
// ULTRASONIC
// ==============================
long getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 20000);
  long distance = duration * 0.0343 / 2;

  if (distance == 0 || distance > 300) return 999;
  return distance;
}

// ==============================
// MAIN LOOP
// ==============================
void loop() {
  motorSpeed();

  long dist = getDistance();
  int irFront = digitalRead(IR_FRONT);
  int irBack = digitalRead(IR_BACK);

  bool manual = digitalRead(MODE_SWITCH);
  String currentMode = manual ? "MANUAL" : "";
  String motorDir = "STOP";

  // ==============================
  // MANUAL MODE (Bluetooth)
  // ==============================
  if (manual) {
    if (BT.available()) {
      char cmd = BT.read();

      if (cmd == 'F') { forward(); motorDir = "FORWARD"; }
      else if (cmd == 'B') { backward(); motorDir = "BACKWARD"; }
      else if (cmd == 'L') { left_turn(); motorDir = "LEFT"; }
      else if (cmd == 'R') { right_turn(); motorDir = "RIGHT"; }
      else if (cmd == 'S') { stopMotor(); motorDir = "STOP"; }
    }

    showOLED(dist, irFront, irBack, "MANUAL", motorDir);
    return;
  }

  // ==============================
  // AUTO MODE
  // ==============================

  // FRONT EDGE
  if (irFront == LOW) {
    backward();
    motorDir = "BACKWARD";
    currentMode = "ESCAPE";
    delay(300);
    right_turn();
    delay(300);
  }
  // BACK EDGE
  else if (irBack == LOW) {
    forward();
    motorDir = "FORWARD";
    currentMode = "ESCAPE";
    delay(300);
    left_turn();
    delay(300);
  }
  // ATTACK
  else if (dist <= 35) {
    forward();
    motorDir = "FORWARD";
    currentMode = "ATTACK";
    delay(120);
  }
  // SEARCH
  else {
    currentMode = "SEARCH";
    left_turn();
    motorDir = "LEFT";
    delay(350);

    dist = getDistance();
    if (dist <= 35) {
      motorDir = "FORWARD";
    } else {
      right_turn();
      motorDir = "RIGHT";
      delay(350);
      dist = getDistance();
      if (dist <= 35) motorDir = "FORWARD";
    }

    stopMotor();
    delay(80);
  }

  showOLED(dist, irFront, irBack, currentMode, motorDir);
}
