#ifndef INPUT_H
#define INPUT_H

#ifndef TARGET_STOPWATCH
#include <M5Cardputer.h>
#endif
#include <M5Unified.h>
#include <vector>
#include "BoardConfig.h"

class GameInput {
public:
    GameInput();
    void update();
    
    // 位移读值接口
    int8_t getDx() const { return dx; }
    int8_t getDy() const { return dy; }
    
    // 跨硬件兼容性增强封装
    bool keyPressed(char k);    // 电平触发 (长按)
    bool keyTriggered(char k);  // 边沿触发 (单次)
    
    bool isLeaderboardPressed();
    bool isSpacePressed();
    bool isEnterPressed(); 
    
    void setImuEnabled(bool enabled) { imu_enabled = enabled; }

    // [V70] 触摸接口 (仅 StopWatch 有效，其他设备恒为 false/-1)
    bool touchBegan() const { return touch_began; }     // 本帧刚按下
    int  menuTouchIndex() const { return menu_touch_idx; } // 本帧点中的菜单项 (-1 无)

private:
    int8_t dx, dy;
    bool imu_enabled;

    bool touch_began;
    int  menu_touch_idx;
    bool double_tap;
    uint32_t last_tap_ms;
    float last_tap_x, last_tap_y;

    // 状态快照，用于模拟 wasPressed
    std::vector<char> _last_keys;
    bool _is_key_in_last(char k);
};

#endif // INPUT_H
