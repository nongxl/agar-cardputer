#include <M5Cardputer.h>
#include <M5Unified.h>
#include <WiFi.h>
#include "src/GameState.h"
#include "src/Render.h"
#include "src/espnow_comm.h"
#include "src/Sync.h"
#include "src/Input.h"

GameState* g_game = nullptr;
Render* g_renderer = nullptr;
EspNowComm* g_comm = nullptr;
SyncManager* g_sync_mgr = nullptr;
GameInput* g_input = nullptr; // [V56] 输入控制器实例化

bool g_is_master = false;
bool g_game_started = false;
bool g_waiting_for_sync = false;
float g_view_scale = 0.6f;

const int TICK_INTERVAL = 50;   
const int STATE_INTERVAL = 80; 
uint8_t g_selected_menu_idx = 0;    
uint32_t g_confirm_flash_timer = 0; 
uint32_t g_item_flash_timer = 0;    
uint8_t g_slave_macs[MAX_PLAYERS][6]; 
unsigned long g_last_join_req_ms = 0; 
bool g_show_leaderboard = true; 

void onMessageReceived(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (!g_game || !g_sync_mgr || len < 1) return;
    uint8_t type = data[0];

    // [V54] 主机专用：处理联机请求 (Master ID Allocator)
    if (g_is_master && type == MSG_TYPE_JOIN_REQ) {
        int assigned_id = -1;
        // 1. 查找是否已登记过该 MAC
        for (int i = 1; i < MAX_PLAYERS; i++) {
            if (memcmp(g_slave_macs[i], mac_addr, 6) == 0) { assigned_id = i; break; }
        }
        // 2. 新玩家分配槽位 (1-3)
        if (assigned_id == -1) {
            for (int i = 1; i < MAX_PLAYERS; i++) {
                bool is_empty = true;
                for (int j = 0; j < 6; j++) if (g_slave_macs[i][j] != 0) is_empty = false;
                if (is_empty) { memcpy(g_slave_macs[i], mac_addr, 6); assigned_id = i; break; }
            }
        }
        // 3. 定向回复分配确认
        if (assigned_id != -1) {
            IdAssignMsg ack; ack.type = MSG_TYPE_ID_ASSIGN;
            memcpy(ack.target_mac, mac_addr, 6);
            ack.assigned_id = (uint8_t)assigned_id;
            ack.world_seed = g_game->getSeed();
            g_comm->sendTo(mac_addr, (uint8_t*)&ack, sizeof(ack));

            // [V57] 主机预激活：立即在房主端登记该玩家，确保下一个 StateMsg 直接包含其 active 状态
            // 初始分配在地图中心区域 (1000, 1000)，颜色设为 0 (随机)，积分为 0
            g_game->updateRemoteInput((uint8_t)assigned_id, 0, 0, 1000, 1000, 0, 0, "Guest");
        }
        return;
    }

    // [V54] 从机专用：处理 ID 分配确认
    if (!g_is_master && type == MSG_TYPE_ID_ASSIGN && g_waiting_for_sync) {
        IdAssignMsg* msg = (IdAssignMsg*)data;
        uint8_t my_mac[6]; WiFi.macAddress(my_mac);
        if (memcmp(msg->target_mac, my_mac, 6) == 0) {
            // 接纳 ID 并同步房间种子 (确保地图一致)
            g_game->init(msg->assigned_id, "Guest", msg->world_seed);
        }
        return;
    }

    if (type == MSG_TYPE_INPUT && len >= (int)sizeof(InputMsg)) {
        if (g_is_master && g_game_started) {
            InputMsg* msg = (InputMsg*)data;
            if (msg->player_id < MAX_PLAYERS && msg->player_id != g_game->getLocalId()) {
                g_game->updateRemoteInput(msg->player_id, msg->dx, msg->dy, msg->x, msg->y, msg->color, msg->score, msg->name);
            }
        }
    } else if (type == MSG_TYPE_STATE && len >= (int)sizeof(StateMsg)) {
        // [V63] 处理来自 Master 的世界状态广播
        if (!g_is_master) { 
            StateMsg* msg = (StateMsg*)data;
            if (g_waiting_for_sync) {
                // 首次加入时的全量同步与初始化
                if (g_game->getLocalId() != 0) {
                    g_game->setDemoMode(false); 
                    g_game->reset();            
                    g_game->syncState(*msg);
                    g_game_started = true; 
                    g_waiting_for_sync = false;
                }
            } else {
                // 游戏过程中的持续状态刷新
                g_game->syncState(*msg);
            }
        }
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5Cardputer.begin(cfg, true);
    M5.Display.setRotation(1);
    
    // [V56] 启动硬件 IMU
    M5.Imu.init();
    
    g_game = new GameState();
    g_renderer = new Render();
    g_renderer->init(&M5.Display);
    g_comm = new EspNowComm();
    g_sync_mgr = new SyncManager();
    g_input = new GameInput(); // [V56]
    
    memset(g_slave_macs, 0, sizeof(g_slave_macs));
    g_game->setDemoMode(true);
    g_game->init(0, "Demo", 12345);
}

void loop() {
    M5Cardputer.update();
    g_input->update(); // [V56] 关键：每帧采集硬件状态
    
    unsigned long now = millis();
    static uint32_t lastTickTime = 0;
    static uint32_t lastStateTime = 0;

    if (!g_game_started) {
        if (now - lastTickTime >= TICK_INTERVAL) {
            g_game->tick(true); 
            lastTickTime = now;
        }
        float alpha = (float)(millis() - lastTickTime) / (float)TICK_INTERVAL;
        if (alpha > 1.0f) alpha = 1.0f;
        g_renderer->draw(*g_game, false, alpha, g_view_scale);
        g_renderer->drawMenuUI(*g_game, g_selected_menu_idx, 
                               (g_confirm_flash_timer > 0 && millis() < g_confirm_flash_timer),
                               (g_item_flash_timer > 0));
        
        // [V54] 菜单交互逻辑
        unsigned long now_ms = millis();
        if (g_item_flash_timer > 0 && now_ms >= g_item_flash_timer) {
            bool is_master = (g_selected_menu_idx == 0);
            if (is_master) {
                g_game->setDemoMode(false);
                g_is_master = true;
                g_game->reset(); 
                g_game_started = true;
                g_comm->init();
                g_comm->setNotifyCallback(onMessageReceived);
                uint8_t mac_info[6]; WiFi.macAddress(mac_info);
                uint32_t world_seed = (uint32_t)mac_info[5] + millis();
                g_game->init(0, "Master", world_seed); 
                memset(g_slave_macs, 0, sizeof(g_slave_macs));
                lastTickTime = millis();
                lastStateTime = millis();
            } else {
                g_waiting_for_sync = true;
                g_is_master = false;
                g_comm->init();
                g_comm->setNotifyCallback(onMessageReceived);
                g_last_join_req_ms = 0;
            }
            g_item_flash_timer = 0; 
        }

        // [V56] 使用规范化的按键控制
        if (g_waiting_for_sync) {
            if (millis() - g_last_join_req_ms > 1000) {
                g_last_join_req_ms = millis();
                JoinReqMsg req; req.type = MSG_TYPE_JOIN_REQ;
                WiFi.macAddress(req.mac);
                g_comm->broadcast((uint8_t*)&req, sizeof(req));
            }
            if (g_input->keyPressed(0x08) || g_input->keyPressed(0x7F)) {
                g_waiting_for_sync = false;
                g_game->init(0, "Demo", 0);
                delay(200);
            }
            g_renderer->drawConnectingOverlay(true);
        } else if (g_item_flash_timer == 0) { 
            if (g_input->keyPressed(';')) {
                if (g_selected_menu_idx > 0) g_selected_menu_idx--;
                delay(150); 
            } else if (g_input->keyPressed('.')) {
                if (g_selected_menu_idx < 1) g_selected_menu_idx++;
                delay(150);
            } else if (g_input->isEnterPressed()) { 
                if (g_confirm_flash_timer == 0 && g_item_flash_timer == 0) {
                    g_item_flash_timer = millis() + 400; 
                }
            }
        }
    } else {
        // [V56] 切换显隐开关
        if (g_input->isLeaderboardPressed()) {
            g_show_leaderboard = !g_show_leaderboard;
        }

        // [V56] 视野缩放控制
        if (g_input->keyPressed('-')) {
            g_view_scale -= 0.02f;
            if (g_view_scale < 0.1f) g_view_scale = 0.1f;
        }
        if (g_input->keyPressed('=')) {
            g_view_scale += 0.02f;
            if (g_view_scale > 1.3f) g_view_scale = 1.3f;
        }

        // [V54] 核心游戏循环 (联机同步控制)
        if (now - lastTickTime >= TICK_INTERVAL) {
            // [V56] 注入同步后的物理位移
            g_game->updateLocalInput(g_input->getDx(), g_input->getDy());
            g_game->tick(g_is_master); 
            lastTickTime = now;
            
            if (!g_is_master) {
                InputMsg msg; msg.type = MSG_TYPE_INPUT;
                msg.player_id = g_game->getLocalId();
                msg.dx = g_game->getLocalDX(); msg.dy = g_game->getLocalDY();
                msg.x = g_game->getLocalPlayerX(); msg.y = g_game->getLocalPlayerY();
                msg.score = g_game->getLocalPlayer()->score; 
                msg.color = g_game->getLocalPlayerColor();
                strncpy(msg.name, g_game->getLocalPlayerName(), 8);
                msg.tick = g_sync_mgr->getTick();
                g_comm->broadcast((uint8_t*)&msg, sizeof(msg));
            }
        }
        if (g_is_master && now - lastStateTime >= STATE_INTERVAL) {
            StateMsg msg; g_game->serializeState(msg);
            g_comm->broadcast((uint8_t*)&msg, sizeof(msg));
            lastStateTime = now;
        }
        float alpha = (float)(millis() - lastTickTime) / (float)TICK_INTERVAL;
        if (alpha > 1.0f) alpha = 1.0f;
        g_renderer->draw(*g_game, g_show_leaderboard, alpha, g_view_scale);
    }
    g_renderer->commit();
}
