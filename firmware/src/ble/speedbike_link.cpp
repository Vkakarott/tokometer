#include "ble/speedbike_link.h"

#include <cctype>
#include <cstring>
#include <string>
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

bool namesMatchIgnoreCase(const std::string &candidate, const char *expected) {
    const size_t expectedLength = strlen(expected);
    if (candidate.length() != expectedLength) {
        return false;
    }

    for (size_t index = 0; index < expectedLength; ++index) {
        const unsigned char candidateByte = static_cast<unsigned char>(candidate[index]);
        const unsigned char expectedByte = static_cast<unsigned char>(expected[index]);
        if (std::tolower(candidateByte) != std::tolower(expectedByte)) {
            return false;
        }
    }

    return true;
}

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *candidate) override {
        if (!namesMatchIgnoreCase(candidate->getName(), SpeedBikeConfig::DeviceName)) {
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
        log_w("connect failed err=%d", client ? client->getLastError() : -1);
        return false;
    }

    NimBLERemoteService *service = client->getService("fff0");
    if (!service) {
        log_w("service fff0 not found");
        client->disconnect();
        return false;
    }

    NimBLERemoteCharacteristic *notifyCharacteristic = service->getCharacteristic("fff1");
    writeCharacteristic = service->getCharacteristic("fff2");
    if (!notifyCharacteristic || !writeCharacteristic) {
        log_w("fff1/fff2 missing (notify=%p write=%p)", notifyCharacteristic, writeCharacteristic);
        client->disconnect();
        return false;
    }

    if (!notifyCharacteristic->subscribe(true, notifyCallback)) {
        log_w("subscribe failed");
        client->disconnect();
        return false;
    }

    log_i("fff2 write=%d no-response=%d; fff1 notify=%d indicate=%d",
          writeCharacteristic->canWrite(), writeCharacteristic->canWriteNoResponse(),
          notifyCharacteristic->canNotify(), notifyCharacteristic->canIndicate());
    connected = true;
    return true;
}

bool SpeedBikeLink::takeConnected() { const bool result = connected; connected = false; return result; }
bool SpeedBikeLink::takeDisconnected() { const bool result = disconnected; disconnected = false; if (result) log_w("speedbike disconnected: %d", disconnectReason); return result; }
bool SpeedBikeLink::popByte(uint8_t &byte) { portENTER_CRITICAL(&ringMutex); const bool result = ring.pop(byte); portEXIT_CRITICAL(&ringMutex); return result; }
bool SpeedBikeLink::write(const uint8_t *bytes, size_t length) {
    char hex[49] = "";
    size_t offset = 0;
    for (size_t index = 0; index < length && offset + 3 < sizeof hex; ++index) {
        offset += snprintf(hex + offset, sizeof hex - offset, "%02X ", bytes[index]);
    }
    const bool result = writeCharacteristic && writeCharacteristic->writeValue(bytes, length, false);
    log_i("TX %sresult=%d", hex, result);
    return result;
}
