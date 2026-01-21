#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_VL53L1X.h>

// sensor and i2c config
static constexpr size_t   kNumSensors     = 4;
static constexpr uint8_t  kXshutPins[]    = { 13, 14, 27, 25 };
static constexpr uint8_t  kI2CAddresses[] = { 0x30, 0x31, 0x32, 0x33 };
static constexpr uint32_t kTimingBudgetUs = 50000;  // 50 ms

// wifi credentials
const char* ssid     = "BakerStreet";
const char* password = "Cookie098";

// udp listener for commands from the pc
static constexpr unsigned int kCtrlPort = 5005;
WiFiUDP udp;

// esp-now peer mac for the esp8266 motor controller
uint8_t motorMAC[6] = { 0xE0, 0x98, 0x06, 0x8E, 0x0F, 0x4D };

// vl53l1x instances (xshut used to assign unique i2c addresses)
Adafruit_VL53L1X sensors[kNumSensors] = {
  Adafruit_VL53L1X(kXshutPins[0]),
  Adafruit_VL53L1X(kXshutPins[1]),
  Adafruit_VL53L1X(kXshutPins[2]),
  Adafruit_VL53L1X(kXshutPins[3]),
};

// latest distance readings in mm
static int16_t distances[kNumSensors] = { -1, -1, -1, -1 };
static int16_t avoidDistance = 120;

// avoidance state
char avoidDirection = 'O';
static bool avoidanceActive = false;

// esp-now send callback (kept for optional debug)
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  (void)tx_info;
  (void)status;
}

// send one byte command to the motor esp8266
void sendESPNOW(char cmd) {
  esp_now_send(motorMAC, (uint8_t*)&cmd, 1);
}

// bring one sensor out of reset, assign address, and start ranging
void initSensor(size_t i) {
  pinMode(kXshutPins[i], OUTPUT);
  digitalWrite(kXshutPins[i], LOW);
  delay(10);
  digitalWrite(kXshutPins[i], HIGH);
  delay(50);

  if (!sensors[i].begin(kI2CAddresses[i])) {
    Serial.printf("sensor %u @0x%02X failed\n", i, kI2CAddresses[i]);
    while (1) delay(10);
  }
  sensors[i].setTimingBudget(kTimingBudgetUs);
  sensors[i].startRanging();
  Serial.printf("sensor %u @0x%02X ready\n", i, kI2CAddresses[i]);
}

// i2c scan for quick wiring/address verification
void scanI2C() {
  Serial.println("i2c scan:");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("found 0x%02X\n", addr);
    }
  }
}

// apply obstacle avoidance before sending the final command
void handleCommand(char cmd, const int16_t dist[]) {
  const int16_t kClear = avoidDistance;      // blocked if <= this
  const int16_t kRelease = avoidDistance + 30; // must exceed this to "clear" (hysteresis)

  auto valid = [](int16_t mm) { return mm > 0; };
  auto blocked = [&](int16_t mm) { return valid(mm) && mm <= kClear; };
  auto clear = [&](int16_t mm) { return !valid(mm) || mm >= kRelease; }; 
  // if a sensor is invalid (-1), treat as "clear" to avoid false stops

  // interpret distances
  bool frontL_block = blocked(dist[1]);
  bool frontR_block = blocked(dist[2]);
  bool front_clear  = clear(dist[1]) && clear(dist[2]);

  bool left_clear   = clear(dist[0]);  // adjust indices if needed
  bool right_clear  = clear(dist[3]);

  bool wantsForward = (cmd == 'W' || cmd == 'Q' || cmd == 'E');

  if (!wantsForward) {
    // manual non-forward commands pass through unchanged
    avoidanceActive = false;
    avoidDirection = 'O';
    sendESPNOW(cmd);
    return;
  }

  if (front_clear) {
    // path is clear, pass through
    avoidanceActive = false;
    avoidDirection = 'O';
    sendESPNOW(cmd);
    return;
  }

  // front is blocked: choose an avoidance action
  avoidanceActive = true;

  // prefer strafing around the obstacle
  if (left_clear && !right_clear) {
    avoidDirection = 'A';   // strafe left
  } else if (right_clear && !left_clear) {
    avoidDirection = 'D';   // strafe right
  } else if (left_clear && right_clear) {
    // both sides look ok: bias away from the closer front sensor
    // if one is invalid, this also biases toward the valid side
    int16_t fl = valid(dist[1]) ? dist[1] : 9999;
    int16_t fr = valid(dist[2]) ? dist[2] : 9999;
    avoidDirection = (fl < fr) ? 'D' : 'A';
  } else {
    // sides not clear: rotate toward the larger opening
    int16_t l = valid(dist[0]) ? dist[0] : 0;
    int16_t r = valid(dist[3]) ? dist[3] : 0;
    avoidDirection = (r >= l) ? '3' : '1'; // rotate right if right is more open
  }

  sendESPNOW(avoidDirection);
}


static char currentCmd = 'S';

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(21, 22);
  Wire.setClock(400000);
  scanI2C();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("wifi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.printf("\nip: %s\n", WiFi.localIP().toString().c_str());

  udp.begin(kCtrlPort);
  Serial.printf("udp @%u\n", kCtrlPort);

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp-now init failed");
    while (1) delay(10);
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, motorMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("failed to add peer");
    while (1) delay(10);
  }

  for (size_t i = 0; i < kNumSensors; i++) {
    initSensor(i);
  }
}

void loop() {
  for (size_t i = 0; i < kNumSensors; ++i) {
    if (sensors[i].dataReady()) {
      uint16_t mm = sensors[i].distance();
      if (mm > 0) distances[i] = mm;
      sensors[i].clearInterrupt();
    }
  }

  if (udp.parsePacket() > 0) {
    char cmd;
    udp.read(&cmd, 1);
    currentCmd = cmd;
  }

  handleCommand(currentCmd, distances);

  delay(100);
}
