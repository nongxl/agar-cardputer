#include "GameState.h"
#include <M5Cardputer.h>
#include <Arduino.h>
#include <string.h>

const char* name_pool[] = {
    "Blob", "Mercury", "Neon", "Void", "Echo", 
    "Aura", "Spark", "M5", "S3", "Atom",
    "Dust", "Core", "Flux", "Nova", "Mars",
    "Zeta", "Pulsar", "Orbit", "Apex", "Zenith"
};
const int NAME_POOL_SIZE = sizeof(name_pool) / sizeof(name_pool[0]);

GameState::GameState() : local_player_id(0), current_seed(0), current_tick(0), last_decay_tick(0), is_initialized(false), is_demo_mode(false) {
    memset(players, 0, sizeof(players));
    memset(food, 0, sizeof(food));
    memset(viruses, 0, sizeof(viruses));
    memset(pellets, 0, sizeof(pellets));
    memset(current_dx, 0, sizeof(current_dx));
    memset(current_dy, 0, sizeof(current_dy));
}

uint16_t GameState::getRGBfromHue(uint16_t hue) {
    float h = hue % 360;
    float s = 0.85f; 
    float v = 0.95f; 
    
    float c = v * s;
    float x = c * (h / 60.0f - floor(h / 60.0f)); 
    float r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    float m = v - c;
    return ((uint16_t)((r + m) * 31) << 11) | ((uint16_t)((g + m) * 63) << 5) | (uint16_t)((b + m) * 31);
}

void GameState::init(uint8_t local_id, const char* name, uint32_t seed) {
    local_player_id = local_id;
    current_seed = seed;
    
    for (int i = 0; i < MAX_VIRUSES; i++) {
        uint32_t v_hash = seed ^ ((uint32_t)i * 0xAAAAAAAA);
        bool safe = false;
        int attempts = 0;
        int32_t vx, vy, vr;
        
        while (!safe && attempts++ < 30) {
            vx = 100 + (v_hash % (MAP_WIDTH - 200));
            vy = 100 + ((v_hash >> 5) % (MAP_HEIGHT - 200));
            // [V26.1] 极致型号范围：5px 到 85px，让小球同样面临袖珍地雷的威胁
            vr = 5 + (v_hash % 81);
            safe = true;
            // [V26] 间距缩短：允许密集堆叠，间距 50px
            for (int j = 0; j < i; j++) {
                int32_t dx = vx - viruses[j].x;
                int32_t dy = vy - viruses[j].y;
                if (dx*dx + dy*dy < 50 * 50) {
                    safe = false;
                    v_hash = (v_hash * 1103515245) + 12345;
                    break;
                }
            }
        }
        viruses[i].x = (uint16_t)vx;
        viruses[i].y = (uint16_t)vy;
        viruses[i].radius = (uint8_t)vr;
        viruses[i].spike_count = 8 + (v_hash % 13);
        viruses[i].spike_length = 20 + (v_hash % 61); // [V26.2] 20-80px 随机刺长
        viruses[i].color = (v_hash * 67) % 360;
        viruses[i].active = true;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].id = i;
        players[i].active = false;
        players[i].respawn_tick = 0;
        
        uint32_t p_hash = seed ^ ((uint32_t)i * 0x55555555);
        bool safe = false;
        int attempts = 0;
        while (!safe && attempts++ < 20) {
            players[i].x = 200 + (p_hash % (MAP_WIDTH - 400));
            players[i].y = 200 + ((p_hash >> 4) % (MAP_HEIGHT - 400));
            safe = true;
            for (int v = 0; v < MAX_VIRUSES; v++) {
                int32_t dx = players[i].x - viruses[v].x, dy = players[i].y - viruses[v].y;
                int32_t safeR = 15 + viruses[v].radius + 60; 
                if (dx*dx + dy*dy < safeR * safeR) {
                    safe = false; p_hash = (p_hash * 1103515245) + 12345; break;
                }
            }
        }
        players[i].radius = 15;
        players[i].score = 0;
        players[i].color = (p_hash * 137) % 360;
        
        int nameIdx = ((i * 3) + (p_hash % 10)) % NAME_POOL_SIZE;
        strncpy(players[i].name, name_pool[nameIdx], 7);
        players[i].name[7] = '\0';
        real_radius[i] = 15.0f;
        realX[i] = (float)players[i].x;
        realY[i] = (float)players[i].y;
        prevX[i] = realX[i];
        prevY[i] = realY[i];
    }
    
    players[local_id].active = true;
    players[local_id].respawn_tick = 40; // 初始降生 2 秒无敌
    
    // [V29] 初始化能量池与状态
    world_mass_pool = 36000;
    gameOver = false;
    winner_id = 0;
    last_decay_tick = 0;
    current_tick = 0;

    for (int i = 0; i < MAX_FOOD; i++) spawnFood(i);
    
    is_initialized = true;
}

