#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// [V15] 极致负载压缩
// MAX_PLAYERS 定位 4 人制竞技场
#define MAX_PLAYERS 4
#define MAX_FOOD 100
#define MAX_PELLETS 128
#define MAX_STATE_PELLETS 16 

enum MsgType {
    MSG_TYPE_STATE = 1,
    MSG_TYPE_INPUT = 2,
    MSG_TYPE_PELLET = 3,
    MSG_TYPE_JOIN_REQ = 4,   // [V54] 请求加入
    MSG_TYPE_ID_ASSIGN = 5   // [V54] 分配 ID
};

// [V54] 从机请求加入包
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t mac[6];
} JoinReqMsg;

// [V54] 主机分配 ID 包
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t target_mac[6];
    uint8_t assigned_id;
    uint32_t world_seed; // [V54.1] 同时下发随机种子，确保地图一致性
} IdAssignMsg;

// [V15] 强制 packed 布局，杜绝内存对齐导致的 288 溢出
typedef struct __attribute__((packed)) {
    uint16_t x, y;
    uint16_t color;
    uint8_t size;
    bool active;
} Food;

typedef struct __attribute__((packed)) {
    uint8_t id;
    char name[8];
    uint16_t x, y;
    int8_t dx, dy; // [V64] 航位推算：同步下发当前物理速度
    uint16_t color; 
    uint8_t radius;
    bool active;
    uint16_t score;
    uint8_t respawn_tick;
} Player;

typedef struct __attribute__((packed)) {
    uint16_t x, y;
    uint8_t size;
    uint16_t color;
    bool active;
} Pellet;

typedef struct __attribute__((packed)) {
    uint8_t type;         
    uint32_t current_tick; // [V54] 统一命名为 current_tick
    uint32_t world_seed;   // [V54] 统一命名为 world_seed
    uint8_t food_bits[13]; 
    Player players[MAX_PLAYERS]; 
    Pellet pellets[MAX_STATE_PELLETS]; 
} StateMsg;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t player_id;
    int8_t dx, dy;
    uint16_t x, y;
    uint16_t color;
    uint16_t score; // [V55] 增加分数冗余
    char name[8];
    uint32_t tick;
} InputMsg;

#endif // PROTOCOL_H
