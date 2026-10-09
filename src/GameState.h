#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "Protocol.h"
#include <vector>

// [V14] 恢复全局地图常量
#define MAP_WIDTH 2000
#define MAP_HEIGHT 2000
#define MAX_VIRUSES 20 

// 刺球实体
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t color; 
    uint8_t radius;
    uint8_t spike_count;  // [V25] 随机针刺数量 (8-20)
    uint8_t spike_length; // [V26.2] 随机针刺长度 (20-80px)
    bool active;
} Virus;

class GameState {
public:
    GameState();
    
    void init(uint8_t local_id, const char* name, uint32_t seed = 0);
    void reset(); // [V41] 全局重置接口
    void setLocalId(uint8_t id) { local_player_id = id; }
    
    void updateLocalInput(int8_t dx, int8_t dy);
    void updateRemoteInput(uint8_t id, int8_t dx, int8_t dy, uint16_t x, uint16_t y, uint16_t color, uint16_t score, const char* name);
    
    uint8_t tick(bool is_master);
    
    void handleEatEvent(uint8_t eater_id, uint8_t victim_id);
    void syncState(const StateMsg& msg);
    
    void getFoodBits(uint8_t* bits);
    void setFoodBits(const uint8_t* bits);
    uint32_t getWorldSeed() { return current_seed; }

    Player* getPlayers() { return players; }
    Food* getFood() { return food; }
    Virus* getViruses() { return viruses; } 
    Pellet* getPellets() { return pellets; } 
    
    uint8_t getLocalId() { return local_player_id; }
    Player* getLocalPlayer() { return &players[local_player_id]; }
    
    // [V54] 联机数据获取接口
    uint32_t getSeed() { return current_seed; }
    int8_t getLocalDX() { return current_dx[local_player_id]; }
    int8_t getLocalDY() { return current_dy[local_player_id]; }
    uint16_t getLocalPlayerX() { return players[local_player_id].x; }
    uint16_t getLocalPlayerY() { return players[local_player_id].y; }
    uint16_t getLocalPlayerColor() { return players[local_player_id].color; }
    const char* getLocalPlayerName() { return players[local_player_id].name; }
    
    // [V54] 序列化接口：用于 Master 广播
    void serializeState(StateMsg& msg);

    // [V29] 终局判定接口
    uint32_t getTotalWorldMass();
    bool isGameOver() { return gameOver; }
    uint8_t getWinnerId() { return winner_id; }

    static uint16_t getRGBfromHue(uint16_t hue);
    void updateRadiusFromScore(uint8_t id); // [V59] 统一半径换算接口
    void setDemoMode(bool enable);
    bool getDemoMode() { return is_demo_mode; }
    bool isInitialized() { return is_initialized; } // [V41]
    
    // [V68] 震动请求接口
    uint16_t getVibrationRequest() { 
        uint16_t res = vibration_requested_ms; 
        vibration_requested_ms = 0; 
        return res; 
    }
    void requestVibration(uint16_t ms) {
        if (ms > vibration_requested_ms) vibration_requested_ms = ms;
    }

    // [V38] 亚像素精度访问接口
    float getRealX(uint8_t id) { return (id < MAX_PLAYERS) ? realX[id] : 0; }
    float getRealY(uint8_t id) { return (id < MAX_PLAYERS) ? realY[id] : 0; }
    float getRealRadius(uint8_t id) { return (id < MAX_PLAYERS) ? real_radius[id] : 0; }

    // [V39] 线性插值显示接口
    float getInterpX(uint8_t id, float alpha) { 
        if (id >= MAX_PLAYERS) return 0;
        return prevX[id] + (realX[id] - prevX[id]) * alpha; 
    }
    float getInterpY(uint8_t id, float alpha) { 
        if (id >= MAX_PLAYERS) return 0;
        return prevY[id] + (realY[id] - prevY[id]) * alpha; 
    }

private:
    Player players[MAX_PLAYERS];
    int8_t current_dx[MAX_PLAYERS];
    int8_t current_dy[MAX_PLAYERS];

    Food food[MAX_FOOD];
    Virus viruses[MAX_VIRUSES]; 
    Pellet pellets[MAX_PELLETS]; 
    
    float real_radius[MAX_PLAYERS]; 
    
    uint8_t local_player_id;
    uint32_t current_seed; 
    uint16_t current_tick;
    uint16_t last_decay_tick;  
    bool is_initialized;
    
    // [V29] 终局机制变量
    uint32_t world_mass_pool; 
    bool gameOver;            
    uint8_t winner_id;        
    uint16_t vibration_requested_ms; // [V68]
    
    void applyMovement(uint8_t id, int8_t dx, int8_t dy);
    void spawnFood(int index);
    
    void triggerVirusExplosion(uint8_t player_id, uint16_t x, uint16_t y); 
    void spawnPellets(uint16_t x, uint16_t y, uint32_t mass, uint16_t color);
    
    bool checkCollision(const Player& a, const Player& b);
    bool checkFoodCollision(const Player& p, const Food& f);
    bool checkVirusCollision(const Player& p, const Virus& v);
    bool checkPelletCollision(const Player& p, const Pellet& l);

private:
    bool is_demo_mode;
    void updateDemoAI(uint8_t id);
    uint8_t ai_decision_timer[MAX_PLAYERS]; 
    float realX[MAX_PLAYERS]; // [V38] 亚像素中心点 X
    float realY[MAX_PLAYERS]; // [V38] 亚像素中心点 Y
    float prevX[MAX_PLAYERS]; // [V39] 上一帧快照 X
    float prevY[MAX_PLAYERS]; // [V39] 上一帧快照 Y
};

#endif // GAME_STATE_H
