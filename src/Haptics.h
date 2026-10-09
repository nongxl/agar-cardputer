#ifndef HAPTICS_H
#define HAPTICS_H

#include <Arduino.h>
#include <M5Unified.h>

class HapticManager {
public:
    HapticManager();
    void init();
    void update();
    void vibrate(uint16_t duration_ms);

private:
    int _vibr_pin;
    bool _use_power_motor; // [V70] StopWatch 内置马达 (通过 M5.Power.setVibration 控制强度)
    uint32_t _stop_ms;
    bool _is_vibrating;
};

#endif // HAPTICS_H
