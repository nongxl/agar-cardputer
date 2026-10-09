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
        // [V72] 调整震动强度分级：短脉冲给足起振占空比，确保轻触反馈清晰有力
        uint8_t level;
        if (duration_ms <= 30)       level = 190;
        else if (duration_ms <= 60)  level = 210;
        else if (duration_ms < 100)  level = 220;
        else if (duration_ms < 150)  level = 240;  // 撞刺球
        else if (duration_ms < 300)  level = 220;  // 复活提醒
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
