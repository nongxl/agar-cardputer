#ifndef INPUT_H
#define INPUT_H

#include <M5Cardputer.h>
#include <M5Unified.h>
#include <vector>

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

private:
    int8_t dx, dy;
    bool imu_enabled;

    // 状态快照，用于模拟 wasPressed
    std::vector<char> _last_keys;
    bool _is_key_in_last(char k);
};

#endif // INPUT_H