void GameState::reset() {
    is_initialized = false;
    current_seed = 0;
    gameOver = false;
    world_mass_pool = 0;
    
    memset(players, 0, sizeof(players));
    memset(current_dx, 0, sizeof(current_dx));
    memset(current_dy, 0, sizeof(current_dy));
    memset(food, 0, sizeof(food));
    memset(viruses, 0, sizeof(viruses));
    memset(pellets, 0, sizeof(pellets));
    memset(ai_decision_timer, 0, sizeof(ai_decision_timer));
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        real_radius[i] = 15.0f;
        realX[i] = 0;
        realY[i] = 0;
        prevX[i] = 0;
        prevY[i] = 0;
    }
}

void GameState::spawnFood(int index) {
    if (world_mass_pool == 0) return; // [V29] 能量池枯竭，停止刷新

    uint32_t f_hash = current_seed ^ ((uint32_t)index * 0xDEADBEEF);
    // [V23] 扩充食物规格至 1 - 40
    int32_t size = 1 + (f_hash % 40);
    
    // [V29] 确保不超支并扣除
    if (world_mass_pool < (uint32_t)size) size = (int32_t)world_mass_pool;
    world_mass_pool -= size;
    
    food[index].size = (uint8_t)size;
    food[index].color = (index * 47) % 360;
    
    int32_t margin = size + 5;
    bool safe = false;
    int attempts = 0;
    while (!safe && attempts++ < 15) {
        food[index].x = margin + (f_hash % (MAP_WIDTH - margin * 2));
        food[index].y = margin + ((f_hash >> 4) % (MAP_HEIGHT - margin * 2));
        safe = true;
        // 注意：食物现在很大，必须严格避开刺球的所有判定面 (Radius + SpikeLength + 5px 留白)
        for (int v = 0; v < MAX_VIRUSES; v++) {
            int32_t dx = (int32_t)food[index].x - (int32_t)viruses[v].x;
            int32_t dy = (int32_t)food[index].y - (int32_t)viruses[v].y;
            int32_t v_total_r = viruses[v].radius + viruses[v].spike_length + 5 + food[index].size; 
            if (dx*dx + dy*dy < v_total_r * v_total_r) {
                safe = false;
                f_hash = (f_hash * 1103515245) + 12345;
                break;
            }
        }
    }
    food[index].active = true;
}

void GameState::updateLocalInput(int8_t dx, int8_t dy) {
    if (!is_initialized) return;
    current_dx[local_player_id] = dx;
    current_dy[local_player_id] = dy;
}

void GameState::updateRemoteInput(uint8_t id, int8_t dx, int8_t dy, uint16_t x, uint16_t y, uint16_t color, uint16_t score, const char* name) {
    if (id < MAX_PLAYERS && id != local_player_id) {
        current_dx[id] = dx;
        current_dy[id] = dy;
        
        // [V16.1] 恢复 Master 端的自动激活逻辑
        // 如果该 ID 玩家当前不活跃且不在重生冷却期，只要收到输入包就立即激活
        if (!players[id].active && players[id].respawn_tick == 0) {
            players[id].active = true;
            players[id].respawn_tick = 40; // 远端客机接入时 2 秒无敌
            if (color != 0) players[id].color = color;
            if (name != nullptr && strlen(name) > 0) strncpy(players[id].name, name, 7);
            // 房主不强刷坐标，允许房客首帧同步坐标
            players[id].x = x;
            players[id].y = y;
            realX[id] = (float)x;
            realY[id] = (float)y;
            real_radius[id] = 15.0f;
            players[id].radius = 15;
            players[id].score = score; // [V55] 使用上报的分数
            updateRadiusFromScore(id); // [V59] 房主强制刷新房客的物理半径
        } else if (players[id].active) {
            // [V59] 活跃期间也允许分数同步更新体型
            players[id].score = score;
            updateRadiusFromScore(id);
            int32_t distDx = (int32_t)players[id].x - (int32_t)x;
            int32_t distDy = (int32_t)players[id].y - (int32_t)y;
            if (distDx*distDx + distDy*distDy < 10000) {
                players[id].x = x;
                players[id].y = y;
                realX[id] = (float)x;
                realY[id] = (float)y;
            }
        }
    }
}

