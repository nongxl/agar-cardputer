#include "Input.h"
#include <Arduino.h>

GameInput::GameInput() : dx(0), dy(0), imu_enabled(true) {}

bool GameInput::keyPressed(char k) {
    return M5Cardputer.Keyboard.isKeyPressed(k);
}

bool GameInput::keyTriggered(char k) {
    if (M5Cardputer.Keyboard.isKeyPressed(k)) {
        for (char c : _last_keys) { if (c == k) return false; }
        return true;
    }
    return false;
}

bool GameInput::isLeaderboardPressed() {
    // [V61] 响应手册修改：支持 ESC 键切换 (字符码 0x1B)
    auto keys = M5Cardputer.Keyboard.keysState();
    bool esc_pressed = false;
    for (auto k : keys.word) { if ((uint8_t)k == 0x1B) { esc_pressed = true; break; } }
    if (esc_pressed) return true;

    // 按照用户提供的成功案例进行重构：直接扫描 keys.word 缓冲区并引入时间防抖
    static unsigned long lastBacktickPress = 0;
    const unsigned long DEBOUNCE_DELAY = 220;
    
    // 继续使用上面获取的 keys
    for (auto key : keys.word) {
        // 同时兼容标准反引号与用户可能使用的特殊字符码
        if (key == '`' || (uint8_t)key == 0x60) {
            unsigned long currentTime = millis();
            if (currentTime - lastBacktickPress > DEBOUNCE_DELAY) {
                lastBacktickPress = currentTime;
                return true;
            }
        }
    }
    return false;
}

bool GameInput::isSpacePressed() {
    return keyTriggered(' ');
}

bool GameInput::isEnterPressed() {
    return M5Cardputer.Keyboard.keysState().enter;
}

void GameInput::update() {
    dx = 0;
    dy = 0;

    // 1. 键盘操纵
    if (keyPressed('e') || keyPressed(';')) dy = -100;
    if (keyPressed('s') || keyPressed('.')) dy = 100;
    if (keyPressed('a') || keyPressed(',')) dx = -100;
    if (keyPressed('d') || keyPressed('/')) dx = 100;

    // 2. IMU 操纵
    if (imu_enabled && M5.Imu.isEnabled() && dx == 0 && dy == 0) {
        M5.Imu.update(); 
        auto imu_data = M5.Imu.getImuData();
        float ay = imu_data.accel.y; 
        float ax = imu_data.accel.x;
        
        if (ay > 0.25f) dy = 100;
        else if (ay < -0.25f) dy = -100;
        if (ax > 0.25f) dx = -100;
        else if (ax < -0.25f) dx = 100;
    }

    _last_keys = M5Cardputer.Keyboard.keysState().word;
}

bool GameInput::_is_key_in_last(char k) {
    for (char c : _last_keys) {
        if (c == k) return true;
    }
    return false;
}
