#include "Haptics.h"

HapticManager::HapticManager() : _vibr_pin(-1), _use_power_motor(false), _stop_ms(0), _is_vibrating(false) {}

void HapticManager::init() {
#ifdef TARGET_STOPWATCH
    // StopWatch：内置振动马达，由 M5Unified 电源管理驱动
    _use_power_motor = true;
    M5.Power.setVibration(0);
#else
    // 只有非 Cardputer 设备才初始化 Pin 0 (StickS3 Hat Vibrator)
    if (M5.getBoard() != m5::board_t::board_M5Cardputer) {
        _vibr_pin = 0;
        pinMode(_vibr_pin, OUTPUT);
        digitalWrite(_vibr_pin, LOW);
    }
#endif
}

void HapticManager::update() {
    if (!_is_vibrating) return;
    if (_vibr_pin == -1 && !_use_power_motor) return;

    if (millis() >= _stop_ms) {
        if (_use_power_motor) M5.Power.setVibration(0);
        else digitalWrite(_vibr_pin, LOW);
        _is_vibrating = false;
    }
}

void HapticManager::vibrate(uint16_t duration_ms) {
    if (duration_ms == 0) return;

    if (_use_power_motor) {
        // 按时长分级强度：轻触/吃食物 (<=20ms) 弱；撞刺球 (~100ms) 强；死亡 (>=300ms) 最强
        uint8_t level;
        if (duration_ms <= 20)       level = 70;
        else if (duration_ms <= 50)  level = 110;
        else if (duration_ms < 100)  level = 150;
        else if (duration_ms < 150)  level = 230;  // 撞刺球
        else if (duration_ms < 300)  level = 200;  // 复活提醒
        else                         level = 255;  // 死亡
        M5.Power.setVibration(level);
        _stop_ms = millis() + duration_ms;
        _is_vibrating = true;
        return;
    }

    if (_vibr_pin == -1) return;
    digitalWrite(_vibr_pin, HIGH);
    _stop_ms = millis() + duration_ms;
    _is_vibrating = true;
}