void GameState::applyMovement(uint8_t id, int8_t dx, int8_t dy) {
    if (id >= MAX_PLAYERS) return;
    if (dx == 0 && dy == 0) return;

    Player& p = players[id];
    float speedScale = 65.0f / ((float)p.radius + 35.0f);
    if (speedScale < 0.8f) speedScale = 0.8f; 
    if (speedScale > 2.2f) speedScale = 2.2f;

    float inputScale = 1.0f;
    if (abs(dx) > 50 || abs(dy) > 50) inputScale = 0.85f; 

    if (!p.active) {
        speedScale = 2.5f; 
    }

    const float BASE_SPEED = 7.0f;
    float nextX = realX[id] + ((float)dx / 100.0f * speedScale * BASE_SPEED * inputScale);
    float nextY = realY[id] + ((float)dy / 100.0f * speedScale * BASE_SPEED * inputScale);
    
    float bound = (float)(p.active ? p.radius : 0); 
    if (nextX < bound) nextX = bound;
    if (nextX > (float)MAP_WIDTH - bound) nextX = (float)MAP_WIDTH - bound;
    if (nextY < bound) nextY = bound;
    if (nextY > (float)MAP_HEIGHT - bound) nextY = (float)MAP_HEIGHT - bound;

    realX[id] = nextX;
    realY[id] = nextY;
    p.x = (uint16_t)realX[id];
    p.y = (uint16_t)realY[id];
}

