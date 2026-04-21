#ifndef SYNC_H
#define SYNC_H

#include "Protocol.h"
#include <vector>

// 挂起的事件结构
struct PendingEat {
    uint8_t eater_id;
    uint8_t victim_id;
    uint16_t tick;
    unsigned long start_time;
    bool confirmed;
};

class SyncManager {
public:
    SyncManager();
    
    // Tick 管理
    void updateTick();
    uint16_t getTick() { return current_tick; }
    
    // 事件过滤与判定
    bool addPendingEat(uint8_t eater, uint8_t victim, uint16_t tick);
    void checkConfirmations(void (*onConfirm)(uint8_t, uint8_t));

private:
    uint16_t current_tick;
    unsigned long last_tick_ms;
    
    std::vector<PendingEat> pending_eats;
    static const int CONFIRM_DELAY_MS = 150;
};

#endif // SYNC_H
