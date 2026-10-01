#include "ble/speedbike_link.h"

#include <NimBLEDevice.h>
#include <esp32-hal-log.h>

#include "config/speedbike_config.h"
#include "core/byte_ring.h"

namespace {
ByteRing ring;
volatile bool deviceFound = false;
volatile bool connected = false;
volatile bool disconnected = false;
volatile int disconnectReason = 0;
bool hasTargetAddress = false;
NimBLEAddress targetAddress;
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *writeCharacteristic = nullptr;
portMUX_TYPE ringMutex = portMUX_INITIALIZER_UNLOCKED;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *candidate) override {
        if (candidate->getName() != SpeedBikeConfig::DeviceName) {
            return;
        }
        targetAddress = candidate->getAddress();
        hasTargetAddress = true;
        deviceFound = true;
        NimBLEDevice::getScan()->stop();
    }
};

class ClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient *, int reason) override {
        writeCharacteristic = nullptr;
        disconnectReason = reason;
        disconnected = true;
    }
};

ScanCallbacks scanCallbacks;
ClientCallbacks clientCallbacks;

void notifyCallback(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
    portENTER_CRITICAL(&ringMutex);
    ring.push(data, length);
    portEXIT_CRITICAL(&ringMutex);
}
}

void SpeedBikeLink::begin() {
    NimBLEDevice::init("");
    client = NimBLEDevice::createClient();
    client->setConnectTimeout(5);
    client->setClientCallbacks(&clientCallbacks, false);
}

void SpeedBikeLink::startScan() {
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->clearResults();
    scan->setScanCallbacks(&scanCallbacks);
    scan->setActiveScan(true);
    scan->start(0, false);
}

bool SpeedBikeLink::takeDeviceFound() {
    const bool result = deviceFound;
    deviceFound = false;
    return result;
}

bool SpeedBikeLink::connect() {
    if (!client || !hasTargetAddress || !client->connect(targetAddress)) {
        return false;
    }
    NimBLERemoteService *service = client->getService("fff0");
    if (!service) { client->disconnect(); return false; }
    NimBLERemoteCharacteristic *notifyCharacteristic = service->getCharacteristic("fff1");
    writeCharacteristic = service->getCharacteristic("fff2");
    if (!notifyCharacteristic || !writeCharacteristic || !notifyCharacteristic->subscribe(true, notifyCallback)) { client->disconnect(); return false; }
    connected = true;
    return true;
}

bool SpeedBikeLink::takeConnected() { const bool result = connected; connected = false; return result; }
bool SpeedBikeLink::takeDisconnected() { const bool result = disconnected; disconnected = false; if (result) log_w("speedbike disconnected: %d", disconnectReason); return result; }
bool SpeedBikeLink::popByte(uint8_t &byte) { portENTER_CRITICAL(&ringMutex); const bool result = ring.pop(byte); portEXIT_CRITICAL(&ringMutex); return result; }
bool SpeedBikeLink::write(const uint8_t *bytes, size_t length) { return writeCharacteristic && writeCharacteristic->writeValue(bytes, length, false); }