uint8_t GameState::tick(bool is_master) {
    // [V39] 快照各角色当前物理位置。用于 Render 层在逻辑帧之间进行平滑插值渲染。
    for (int i = 0; i < MAX_PLAYERS; i++) {
        prevX[i] = realX[i];
        prevY[i] = realY[i];
    }
    
    if (!is_initialized) return 255;
    current_tick++;
    
    bool shouldDecay = (current_tick - last_decay_tick > 100);
    if (shouldDecay) last_decay_tick = current_tick;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        // [V20] 允许所有玩家处理运动，含非 active 进行相机的幽灵移动
        applyMovement(i, current_dx[i], current_dy[i]);
        if (is_demo_mode && is_master) {
            updateDemoAI(i);
        }
        if (players[i].active) {
            if (players[i].respawn_tick > 0) players[i].respawn_tick--;
            if (is_master && shouldDecay && players[i].score > 20) {
                // [V60] 统一公式衰减：直接减少分数，再通过统一接口更新半径
                // 每 100 帧（约 5 秒）减少 1% 的质量，模拟能量消耗
                players[i].score = (uint16_t)((float)players[i].score * 0.99f); 
                updateRadiusFromScore(i);
            }
        } else if (is_master) {
            if (players[i].respawn_tick > 0) {
                players[i].respawn_tick--;
                if (players[i].respawn_tick == 0) {
                    uint32_t r_hash = current_seed ^ (current_tick + i);
                    bool safe = false;
                    int attempts = 0;
                    while (!safe && attempts++ < 20) {
                        players[i].x = 400 + (r_hash % (MAP_WIDTH - 800));
                        players[i].y = 400 + ((r_hash >> 4) % (MAP_HEIGHT - 800));
                        safe = true;
                        for (int v = 0; v < MAX_VIRUSES; v++) {
                            int32_t dx = players[i].x - viruses[v].x, dy = players[i].y - viruses[v].y;
                            int32_t safeR = 15 + viruses[v].radius + 60;
                            if (dx*dx + dy*dy < safeR * safeR) {
                                safe = false; r_hash = (r_hash * 1103515245) + 12345; break;
                            }
                        }
                    }
                    players[i].radius = 15;
                    players[i].score = 0;
                    real_radius[i] = 15.0f;
                    realX[i] = (float)players[i].x;
                    realY[i] = (float)players[i].y;
                    players[i].active = true; 
                    players[i].respawn_tick = 40; // 重生 2 秒无敌
                }
            }
        }
    }

    if (is_master) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!players[i].active) continue;
            for (int j = 0; j < MAX_PLAYERS; j++) {
                if (i == j || !players[j].active) continue;
                if (players[j].respawn_tick > 0) continue; // 小球具有免疫期，无法被吃
                if (players[i].radius > (float)players[j].radius * 1.15f) {
                    if (checkCollision(players[i], players[j])) {
                        uint32_t victim_mass = (uint32_t)players[j].score + 20;
                        players[i].score += (victim_mass * 7) / 10;
                        updateRadiusFromScore(i); // [V59] 统一使用公式重算
                        
                        spawnPellets(players[j].x, players[j].y, (victim_mass * 3) / 10, players[j].color);
                        players[j].active = false;
                        players[j].respawn_tick = 200; 
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        if (!is_master && i != local_player_id) continue;

        for (int v = 0; v < MAX_VIRUSES; v++) {
            if (!viruses[v].active) continue;

            int32_t dx = (int32_t)players[i].x - (int32_t)viruses[v].x;
            int32_t dy = (int32_t)players[i].y - (int32_t)viruses[v].y;
            uint32_t distSq = dx*dx + dy*dy;
            
            int32_t v_core_r = viruses[v].radius;
            int32_t v_total_r = v_core_r + viruses[v].spike_length; // [V26.2] 使用动态刺长
            uint32_t rTotal = players[i].radius + v_total_r;
            
            // 第一步：快速筛选。如果连最外圈包络圆都没进，忽略
            if (distSq > (rTotal + 10) * (rTotal + 10)) continue;

            float dist = sqrt(distSq);
            if (dist < 1.0f) { dist = 1.0f; dx = 1; dy = 0; }
            
            bool hit = false;
            float pushTarget = dist; // 默认不推开

            // [V25] 高精度多矢量针刺碰撞判定
            if (dist < (float)(players[i].radius + v_core_r)) {
                // 1. 撞到核心圆：直接引爆并阻挡
                hit = true;
                pushTarget = (float)players[i].radius + v_core_r + 1.0f;
            } else {
                // 2. 针刺判定。通过极坐标反推
                float p_angle = atan2(dy, dx);
                int spikes = viruses[v].spike_count;
                for (int n = 0; n < spikes; n++) {
                    float s_angle = (n * (2.0f * PI / spikes));
                    float diff = abs(p_angle - s_angle);
                    if (diff > PI) diff = 2.0f * PI - diff;
                    
                    // 计算圆心到该针刺直线的垂直距离
                    // 线段在投影轴上的长度投影 pDist = dist * cos(diff)
                    // 线段在垂直轴上的距离 d = dist * sin(diff)
                    float dSide = dist * sin(diff);
                    float dForward = dist * cos(diff);
                    
                    // [V27] 修正：判定带向外延伸 PlayerRadius，确保针尖“擦边即炸”
                    if (dSide < (float)players[i].radius + 1.5f && dForward > (float)v_core_r && dForward < (float)v_total_r + (float)players[i].radius) {
                        hit = true;
                        pushTarget = dist + 1.0f; // 阻挡其继续深入
                        break;
                    }
                }
            }

            // 无论玩家多大，只要触碰到任何针刺或核心，触碰即炸
            bool should_explode = (players[i].respawn_tick == 0);

            if (should_explode && hit) {
                triggerVirusExplosion(i, players[i].x, players[i].y);
                players[i].respawn_tick = 40; 
                // [V27] 震撼弹飞效果：85px，并立即执行坐标限幅，彻底根除溢出瞬移
                float boomTarget = players[i].radius + v_total_r + 85.0f; 
                int32_t targetX = viruses[v].x + (int32_t)((dx / dist) * boomTarget);
                int32_t targetY = viruses[v].y + (int32_t)((dy / dist) * boomTarget);
                if (targetX < 0) targetX = 0; if (targetX >= MAP_WIDTH) targetX = MAP_WIDTH - 1;
                if (targetY < 0) targetY = 0; if (targetY >= MAP_HEIGHT) targetY = MAP_HEIGHT - 1;
                players[i].x = targetX;
                players[i].y = targetY;
                realX[i] = (float)targetX;
                realY[i] = (float)targetY;
            } else if (hit && players[i].respawn_tick == 0) {
                // 仅阻挡
                players[i].x = viruses[v].x + (int32_t)((dx / dist) * pushTarget);
                players[i].y = viruses[v].y + (int32_t)((dy / dist) * pushTarget);
                realX[i] = (float)players[i].x;
                realY[i] = (float)players[i].y;
            }
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        if (!is_master && i != local_player_id) continue; 

        for (int f = 0; f < MAX_FOOD; f++) {
            // [V23] 捕食逻辑：玩家半径必须 > 食物半径才能吞噬
            if (food[f].active && players[i].radius > (food[f].size + 1) && checkFoodCollision(players[i], food[f])) {
                players[i].score += food[f].size;
                updateRadiusFromScore(i); // [V59] 无论主从，本地先行反馈体型变化
                food[f].active = false;
            }
        }

        for (int p = 0; p < MAX_PELLETS; p++) {
            if (pellets[p].active && checkPelletCollision(players[i], pellets[p])) {
                players[i].score += pellets[p].size;
                updateRadiusFromScore(i); // [V59] 统一更新
                pellets[p].active = false;
            }
        }
    } // [V27 FIX] 关闭 player 循环

    // [V29] 终局判定逻辑：池干且地图无物
    if (is_master && !gameOver && world_mass_pool == 0) {
        bool any_active = false;
        for (int i = 0; i < MAX_FOOD; i++) if (food[i].active) { any_active = true; break; }
        if (!any_active) {
            for (int i = 0; i < MAX_PELLETS; i++) if (pellets[i].active) { any_active = true; break; }
        }
        
        if (!any_active) {
            gameOver = true;
            uint16_t maxScore = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].active && players[i].score >= maxScore) {
                    maxScore = players[i].score;
                    winner_id = i;
                }
            }
        }
    }

    return 255;
}

