// Body Feather M0 - Dual I2C Path Version
// Default Wire SDA/SCL:
//   - Master bus for OLED bonnet
//   - Master bus to send commands TO STEALTH at address 9
//
// SERCOM1 on D11/D13:
//   - Slave bus at address 10
//   - Receives commands FROM STEALTH

#include <Wire.h>
#include <SPI.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "wiring_private.h"

#define BODY_I2C_ADDRESS 10
#define STEALTH_I2C_ADDR 9

#define BODY_RADIO_NODE 10
#define FRONT_RADIO_NODE 20
#define REAR_RADIO_NODE 30
#define DOME_RADIO_NODE 40

#define RFM69_CS  8
#define RFM69_INT 3
#define RFM69_RST 4

#define RF69_FREQ 915.0

#define STEALTH_CMD_FRONT_ALL_OPEN   0x16
#define STEALTH_CMD_FRONT_ALL_CLOSE  0x17
#define STEALTH_CMD_REAR_ALL_OPEN    0x18
#define STEALTH_CMD_REAR_ALL_CLOSE   0x19
#define STEALTH_CMD_DOME_ALL_OPEN    0x20
#define STEALTH_CMD_DOME_ALL_CLOSE   0x21
#define STEALTH_CMD_DOME_WAVE        0x22

#define ACTION_SERVO_GROUP_MOVE      2
#define ACTION_DOME_ALL_OPEN         5
#define ACTION_DOME_ALL_CLOSE        6
#define ACTION_DOME_WAVE             7
#define ACTION_STEALTH_PLAY_SOUND    0x30

#define GROUP_ALL_SERVOS 255

#define SERVO_POS_OPEN    1
#define SERVO_POS_CLOSED  2

// Alternate I2C receive-only bus:
// D11 = SDA
// D13 = SCL
TwoWire stealthReceiveWire(&sercom1, 11, 13);

// OLED on default Wire bus
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

RH_RF69 rf69(RFM69_CS, RFM69_INT);
RHReliableDatagram manager(rf69, BODY_RADIO_NODE);

volatile bool i2cPacketReady = false;
volatile uint8_t i2cValue = 0;

bool oledReady = false;

struct PanelCommand {
  uint8_t actionType;
  uint8_t targetGroup;
  uint8_t position;
  uint8_t reserved;
};

void SERCOM1_Handler() {
  stealthReceiveWire.onService();
}

void oledStatus(const char* line1, const char* line2 = "", const char* line3 = "") {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 10);
  display.println(line2);

  display.setCursor(0, 20);
  display.println(line3);

  display.display();
}

void scanDefaultI2CBus() {
  Serial.println("Scanning default I2C bus...");

  int count = 0;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Default I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" / decimal ");
      Serial.println(address);
      count++;
    }
  }

  if (count == 0) {
    Serial.println("No devices found on default I2C bus.");
  }

  Serial.println("Default I2C scan done.");
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

void receiveStealthI2C(int byteCount) {
  if (stealthReceiveWire.available()) {
    i2cValue = stealthReceiveWire.read();
    i2cPacketReady = true;
  }

  while (stealthReceiveWire.available()) {
    stealthReceiveWire.read();
  }
}

void setupDefaultI2CMasterBus() {
  Wire.begin();

  // for debugging only
  // scanDefaultI2CBus();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed.");
    oledReady = false;
  } else {
    oledReady = true;
    oledStatus("Body Feather", "OLED ready", "Default I2C");
    Serial.println("OLED ready on default I2C bus.");
  }
}

void setupStealthReceiveBus() {
  stealthReceiveWire.begin(BODY_I2C_ADDRESS);
  stealthReceiveWire.onReceive(receiveStealthI2C);

  pinPeripheral(11, PIO_SERCOM);
  pinPeripheral(13, PIO_SERCOM);

  Serial.println("STEALTH receive I2C ready on D11=SDA, D13=SCL, addr 10.");
}

void sendStealthI2CCommand(const char* cmd) {
  byte sum = 0;

  Serial.print("Sending STEALTH I2C command on default Wire bus: ");
  Serial.println(cmd);

  oledStatus("TX to STEALTH", cmd, "Default I2C");

  Wire.beginTransmission(STEALTH_I2C_ADDR);

  for (int i = 0; cmd[i] != '\0'; i++) {
    Wire.write(cmd[i]);
    sum += byte(cmd[i]);
  }

  Wire.write(sum);

  byte result = Wire.endTransmission();

  Serial.print("STEALTH I2C result=");
  Serial.println(result);

  char line3[22];
  snprintf(line3, sizeof(line3), "I2C result %u", result);
  oledStatus("TX to STEALTH", cmd, line3);
}

void playStealthSoundBank(uint8_t bankNumber) {
  char cmd[4];
  snprintf(cmd, sizeof(cmd), "$%02u", bankNumber);
  sendStealthI2CCommand(cmd);
}

void setupRadio() {
  resetRadio();

  if (!manager.init()) {
    Serial.println("ERROR: RFM69 init failed.");
    oledStatus("ERROR", "RFM69 init failed", "");
    while (true) delay(1000);
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("ERROR: setFrequency failed.");
    oledStatus("ERROR", "Radio freq failed", "");
    while (true) delay(1000);
  }

  rf69.setTxPower(14, true);
  rf69.setModeRx();

  Serial.println("RFM69 radio ready.");
  oledStatus("Body Feather", "Radio ready", "Node 10");
}

