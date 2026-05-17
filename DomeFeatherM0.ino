// Dome Feather M0
#include <Wire.h>
#include <SPI.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>
#include <Adafruit_PWMServoDriver.h>

#define DOME_RADIO_NODE 40

#define RFM69_CS  8
#define RFM69_INT 3
#define RFM69_RST 4
#define LED       13

#define RF69_FREQ 915.0

#define PCA9685_ADDRESS 0x40
#define SERVO_FREQ 50

#define ACTION_DOME_ALL_OPEN   5
#define ACTION_DOME_ALL_CLOSE  6

#define SERVO_MOVE_TIME_MS 700
#define BETWEEN_SERVO_DELAY_MS 120

RH_RF69 rf69(RFM69_CS, RFM69_INT);
RHReliableDatagram manager(rf69, DOME_RADIO_NODE);

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADDRESS);

struct ServoConfig {
  uint8_t channel;
  const char *name;
  uint16_t openUs;
  uint16_t closedUs;
  bool isOpen;
};

struct PanelCommand {
  uint8_t actionType;
  uint8_t targetGroup;
  uint8_t position;
  uint8_t reserved;
};

uint16_t angleToUs(uint16_t angle) {
  return 500 + ((uint32_t)angle * 2000 / 180);
}

ServoConfig servos[] = {
  // Base servos: original Arduino pins 2-9, now PCA channels 0-7
  {0, "Base 2", angleToUs(40),  angleToUs(175), false},
  {1, "Base 3", angleToUs(40),  angleToUs(175), false},
  {2, "Base 4", angleToUs(40),  angleToUs(175), false},
  {3, "Base 5", angleToUs(175), angleToUs(40),  false},
  {4, "Base 6", angleToUs(60),  angleToUs(160), false},
  {5, "Base 7", angleToUs(40),  angleToUs(175), false},
  {6, "Base 8", angleToUs(40),  angleToUs(175), false},
  {7, "Base 9", angleToUs(40),  angleToUs(175), false},

  // Pie servos: original Arduino pins 2-6, now PCA channels 8-12
  {8,  "Pie 2", angleToUs(40), angleToUs(150), false},
  {9,  "Pie 3", angleToUs(40), angleToUs(150), false},
  {10, "Pie 4", angleToUs(40), angleToUs(150), false},
  {11, "Pie 5", angleToUs(40), angleToUs(160), false},
  {12, "Pie 6", angleToUs(40), angleToUs(120), false}
};

const uint8_t SERVO_COUNT = sizeof(servos) / sizeof(servos[0]);

uint16_t usToTicks(uint16_t us) {
  return (uint32_t)us * 4096 / 20000;
}

void powerDown(uint8_t channel) {
  pwm.setPWM(channel, 0, 0);
}

void moveServo(uint8_t index, bool open) {
  if (index >= SERVO_COUNT) return;

  ServoConfig &s = servos[index];

  uint16_t targetUs = open ? s.openUs : s.closedUs;
  uint16_t ticks = usToTicks(targetUs);

  Serial.print("Moving ");
  Serial.print(s.name);
  Serial.print(" on PCA channel ");
  Serial.print(s.channel);
  Serial.print(" to ");
  Serial.print(open ? "OPEN" : "CLOSED");
  Serial.print(" / ");
  Serial.print(targetUs);
  Serial.print(" us / ticks ");
  Serial.println(ticks);

  pwm.setPWM(s.channel, 0, ticks);

  delay(SERVO_MOVE_TIME_MS);

  powerDown(s.channel);

  s.isOpen = open;

  delay(BETWEEN_SERVO_DELAY_MS);
}

void openAll() {
  Serial.println();
  Serial.println("Opening ALL dome servos");

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    moveServo(i, true);
  }

  Serial.println("All dome servos opened.");
}

void closeAll() {
  Serial.println();
  Serial.println("Closing ALL dome servos");

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    moveServo(i, false);
  }

  Serial.println("All dome servos closed.");
}

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
    Serial.println("ERROR: RFM69 radio init failed.");
    while (true) delay(1000);
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("ERROR: RFM69 frequency set failed.");
    while (true) delay(1000);
  }

  rf69.setTxPower(14, true);

  Serial.println("RFM69 radio ready.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println("Dome Feather starting...");
  Serial.println("Radio node: 40");
  Serial.println("PCA9685 address: 0x40");
  Serial.println("Base servos: PCA channels 0-7");
  Serial.println("Pie servos: PCA channels 8-12");

  Wire.begin();

  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  closeAll();

  setupRadio();

  Serial.println("Dome Feather ready.");
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

      if (cmd.actionType == ACTION_DOME_ALL_OPEN) {
        openAll();
      } else if (cmd.actionType == ACTION_DOME_ALL_CLOSE) {
        closeAll();
      } else {
        Serial.println("Unknown Dome command.");
      }

      digitalWrite(LED, LOW);
    }
  }
}