uint32_t GameState::getTotalWorldMass() {
    uint32_t total = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) if (players[i].active) total += players[i].score;
    for (int i = 0; i < MAX_FOOD; i++) if (food[i].active) total += food[i].size;
    for (int i = 0; i < MAX_PELLETS; i++) if (pellets[i].active) total += pellets[i].size;
    return total;
}

void GameState::syncState(const StateMsg& msg) {
    if (current_seed == 0 && msg.world_seed != 0) init(local_player_id, "", msg.world_seed);
    setFoodBits(msg.food_bits);
    
    for (int i = 0; i < MAX_STATE_PELLETS; i++) {
        pellets[i] = msg.pellets[i];
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        // [V62] 同步机制重构：权威死亡判定与位移对齐
        bool was_inactive = !players[i].active;
        players[i].respawn_tick = msg.players[i].respawn_tick;
        
        // 只有当房主说你不活跃，且并未给出重生倒计时时（即：房主还没收到你的 JoinReq），才进行生存保护
        if (i == local_player_id && players[i].active && !msg.players[i].active && msg.players[i].respawn_tick == 0) {
            // 保留本地的活跃状态，继续等待房主确认
        } else {
            players[i].active = msg.players[i].active;
        }
        
        // [V55] 强制同步得分与属性
        players[i].score = msg.players[i].score;
        players[i].radius = msg.players[i].radius;
        players[i].color = msg.players[i].color;
        strncpy(players[i].name, msg.players[i].name, 7);

        if (players[i].active) {
            if (i == local_player_id) {
                // 本地同步逻辑... (略)
                int dx = abs((int)players[i].x - (int)msg.players[i].x);
                int dy = abs((int)players[i].y - (int)msg.players[i].y);
                if (dx > 120 || dy > 120) {
                    players[i].x = msg.players[i].x;
                    players[i].y = msg.players[i].y;
                    realX[i] = (float)players[i].x;
                    realY[i] = (float)players[i].y;
                    prevX[i] = realX[i]; 
                }
                players[i].radius = msg.players[i].radius; // 显式更新本地半径
                real_radius[i] = (float)players[i].radius;
                // [V64] 房客本地 ID 不同步速度，以手感为主
            } else {
                players[i] = msg.players[i];
                real_radius[i] = (float)players[i].radius;
                
                // [V64] 接收远程速度并缓存，用于本地 tick() 中的航位预测
                current_dx[i] = msg.players[i].dx;
                current_dy[i] = msg.players[i].dy;

                // [V62] 如果是刚激活或者位置突变，强制重置插值锚点
                if (was_inactive || abs(realX[i] - players[i].x) > 10) {
                    realX[i] = (float)players[i].x;
                    realY[i] = (float)players[i].y;
                    prevX[i] = realX[i];
                    prevY[i] = realY[i]; 
                }
            }
        }
    }
}

void GameState::getFoodBits(uint8_t* bits) {
    memset(bits, 0, 13);
    for (int i = 0; i < MAX_FOOD; i++) { if (food[i].active) bits[i / 8] |= (1 << (i % 8)); }
    // [V29] 利用 food_bits 结尾多余的 4 位同步终局状态
    if (gameOver) {
        bits[12] |= 0x10; // Bit 100: GameOver
        bits[12] |= (winner_id & 0x03) << 5; // Bit 101-102: WinnerID
    }
}

