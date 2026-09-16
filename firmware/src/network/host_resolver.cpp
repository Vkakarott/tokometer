#include "network/host_resolver.h"

#include <ESPmDNS.h>

#include "config/api_config.h"

namespace {

constexpr char DEVICE_MDNS_NAME[] = "tokesp";

IPAddress resolvedAddress;
bool hasAddress = false;

// ESPmDNS queries take the bare label: "machine.local" -> "machine".
String hostLabel() {
    String host(API_HOST);
    const int suffix = host.lastIndexOf(".local");
    return suffix > 0 ? host.substring(0, suffix) : host;
}

bool resolveHost() {
    const IPAddress found = MDNS.queryHost(hostLabel(), ApiConfig::MDNS_TIMEOUT_MS);
    if (static_cast<uint32_t>(found) == 0) {
        log_w("mDNS could not resolve %s", API_HOST);
        return false;
    }
    resolvedAddress = found;
    hasAddress = true;
    log_i("%s resolved to %s", API_HOST, resolvedAddress.toString().c_str());
    return true;
}

}  // namespace

void initializeHostResolver() {
    // Also publishes this board as tokesp.local, which is handy for debugging.
    MDNS.begin(DEVICE_MDNS_NAME);
}

void forgetResolvedHost() {
    hasAddress = false;
}

String backendBaseUrl() {
    if (!hasAddress && !resolveHost()) return String();
    return "http://" + resolvedAddress.toString() + ":" + String(API_PORT);
}
