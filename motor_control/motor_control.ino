/********************************************************************
 * esp8266 robot car esp-now receiver
 ********************************************************************/
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <user_interface.h>

// motor pins
#define motor1Pin1 5   // d1
#define motor1Pin2 4   // d2
#define motor2Pin1 0   // d3
#define motor2Pin2 2   // d4
#define motor3Pin1 14  // d5
#define motor3Pin2 12  // d6
#define motor4Pin1 13  // d7
#define motor4Pin2 15  // d8

void handleCmd(char c);

void forward(), forwardLeft(), forwardRight();
void rotateLeft(), rotateRight(), left(), right();
void backward(), backLeft(), backRight(), stop();

// shared command state
volatile char pendingCmd = 'S';
volatile bool newCmd = false;

// esp-now receive callback
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len == 1) {
    pendingCmd = (char)incomingData[0];
    newCmd = true;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(motor1Pin1, OUTPUT); pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT); pinMode(motor2Pin2, OUTPUT);
  pinMode(motor3Pin1, OUTPUT); pinMode(motor3Pin2, OUTPUT);
  pinMode(motor4Pin1, OUTPUT); pinMode(motor4Pin2, OUTPUT);
  stop();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(1);

  if (esp_now_init() != 0) {
    Serial.println("esp-now init failed");
    while (true) delay(100);
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.print("ready mac = ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  if (newCmd) {
    noInterrupts();
    char c = pendingCmd;
    newCmd = false;
    interrupts();

    Serial.printf("cmd: %c\n", c);
    handleCmd(c);
  }
}

void handleCmd(char c) {
  switch (c) {
    case 'W': forward();      break;
    case 'A': left();         break;
    case 'D': right();        break;
    case 'S': stop();         break;
    case 'Q': forwardLeft();  break;
    case 'E': forwardRight(); break;
    case 'Z': backLeft();     break;
    case 'X': backward();     break;
    case 'C': backRight();    break;
    case '1': rotateLeft();   break;
    case '3': rotateRight();  break;
    default : stop();         break;
  }
}

// movement functions (original behavior)

void rotateLeft() {
  digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH);
  digitalWrite(motor3Pin1, HIGH); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW);  digitalWrite(motor4Pin2, HIGH);
}

void rotateRight() {
  digitalWrite(motor1Pin1, LOW);  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW);  digitalWrite(motor3Pin2, HIGH);
  digitalWrite(motor4Pin1, HIGH); digitalWrite(motor4Pin2, LOW);
}

void forwardLeft() {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, HIGH);
  digitalWrite(motor3Pin1, LOW); digitalWrite(motor3Pin2, HIGH);
  digitalWrite(motor4Pin1, LOW); digitalWrite(motor4Pin2, LOW);
}

void forward() {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, HIGH);
  digitalWrite(motor3Pin1, LOW); digitalWrite(motor3Pin2, HIGH);
  digitalWrite(motor4Pin1, LOW); digitalWrite(motor4Pin2, HIGH);
}

void forwardRight() {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW); digitalWrite(motor4Pin2, HIGH);
}

void left() {
  digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, HIGH);
  digitalWrite(motor3Pin1, LOW);  digitalWrite(motor3Pin2, HIGH);
  digitalWrite(motor4Pin1, HIGH); digitalWrite(motor4Pin2, LOW);
}

void right() {
  digitalWrite(motor1Pin1, LOW);  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, HIGH); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW);  digitalWrite(motor4Pin2, HIGH);
}

void stop() {
  digitalWrite(motor1Pin1, LOW); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW); digitalWrite(motor4Pin2, LOW);
}

void backLeft() {
  digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);  digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, LOW);  digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, HIGH); digitalWrite(motor4Pin2, LOW);
}

void backward() {
  digitalWrite(motor1Pin1, HIGH); digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, HIGH); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, HIGH); digitalWrite(motor4Pin2, LOW);
}

void backRight() {
  digitalWrite(motor1Pin1, LOW);  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH); digitalWrite(motor2Pin2, LOW);
  digitalWrite(motor3Pin1, HIGH); digitalWrite(motor3Pin2, LOW);
  digitalWrite(motor4Pin1, LOW);  digitalWrite(motor4Pin2, LOW);
}