bool sendCommand(uint8_t destinationNode,
                 uint8_t actionType,
                 uint8_t targetGroup,
                 uint8_t position) {
  PanelCommand cmd;
  cmd.actionType = actionType;
  cmd.targetGroup = targetGroup;
  cmd.position = position;
  cmd.reserved = 0;

  Serial.print("Sending radio command to node ");
  Serial.print(destinationNode);
  Serial.print(": ");
  Serial.print(actionType);
  Serial.print(" ");
  Serial.print(targetGroup);
  Serial.print(" ");
  Serial.println(position);

  bool ok = manager.sendtoWait(
    (uint8_t *)&cmd,
    sizeof(cmd),
    destinationNode
  );

  rf69.setModeRx();

  Serial.print("Radio send result: ");
  Serial.println(ok ? "SUCCESS" : "FAILED");

  return ok;
}

void handleStealthI2CCommand(uint8_t command) {
  Serial.println();
  Serial.print("STEALTH I2C command received. DEC: ");
  Serial.print(command);
  Serial.print(" HEX: 0x");
  Serial.println(command, HEX);

  char line2[22];
  snprintf(line2, sizeof(line2), "CMD 0x%02X", command);
  oledStatus("RX from STEALTH", line2, "Alt I2C D11/D13");

  bool ok = false;

  switch (command) {
    case STEALTH_CMD_FRONT_ALL_OPEN:
      ok = sendCommand(FRONT_RADIO_NODE, ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_OPEN);
      oledStatus("Front Open", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_FRONT_ALL_CLOSE:
      ok = sendCommand(FRONT_RADIO_NODE, ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED);
      oledStatus("Front Close", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_REAR_ALL_OPEN:
      ok = sendCommand(REAR_RADIO_NODE, ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_OPEN);
      oledStatus("Rear Open", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_REAR_ALL_CLOSE:
      ok = sendCommand(REAR_RADIO_NODE, ACTION_SERVO_GROUP_MOVE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED);
      oledStatus("Rear Close", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_DOME_ALL_OPEN:
      ok = sendCommand(DOME_RADIO_NODE, ACTION_DOME_ALL_OPEN, GROUP_ALL_SERVOS, SERVO_POS_OPEN);
      oledStatus("Dome Open", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_DOME_ALL_CLOSE:
      ok = sendCommand(DOME_RADIO_NODE, ACTION_DOME_ALL_CLOSE, GROUP_ALL_SERVOS, SERVO_POS_CLOSED);
      oledStatus("Dome Close", ok ? "Radio success" : "Radio failed", "");
      break;

    case STEALTH_CMD_DOME_WAVE:
      ok = sendCommand(DOME_RADIO_NODE, ACTION_DOME_WAVE, GROUP_ALL_SERVOS, 0);
      oledStatus("Dome Wave", ok ? "Radio success" : "Radio failed", "");
      break;

    default:
      Serial.println("No mapped action for this STEALTH I2C command.");
      oledStatus("Unknown STEALTH", line2, "No mapped action");
      break;
  }

  rf69.setModeRx();
}

void handleBodyRadioCommand(uint8_t* data, uint8_t len, uint8_t from) {
  Serial.println();
  Serial.print("Radio command received from node ");
  Serial.print(from);
  Serial.print(" len=");
  Serial.println(len);

  Serial.print("Payload: ");
  for (uint8_t i = 0; i < len; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (len < sizeof(PanelCommand)) {
    Serial.println("Radio packet too short. Ignoring.");
    oledStatus("Radio RX", "Packet too short", "");
    rf69.setModeRx();
    return;
  }

  PanelCommand cmd;
  memcpy(&cmd, data, sizeof(PanelCommand));

  char line2[22];
  snprintf(line2, sizeof(line2), "from %u act 0x%02X", from, cmd.actionType);
  oledStatus("Radio RX", line2, "");

  switch (cmd.actionType) {
    case ACTION_STEALTH_PLAY_SOUND:
      Serial.print("Mapped radio action: play STEALTH sound bank ");
      Serial.println(cmd.targetGroup);
      playStealthSoundBank(cmd.targetGroup);
      break;

    default:
      Serial.print("No mapped Body radio action for actionType 0x");
      Serial.println(cmd.actionType, HEX);
      oledStatus("Radio RX", "Unknown action", "");
      break;
  }

  rf69.setModeRx();
}

void receiveRadioOnce() {
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from;

  if (manager.recvfromAck(buf, &len, &from)) {
    handleBodyRadioCommand(buf, len, from);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("Body Feather starting...");
  Serial.println("Default I2C: master for OLED + STEALTH addr 9");
  Serial.println("Alt I2C: slave receive from STEALTH at addr 10");
  Serial.println("Alt I2C pins: D11 SDA, D13 SCL");
  Serial.println("Radio node: 10");

  setupDefaultI2CMasterBus();
  setupStealthReceiveBus();
  setupRadio();

  oledStatus("Body Feather", "Ready", "Dual I2C active");
  Serial.println("Body Feather ready.");
}

void loop() {
  static unsigned long lastHeartbeat = 0;

  if (millis() - lastHeartbeat > 2000) {
    lastHeartbeat = millis();
    Serial.println("Loop heartbeat - polling radio");
  }

  receiveRadioOnce();

  if (i2cPacketReady) {
    uint8_t command;

    noInterrupts();
    command = i2cValue;
    i2cPacketReady = false;
    interrupts();

    handleStealthI2CCommand(command);
  }
}