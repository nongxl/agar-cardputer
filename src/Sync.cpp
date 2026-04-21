#include "Sync.h"
#include <Arduino.h>
#include <algorithm>

SyncManager::SyncManager() : current_tick(0), last_tick_ms(0) {}

void SyncManager::updateTick() {
    if (millis() - last_tick_ms >= 50) {
        current_tick++;
        last_tick_ms = millis();
    }
}

bool SyncManager::addPendingEat(uint8_t eater, uint8_t victim, uint16_t tick) {
    // 检查是否已有冲突事件 (同一受害者)
    for (auto& event : pending_eats) {
        if (event.victim_id == victim) {
            // 规则：player_id 大者优先 (authoritative)
            if (eater > event.eater_id) {
                event.eater_id = eater;
                event.tick = tick;
                event.start_time = millis(); // 重置确认时间
                return true;
            } else {
                return false; // 丢弃低优先级事件
            }
        }
    }
    
    // 新增待确认事件
    pending_eats.push_back({eater, victim, tick, millis(), false});
    return true;
}

void SyncManager::checkConfirmations(void (*onConfirm)(uint8_t, uint8_t)) {
    unsigned long now = millis();
    
    // 使用迭代器安全删除
    for (auto it = pending_eats.begin(); it != pending_eats.end(); ) {
        if (now - it->start_time >= CONFIRM_DELAY_MS) {
            // 时间到，确认吞噬
            if (onConfirm) {
                onConfirm(it->eater_id, it->victim_id);
            }
            it = pending_eats.erase(it);
        } else {
            ++it;
        }
    }
}
