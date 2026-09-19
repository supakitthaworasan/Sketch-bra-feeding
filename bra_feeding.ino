/*
 * Bra_Feeding - ESP32 WROOM-32D
 * ESP32 Arduino Core 3.x
 *
 * คำสั่งที่รับ (ตัวพิมพ์เล็ก): start, stop, pause, low, mid, high
 * สถานะที่ส่งกลับด้วย notify: "<running>,<level>"  เช่น "1,2"
 *   running: 0 = หยุด, 1 = กำลังทำงาน
 *   level  : 1 = low, 2 = mid, 3 = high
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const int motorRightPin = 18;
const int motorLeftPin  = 19;

const int pwmFreq = 5000;
const int pwmResolution = 10;   // 0-1023

const int DUTY_LOW  = 341;
const int DUTY_MID  = 682;
const int DUTY_HIGH = 1023;

bool deviceConnected = false;
bool isRunning = false;
int  levelIndex = 1;            // 1=low, 2=mid, 3=high

BLECharacteristic *pCharacteristic = nullptr;

int dutyForLevel(int level) {
  if (level == 3) return DUTY_HIGH;
  if (level == 2) return DUTY_MID;
  return DUTY_LOW;
}

void notifyStatus() {
  if (!deviceConnected || pCharacteristic == nullptr) return;
  String payload = String(isRunning ? 1 : 0) + "," + String(levelIndex);
  pCharacteristic->setValue(payload.c_str());
  pCharacteristic->notify();
}

void updateMotors() {
  int duty = isRunning ? dutyForLevel(levelIndex) : 0;
  ledcWrite(motorRightPin, duty);
  ledcWrite(motorLeftPin,  duty);
  notifyStatus();
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    isRunning = false;
    updateMotors();                 // ตัดมอเตอร์ทันทีเพื่อความปลอดภัย
    pServer->startAdvertising();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String command = pChar->getValue();   // core 3.x คืนค่าเป็น String
    command.trim();
    command.toLowerCase();

    if (command == "start") {
      isRunning = true;
      levelIndex = 1;                     // เริ่มที่ระดับต่ำเสมอ
    } else if (command == "stop" || command == "pause") {
      isRunning = false;                  // pause = ตัดมอเตอร์ แต่จำระดับไว้
    } else if (command == "low") {
      levelIndex = 1;
    } else if (command == "mid") {
      levelIndex = 2;
    } else if (command == "high") {
      levelIndex = 3;
    } else {
      return;                             // คำสั่งไม่รู้จัก ไม่ต้องทำอะไร
    }
    updateMotors();
  }
};

void setup() {
  ledcAttach(motorRightPin, pwmFreq, pwmResolution);
  ledcAttach(motorLeftPin,  pwmFreq, pwmResolution);
  ledcWrite(motorRightPin, 0);
  ledcWrite(motorLeftPin,  0);

  BLEDevice::init("ESP32_Bra_Feeding");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("0,1");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {
  delay(2000);
}
