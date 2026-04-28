#include "Haptics.h"

HapticManager::HapticManager() : _vibr_pin(-1), _stop_ms(0), _is_vibrating(false) {}

void HapticManager::init() {
    // 只有非 Cardputer 设备才初始化 Pin 0 (StickS3 Hat Vibrator)
    if (M5.getBoard() != m5::board_t::board_M5Cardputer) {
        _vibr_pin = 0;
        pinMode(_vibr_pin, OUTPUT);
        digitalWrite(_vibr_pin, LOW);
    }
}

void HapticManager::update() {
    if (_vibr_pin == -1 || !_is_vibrating) return;

    if (millis() >= _stop_ms) {
        digitalWrite(_vibr_pin, LOW);
        _is_vibrating = false;
    }
}

void HapticManager::vibrate(uint16_t duration_ms) {
    if (_vibr_pin == -1 || duration_ms == 0) return;

    digitalWrite(_vibr_pin, HIGH);
    _stop_ms = millis() + duration_ms;
    _is_vibrating = true;
}