void GameState::setFoodBits(const uint8_t* bits) {
    for (int i = 0; i < MAX_FOOD; i++) {
        bool shouldBeActive = (bits[i / 8] & (1 << (i % 8)));
        if (!shouldBeActive && food[i].active) food[i].active = false;
        else if (shouldBeActive && !food[i].active) spawnFood(i);
    }
    // [V29] 解析终局同步状态
    gameOver = (bits[12] & 0x10) != 0;
    if (gameOver) {
        winner_id = (bits[12] >> 5) & 0x03;
    }
}

void GameState::spawnPellets(uint16_t x, uint16_t y, uint32_t mass, uint16_t color) {
    if (mass == 0) return;

    // [V58] 视觉分片：增加基础片数，让爆炸看起来更密集
    int count = mass / 8 + 10;
    if (count > MAX_PELLETS / 2) count = MAX_PELLETS / 2;
    if (count < 12) count = 12; 

    uint32_t remaining_mass = mass;
    int p_idx = 0;
    
    for (int i = 0; i < count; i++) {
        while(p_idx < MAX_PELLETS && pellets[p_idx].active) p_idx++;
        if (p_idx >= MAX_PELLETS) break;

        uint32_t r_hash = current_seed ^ (current_tick + i);
        uint8_t individualSize = 2; 

        // [V58] 阶梯质量分配 (基于确定性哈希)
        if (i < 4 && remaining_mass > (mass / 3)) {
            // 阶段一：4 枚大主碎片，占据剩余质量的约 10%~15%
            uint32_t portion = (remaining_mass * (10 + (r_hash % 6))) / 100;
            individualSize = (portion > 255) ? 255 : (uint8_t)portion;
        } else if (i < 12 && remaining_mass > 10) {
            // 阶段二：8 枚中等碎片，每枚约占总重的 3%~5%
            uint32_t portion = (mass * (3 + (r_hash % 3))) / 100;
            if (portion > remaining_mass) portion = remaining_mass;
            individualSize = (uint8_t)portion;
        } else {
            // 阶段三：末尾碎屑，填充感，大小控制在 2-6 之间
            individualSize = (remaining_mass / (count - i + 1)) + 1;
            if (individualSize > 6) individualSize = 6;
        }

        if (individualSize < 2) individualSize = 2;
        if (individualSize > remaining_mass) individualSize = (uint8_t)remaining_mass;
        remaining_mass -= individualSize;
        
        // 质量守恒：最后一块吃掉剩余 mass
        if (i == count - 1 && remaining_mass > 0) {
            uint32_t finalAdd = (remaining_mass > 50) ? 50 : remaining_mass;
            if ((uint32_t)individualSize + finalAdd <= 255) individualSize += (uint8_t)finalAdd;
        }

        // [V58] 动力学散射：根据大小调整距离
        float angle = (i * (360.0f / count) + (r_hash % 40) - 20) * (PI / 180.0f);
        // 大碎片的散射半径范围更大 (150-350px)，小碎屑更聚集 (50-200px)
        float base_dist = (individualSize > 30) ? 150.0f : 50.0f;
        float dist = base_dist + (float)(r_hash % 200); 
        
        float px = (float)x + cos(angle) * dist;
        float py = (float)y + sin(angle) * dist;
        
        // 边缘弹球与避障
        if (px < 0) px = -px;
        if (px >= MAP_WIDTH) px = MAP_WIDTH - (px - MAP_WIDTH);
        if (py < 0) py = -py;
        if (py >= MAP_HEIGHT) py = MAP_HEIGHT - (py - MAP_HEIGHT);
        
        for (int v = 0; v < MAX_VIRUSES; v++) {
            if (!viruses[v].active) continue;
            float vdx = px - viruses[v].x; float vdy = py - viruses[v].y;
            float vdSq = vdx*vdx + vdy*vdy;
            float v_bound = (float)viruses[v].radius + viruses[v].spike_length + 5.0f;
            if (vdSq < v_bound * v_bound) {
                float vdist = sqrt(vdSq);
                if (vdist < 1.0f) vdist = 1.0f;
                px = viruses[v].x + (vdx / vdist) * (v_bound + 2.0f);
                py = viruses[v].y + (vdy / vdist) * (v_bound + 2.0f);
            }
        }
        if (px < 0) px = 0; if (px >= MAP_WIDTH) px = MAP_WIDTH - 1;
        if (py < 0) py = 0; if (py >= MAP_HEIGHT) py = MAP_HEIGHT - 1;

        pellets[p_idx].x = (int16_t)px;
        pellets[p_idx].y = (int16_t)py;
        pellets[p_idx].size = individualSize; 
        pellets[p_idx].color = color;
        pellets[p_idx].active = true;
        p_idx++;
    }
}

