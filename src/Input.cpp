#include "Input.h"
#include <Arduino.h>

GameInput::GameInput() : dx(0), dy(0), imu_enabled(true),
    touch_began(false), menu_touch_idx(-1), double_tap(false),
    last_tap_ms(0), last_tap_x(0), last_tap_y(0) {
#ifdef TARGET_STOPWATCH
    imu_enabled = false; // [V70] StopWatch 默认关闭倾斜控制，避免与触摸冲突
#endif
}

bool GameInput::keyPressed(char k) {
    // [V67] 兼容 StickS3 硬件按键映射
#ifndef TARGET_STOPWATCH
    if (M5.getBoard() == m5::board_t::board_M5Cardputer) {
        if (M5Cardputer.Keyboard.isKeyPressed(k)) return true;
        return false;
    }
#endif
    // StickS3 / StopWatch 映射逻辑 (两个物理按键)
    if ((k == '=' || k == '+') && M5.BtnA.isPressed()) return true;
    if ((k == '-' || k == '_') && M5.BtnB.isPressed()) return true;
    
    // [V67] 菜单切换适配：BtnB 仅映射为 '.' (Next) 键，配合 main.cpp 的循环逻辑
    if (k == '.' && M5.BtnB.wasPressed()) return true;
    return false;
}

bool GameInput::keyTriggered(char k) {
    if (keyPressed(k)) {
        for (char c : _last_keys) { if (c == k) return false; }
        return true;
    }
    return false;
}

bool GameInput::isLeaderboardPressed() {
#ifdef TARGET_STOPWATCH
    // [V70] StopWatch：双击屏幕中心切换排行榜 (替代 ESC)
    return double_tap;
#else
    if (M5.getBoard() != m5::board_t::board_M5Cardputer) return false;
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
#endif
}

bool GameInput::isSpacePressed() {
    return keyTriggered(' ');
}

bool GameInput::isEnterPressed() {
#ifndef TARGET_STOPWATCH
    if (M5.getBoard() == m5::board_t::board_M5Cardputer) {
        return M5Cardputer.Keyboard.keysState().enter;
    }
#endif
    return M5.BtnA.wasPressed(); // StickS3 / StopWatch 确认键
}

void GameInput::update() {
    dx = 0;
    dy = 0;
    touch_began = false;
    menu_touch_idx = -1;
    double_tap = false;

#ifdef TARGET_STOPWATCH
    // [V70] 触摸输入：物理坐标 (466x466) 换算为逻辑坐标 (233x233)
    auto t = M5.Touch.getDetail();
    const float k = (float)SCREEN_W / (float)M5.Display.width();
    float tx = t.x * k;
    float ty = t.y * k;

    if (t.wasPressed()) {
        touch_began = true;
        uint32_t now = millis();

        // 菜单项命中检测
        const float half_w = MENU_OPT_W / 2.0f;
        if (fabsf(tx - SCREEN_CX) <= half_w) {
            if (fabsf(ty - MENU_OPT1_Y) <= 18) menu_touch_idx = 0;
            else if (fabsf(ty - MENU_OPT2_Y) <= 18) menu_touch_idx = 1;
        }

        // 双击屏幕中心检测
        float cdx = tx - SCREEN_CX, cdy = ty - SCREEN_CY;
        bool in_center = (cdx * cdx + cdy * cdy) <= (TOUCH_CENTER_RADIUS * TOUCH_CENTER_RADIUS);
        if (in_center && last_tap_ms != 0 && (now - last_tap_ms) < TOUCH_DOUBLE_TAP_MS) {
            double_tap = true;
            last_tap_ms = 0;
        } else {
            last_tap_ms = in_center ? now : 0;
        }
    }

    if (t.isPressed()) {
        // 以屏幕中心 (角色位置) 为参考，取触点方向；死区内不移动
        float vx = tx - SCREEN_CX;
        float vy = ty - SCREEN_CY;
        float dist = sqrtf(vx * vx + vy * vy);
        if (dist > TOUCH_DEADZONE) {
            dx = (int8_t)lroundf(vx / dist * 100.0f);
            dy = (int8_t)lroundf(vy / dist * 100.0f);
        }
    }
    // StopWatch 不使用键盘/IMU 移动
#else
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

    if (M5.getBoard() == m5::board_t::board_M5Cardputer) {
        _last_keys = M5Cardputer.Keyboard.keysState().word;
    }
#endif
}

bool GameInput::_is_key_in_last(char k) {
    for (char c : _last_keys) {
        if (c == k) return true;
    }
    return false;
}
