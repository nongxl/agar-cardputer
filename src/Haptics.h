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
    uint32_t _stop_ms;
    bool _is_vibrating;
};

#endif // HAPTICS_H
