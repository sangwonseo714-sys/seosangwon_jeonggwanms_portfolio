#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_BMP280.h>
#include <Servo.h>

SoftwareSerial bluetooth(2, 3);  // Arduino RX, TX
Adafruit_BMP280 bmp;
Servo servo;

const byte SERVO_PIN = 9;
const int SERVO_HOME = 0;
const int SERVO_TEST_POSITION = 45;  // 검증용 제한 각도

const float SEA_LEVEL_HPA = 1013.25;
const float TRIGGER_ALTITUDE_M = 1.0;  // 실내 기준값. 시험에 맞게 조정
const float TRIGGER_HYSTERESIS_M = 0.2;

bool automaticTestDone = false;
float baselineAltitudeM = 0.0;
float triggerAltitudeM = 0.0;

void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);

  servo.attach(SERVO_PIN);
  servo.write(SERVO_HOME);

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println(F("BMP280을 찾을 수 없습니다."));
    bluetooth.println(F("ERROR: BMP280"));
    while (true) delay(100);
  }

  // 시작 위치의 고도를 기준으로 삼아 실내 기준 높이를 설정합니다.
  baselineAltitudeM = bmp.readAltitude(SEA_LEVEL_HPA);
  triggerAltitudeM = baselineAltitudeM - TRIGGER_ALTITUDE_M;

  Serial.println(F("안전 검증 모드 준비 완료"));
  Serial.println(F("O: 수동 서보 시험, C: 원위치, R: 센서값 출력"));
  bluetooth.println(F("Ready: O=test, C=home, R=read"));

  printReadings();
}

void loop() {
  readBluetoothCommand();

  float altitudeM = bmp.readAltitude(SEA_LEVEL_HPA);

  Serial.print(F("Altitude estimate: "));
  Serial.print(altitudeM, 2);
  Serial.println(F(" m"));

  // 고도가 기준값 아래로 내려가면 검증용 서보 동작을 한 번 수행합니다.
  if (!automaticTestDone && altitudeM <= triggerAltitudeM) {
    runServoTest();
    automaticTestDone = true;
  }

  // 다시 기준값보다 충분히 높아지면 다음 시험을 허용합니다.
  if (automaticTestDone &&
      altitudeM > triggerAltitudeM + TRIGGER_HYSTERESIS_M) {
    automaticTestDone = false;
  }

  delay(200);
}

void readBluetoothCommand() {
  if (!bluetooth.available()) return;

  char command = bluetooth.read();

  if (command == 'O' || command == 'o') {
    runServoTest();
  } else if (command == 'C' || command == 'c') {
    servo.write(SERVO_HOME);
    sendMessage(F("Servo: HOME"));
  } else if (command == 'R' || command == 'r') {
    printReadings();
  }
}

void runServoTest() {
  servo.write(SERVO_TEST_POSITION);
  sendMessage(F("Servo test position"));
  delay(700);
  servo.write(SERVO_HOME);
  sendMessage(F("Servo returned home"));
}

void printReadings() {
  float pressureHpa = bmp.readPressure() / 100.0;
  float altitudeM = bmp.readAltitude(SEA_LEVEL_HPA);

  Serial.print(F("Pressure: "));
  Serial.print(pressureHpa, 2);
  Serial.print(F(" hPa, altitude estimate: "));
  Serial.print(altitudeM, 2);
  Serial.println(F(" m"));

  bluetooth.print(F("P="));
  bluetooth.print(pressureHpa, 2);
  bluetooth.print(F(" hPa, H="));
  bluetooth.print(altitudeM, 2);
  bluetooth.println(F(" m"));
}

void sendMessage(const __FlashStringHelper *message) {
  Serial.println(message);
  bluetooth.println(message);
}