void GameState::triggerVirusExplosion(uint8_t player_id, uint16_t x, uint16_t y) {
    Player& p = players[player_id];
    uint32_t lost_mass = (uint32_t)p.score; // [V26] 倾家荡产：全量爆破
    p.score = 0; 
    updateRadiusFromScore(player_id); // [V59] 重算半径
    
    // [V31.1] 落实爆炸损耗：触雷后 15% 质量转化为热能损耗，仅喷发 85% 碎肉
    uint32_t spawn_mass = (uint32_t)((float)lost_mass * 0.85f);
    if (spawn_mass > 0) {
        spawnPellets(x, y, spawn_mass, p.color);
    }
}

bool GameState::checkCollision(const Player& a, const Player& b) {
    int32_t dx = (int32_t)a.x - (int32_t)b.x, dy = (int32_t)a.y - (int32_t)b.y;
    int32_t distSq = dx*dx + dy*dy;
    int32_t collisionR = a.radius + b.radius - 4; 
    return distSq < (collisionR * collisionR);
}

bool GameState::checkFoodCollision(const Player& p, const Food& f) {
    if (p.radius <= f.size + 1) return false;
    int32_t dx = (int32_t)p.x - (int32_t)f.x, dy = (int32_t)p.y - (int32_t)f.y;
    int32_t rTotal = (int32_t)p.radius + (int32_t)f.size;
    return (dx*dx + dy*dy) < (rTotal * rTotal);
}

bool GameState::checkVirusCollision(const Player& p, const Virus& v) {
    int32_t dx = (int32_t)p.x - (int32_t)v.x, dy = (int32_t)p.y - (int32_t)v.y;
    uint32_t distSq = dx*dx + dy*dy;
    int32_t v_core_r = v.radius;
    int32_t v_total_r = v_core_r + v.spike_length; 
    
    float dist = sqrt(distSq);
    if (dist < (float)(p.radius + v_core_r)) return true; // 撞核心
    
    // 针刺检测
    float p_angle = atan2(dy, dx);
    int spikes = v.spike_count;
    for (int n = 0; n < spikes; n++) {
        float s_angle = (n * (2.0f * PI / spikes));
        float diff = p_angle - s_angle;
        // [V30] 标准角度归一化 (-PI, PI)
        while (diff > PI) diff -= 2.0f * PI;
        while (diff < -PI) diff += 2.0f * PI;
        diff = abs(diff);
        
        float dSide = abs(dist * sin(diff));
        float dForward = dist * cos(diff);
        
        // [V30] 修正判定：dSide 必须小于半径，且投影点必须在针刺物理覆盖范围内 (核心到针尖)
        if (dSide < (float)p.radius && dForward > (float)v_core_r && dForward < (float)v_total_r) return true;
    }
    return false;
}

bool GameState::checkPelletCollision(const Player& p, const Pellet& l) {
    int32_t dx = (int32_t)p.x - (int32_t)l.x, dy = (int32_t)p.y - (int32_t)l.y;
    int32_t rTotal = (int32_t)p.radius + (int32_t)l.size + 1;
    return (dx*dx + dy*dy) < (rTotal * rTotal);
}

void GameState::updateRadiusFromScore(uint8_t id) {
    if (id >= MAX_PLAYERS) return;
    // [V61] 经典平方根成长公式：Radius = 15 + sqrt(score) * 2.5
    // 这种算法能保证面积增长与质量线性相关，视觉反馈更自然
    float new_r = 15.0f + sqrtf((float)players[id].score) * 2.5f;
    if (new_r < 15.0f) new_r = 15.0f;
    if (new_r > 250.0f) new_r = 250.0f;
    
    real_radius[id] = new_r;
    players[id].radius = (uint8_t)new_r;
}

void GameState::setDemoMode(bool enable) {
    is_demo_mode = enable;
    if (enable) {
        // [V43] 演示模式驱动：随机化 AI 初始体型，确保产生吞噬对战
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (i == local_player_id) continue;
            players[i].active = true;
            players[i].score = 100 + (rand() % 700); // 随机分配 100-800 分
            updateRadiusFromScore(i); // [V59] 使用统一接口
            players[i].respawn_tick = 0;
            ai_decision_timer[i] = 0;
            updateDemoAI(i); 
        }
    }
}

