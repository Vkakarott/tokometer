#include "storage/device_store.h"

#include <Preferences.h>

namespace {

constexpr char NAMESPACE[] = "tokesp";
constexpr char TOKEN_KEY[] = "token";
constexpr char LAYOUT_KEY[] = "layout";

}  // namespace

bool loadAccessToken(char *out, size_t size) {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) return false;

    const bool loaded = preferences.isKey(TOKEN_KEY) && preferences.getString(TOKEN_KEY, out, size) > 1;
    preferences.end();
    return loaded;
}

void saveAccessToken(const char *token) {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.putString(TOKEN_KEY, token);
    preferences.end();
}

void clearAccessToken() {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.remove(TOKEN_KEY);
    preferences.end();
}

uint8_t loadLayoutIndex() {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) return 0;

    const uint8_t index = preferences.isKey(LAYOUT_KEY) ? preferences.getUChar(LAYOUT_KEY, 0) : 0;
    preferences.end();
    return index;
}

void saveLayoutIndex(uint8_t index) {
    Preferences preferences;
    preferences.begin(NAMESPACE, false);
    preferences.putUChar(LAYOUT_KEY, index);
    preferences.end();
}
