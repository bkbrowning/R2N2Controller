# Rear Panel Feather M0
#include <Wire.h>
#include <SPI.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>
#include <Adafruit_PWMServoDriver.h>

#define REAR_RADIO_NODE 30

#define RFM69_CS  8
#define RFM69_INT 3
#define RFM69_RST 4
#define LED       13

#define RF69_FREQ 915.0

#define PCA9685_ADDRESS 0x40
#define SERVO_FREQ 50

#define ACTION_SERVO_GROUP_MOVE 2

#define GROUP_ALL_SERVOS 255

#define SERVO_POS_OPEN    1
#define SERVO_POS_CLOSED  2

#define SERVO_MOVE_TIME_MS 700
#define BETWEEN_SERVO_DELAY_MS 150

RH_RF69 rf69(RFM69_CS, RFM69_INT);
RHReliableDatagram manager(rf69, REAR_RADIO_NODE);

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);

struct PanelCommand {
  uint8_t actionType;
  uint8_t targetGroup;
  uint8_t position;
  uint8_t reserved;
};

struct ServoConfig {
  uint8_t pin;
  const char *name;
  uint16_t openUs;
  uint16_t closedUs;
};

ServoConfig rearServos[] = {
  {0,  "Left Side Panel",          1240, 1950},
  {1,  "Upper Door Lock",          1900, 1200},
  {2,  "Upper Door",                720, 1350},
  {3,  "Right Mid Panel",          1000, 1793},
  {4,  "Center Mid Panel",         1247, 2186},
  {5,  "Left Mid Panel",           1514,  720},
  {8,  "Left Lower Panel",         1025, 1700},
  {9,  "Right Lower Panel",        1245, 1948},
  {10, "Right-Center Lower Panel", 1728, 1200},
  {11, "Left-Center Lower Panel",  2320, 1590},
  {12, "Right Side Panel",         1600,  855}
};

const uint8_t REAR_SERVO_COUNT = sizeof(rearServos) / sizeof(rearServos[0]);

void resetRadio() {
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);
  delay(10);
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);
}

void setupRadio() {
  resetRadio();

  if (!manager.init()) {
    Serial.println("ERROR: RFM69 init failed.");
    while (true) delay(1000);
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("ERROR: setFrequency failed.");
    while (true) delay(1000);
  }

  rf69.setTxPower(14, true);
  Serial.println("RFM69 radio ready.");
}

uint16_t microsecondsToTicks(uint16_t us) {
  return (uint32_t)us * 4096 / 20000;
}

void powerDownServo(uint8_t servoPin) {
  pwm.setPWM(servoPin, 0, 0);
}

bool moveServoByIndex(uint8_t index, uint8_t position) {
  if (index >= REAR_SERVO_COUNT) {
    return false;
  }

  ServoConfig s = rearServos[index];

  uint16_t targetUs =
    position == SERVO_POS_OPEN ? s.openUs : s.closedUs;

  uint16_t ticks = microsecondsToTicks(targetUs);

  Serial.print("Moving servo ");
  Serial.print(s.pin);
  Serial.print(" - ");
  Serial.print(s.name);
  Serial.print(" to ");
  Serial.print(position == SERVO_POS_OPEN ? "OPEN" : "CLOSED");
  Serial.print(" / ");
  Serial.print(targetUs);
  Serial.print(" us / ticks ");
  Serial.println(ticks);

  pwm.setPWM(s.pin, 0, ticks);

  delay(SERVO_MOVE_TIME_MS);

  powerDownServo(s.pin);

  Serial.print("Powered down servo ");
  Serial.println(s.pin);

  delay(BETWEEN_SERVO_DELAY_MS);

  return true;
}

bool moveServoByPin(uint8_t servoPin, uint8_t position) {
  for (uint8_t i = 0; i < REAR_SERVO_COUNT; i++) {
    if (rearServos[i].pin == servoPin) {
      return moveServoByIndex(i, position);
    }
  }

  Serial.print("Servo pin not found: ");
  Serial.println(servoPin);
  return false;
}

void moveAllServos(uint8_t position) {
  Serial.println();
  Serial.print("Moving ALL rear servos to ");
  Serial.println(position == SERVO_POS_OPEN ? "OPEN" : "CLOSED");

  if (position == SERVO_POS_OPEN) {
    Serial.println("Rear door sequence: unlock/open pin 1, then open door pin 2");
    moveServoByPin(1, SERVO_POS_OPEN);
    moveServoByPin(2, SERVO_POS_OPEN);
  }

  for (uint8_t i = 0; i < REAR_SERVO_COUNT; i++) {
    uint8_t pin = rearServos[i].pin;

    if (pin == 1 || pin == 2) {
      continue;
    }

    moveServoByIndex(i, position);
  }

  if (position == SERVO_POS_CLOSED) {
    Serial.println("Rear door sequence: close door pin 2, then close/lock pin 1");
    moveServoByPin(2, SERVO_POS_CLOSED);
    moveServoByPin(1, SERVO_POS_CLOSED);
  }

  Serial.println("All rear servo movement complete.");
}

void closeAllServosOnBoot() {
  Serial.println();
  Serial.println("Boot safety: moving all rear servos to CLOSED...");

  moveAllServos(SERVO_POS_CLOSED);

  Serial.println("Boot safety complete.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println("Rear Panel Feather starting...");
  Serial.println("Radio node: 30");
  Serial.println("PCA9685 address: 0x40");

  Wire.begin();

  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  closeAllServosOnBoot();

  setupRadio();

  Serial.println("Rear Panel ready.");
}

void loop() {
  uint8_t buffer[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buffer);
  uint8_t from;

  if (manager.available()) {
    if (manager.recvfromAck(buffer, &len, &from)) {
      digitalWrite(LED, HIGH);

      Serial.println();
      Serial.print("Radio command received from node ");
      Serial.println(from);

      if (len != sizeof(PanelCommand)) {
        Serial.print("Unexpected packet size: ");
        Serial.println(len);
        digitalWrite(LED, LOW);
        return;
      }

      PanelCommand cmd;
      memcpy(&cmd, buffer, sizeof(cmd));

      Serial.print("Action type: ");
      Serial.println(cmd.actionType);

      Serial.print("Target group: ");
      Serial.println(cmd.targetGroup);

      Serial.print("Position: ");
      Serial.println(cmd.position);

      if (cmd.actionType == ACTION_SERVO_GROUP_MOVE &&
          cmd.targetGroup == GROUP_ALL_SERVOS) {
        moveAllServos(cmd.position);
      } else {
        Serial.println("Unknown command.");
      }

      digitalWrite(LED, LOW);
    }
  }
}