void GameState::updateDemoAI(uint8_t id) {
    Player& p = players[id];
    if (!p.active) return;

    // [V36] 决策阻尼：每 10 帧才重新计算一次寻径
    if (ai_decision_timer[id] > 0) {
        ai_decision_timer[id]--;
        return;
    }
    ai_decision_timer[id] = 10; 

    // 1. 寻找最近的可食用食物
    int targetFood = -1;
    float minDistSq = 1e6f;
    for (int i = 0; i < MAX_FOOD; i++) {
        if (food[i].active && p.radius > (food[i].size + 1)) {
            float dx = (float)food[i].x - p.x;
            float dy = (float)food[i].y - p.y;
            float d2 = dx*dx + dy*dy;
            if (d2 < minDistSq) {
                minDistSq = d2;
                targetFood = i;
            }
        }
    }

    float targetAngle = atan2((float)current_dy[id], (float)current_dx[id]);
    bool hasTarget = false;

    if (targetFood != -1) {
        targetAngle = atan2((float)food[targetFood].y - p.y, (float)food[targetFood].x - p.x);
        hasTarget = true;
    }

    // 2. 避开刺球
    for (int v = 0; v < MAX_VIRUSES; v++) {
        if (viruses[v].active) {
            float dx = (float)viruses[v].x - (float)p.x;
            float dy = (float)viruses[v].y - (float)p.y;
            float d2 = dx*dx + dy*dy;
            float limit = p.radius + viruses[v].radius + 40.0f;
            if (d2 < limit * limit) {
                float escapeAngle = atan2(-dy, -dx);
                targetAngle = (hasTarget) ? (targetAngle + escapeAngle*2.0f) / 3.0f : escapeAngle;
                hasTarget = true;
            }
        }
    }

    // 3. [V43] 边缘规避逻辑：防止 AI 卡死在边界
    float wallForceX = 0, wallForceY = 0;
    if (p.x < 150) wallForceX = 1.0f;
    else if (p.x > MAP_WIDTH - 150) wallForceX = -1.0f;
    if (p.y < 150) wallForceY = 1.0f;
    else if (p.y > MAP_HEIGHT - 150) wallForceY = -1.0f;

    if (wallForceX != 0 || wallForceY != 0) {
        float wallAngle = atan2(wallForceY, wallForceX);
        if (!hasTarget) {
            targetAngle = wallAngle;
        } else {
            // 平滑转向，避免生硬抖动
            targetAngle = (targetAngle + wallAngle) / 2.0f;
        }
        hasTarget = true;
    }

    // 3. [V34] 边界排斥：防止卡在墙角
    const float margin = 150.0f;
    if (p.x < margin || p.x > MAP_WIDTH - margin || p.y < margin || p.y > MAP_HEIGHT - margin) {
        float angleToCenter = atan2(MAP_HEIGHT/2.0f - p.y, MAP_WIDTH/2.0f - p.x);
        targetAngle = (hasTarget) ? (targetAngle + angleToCenter) / 2.0f : angleToCenter;
        hasTarget = true;
    }

    // 4. [V34] 静止补偿：如果当前无目标，增加随机运动
    if (!hasTarget) {
        targetAngle += (float)((micros() % 100) - 50) / 500.0f;
        hasTarget = true;
    }

    if (hasTarget) {
        // [V36] 更加平滑的运动插值 (90% 历史权重)
        int8_t target_dx = (int8_t)(cos(targetAngle) * 100);
        int8_t target_dy = (int8_t)(sin(targetAngle) * 100);
        
        current_dx[id] = (current_dx[id] * 9 + target_dx) / 10;
        current_dy[id] = (current_dy[id] * 9 + target_dy) / 10;
    }
}

// [V54] 序列化接口：用于 Master 广播
void GameState::serializeState(StateMsg& msg) {
    msg.type = MSG_TYPE_STATE;
    msg.world_seed = current_seed;
    msg.current_tick = current_tick;
    
    // [V55] 补全食物存活位图
    getFoodBits(msg.food_bits);
    
    // 拷贝所有玩家数据
    for (int i = 0; i < MAX_PLAYERS; i++) {
        msg.players[i] = players[i];
        // [V64] 航位推算补全：将此时此刻的物理速度注入报文
        msg.players[i].dx = current_dx[i];
        msg.players[i].dy = current_dy[i];
    }
    
    memcpy(msg.pellets, pellets, sizeof(msg.pellets)); 
}
