#include "Render.h"
#include <M5Unified.h>

Render::Render() : _display(nullptr), smoothCamX(0), smoothCamY(0) {
}

void Render::init(M5GFX* display) {
    _display = display;
    _canvas.setColorDepth(16);
    _canvas.createSprite(SCREEN_W, SCREEN_H);
}

void Render::draw(GameState& state, bool show_leaderboard, float alpha, float VIEW_SCALE) {
    if (!_display) return;

    _canvas.fillScreen(0xF7DF); 
    uint8_t local_id = state.getLocalId();

    // 相机位置保护：如果尚未初始化（未收到主数据），相机锁定在地图中心
    float targetX, targetY;
    if (state.isInitialized()) {
        targetX = state.getInterpX(local_id, alpha);
        targetY = state.getInterpY(local_id, alpha);
    } else {
        targetX = MAP_WIDTH / 2.0f;
        targetY = MAP_HEIGHT / 2.0f;
    }

    // [V73] 本地视角相机直接对齐角色高精度插值坐标，消除低通滤波滞后导致的小球与背景前后晃动
    smoothCamX = targetX;
    smoothCamY = targetY;

    float effectiveScale = VIEW_SCALE * VIEW_SCALE_FACTOR;
    float camX = smoothCamX - (SCREEN_CX / effectiveScale);
    float camY = smoothCamY - (SCREEN_CY / effectiveScale);

    int mapLeft = (int)((0.0f - camX) * effectiveScale);
    int mapTopSize = (int)((0.0f - camY) * effectiveScale);
    int mapWidth = (int)((float)MAP_WIDTH * effectiveScale);
    int mapHeight = (int)((float)MAP_HEIGHT * effectiveScale);
    for (int b = 0; b < 2; b++) {
        _canvas.drawRect(mapLeft - b, mapTopSize - b, mapWidth + 2*b, mapHeight + 2*b, TFT_BLACK); 
    }

    // 1. 绘制网格
    _canvas.setClipRect(mapLeft, mapTopSize, mapWidth, mapHeight);
    float gridStep = 50.0f * effectiveScale;
    for (float x = fmod(-camX * effectiveScale, gridStep); x < (float)SCREEN_W; x += gridStep)
        _canvas.drawFastVLine((int)x, 0, SCREEN_H, 0xCE59);
    for (float y = fmod(-camY * effectiveScale, gridStep); y < (float)SCREEN_H; y += gridStep)
        _canvas.drawFastHLine(0, (int)y, SCREEN_W, 0xCE59);
    _canvas.clearClipRect();

    // 2. 绘制食物
    Food* food = state.getFood();
    for (int i = 0; i < MAX_FOOD; i++) {
        if (food[i].active) {
            int relX = (int)(((float)food[i].x - camX) * effectiveScale);
            int relY = (int)(((float)food[i].y - camY) * effectiveScale);
            if (relX > -15 && relX < SCREEN_W + 15 && relY > -15 && relY < SCREEN_H + 15) {
                _canvas.fillCircle(relX, relY, (int)((food[i].size + 1) * effectiveScale + 1), GameState::getRGBfromHue(food[i].color));
            }
        }
    }

    // 3. 绘制动态碎肉
    Pellet* pellets = state.getPellets();
    for (int i = 0; i < MAX_PELLETS; i++) {
        if (pellets[i].active) {
            int relX = (int)(((float)pellets[i].x - camX) * effectiveScale);
            int relY = (int)(((float)pellets[i].y - camY) * effectiveScale);
            if (relX > -15 && relX < SCREEN_W + 15 && relY > -15 && relY < SCREEN_H + 15) {
                _canvas.fillCircle(relX, relY, (int)((float)pellets[i].size * effectiveScale), GameState::getRGBfromHue(pellets[i].color));
            }
        }
    }

    // 4. [V17] 原初几何刺球渲染 (24 齿轮替半径算法)
    Virus* viruses = state.getViruses();
    const uint16_t COLOR_VIRUS_FILL = 0x37E6;   // #33FF33
    const uint16_t COLOR_VIRUS_STROKE = 0x1A23; // #19D119 (深绿)
    
    for (int v = 0; v < MAX_VIRUSES; v++) {
        if (viruses[v].active) {
            int relX = (viruses[v].x - camX) * effectiveScale;
            int relY = (viruses[v].y - camY) * effectiveScale;
            float r = (float)viruses[v].radius * effectiveScale;
            
            if (relX > -100 && relX < SCREEN_W + 100 && relY > -100 && relY < SCREEN_H + 100) {
                uint16_t vColor = GameState::getRGBfromHue(viruses[v].color);
                _canvas.fillCircle(relX, relY, r, vColor);
                _canvas.drawCircle(relX, relY, r, TFT_BLACK);

                int spikes = viruses[v].spike_count;
                for (int i = 0; i < spikes; i++) {
                    float angle = (i * 360.0f / spikes) * (PI / 180.0f);
                    float angle1 = angle - (float)(100.0f / (spikes * r)) * (PI / 180.0f);
                    float angle2 = angle + (float)(100.0f / (spikes * r)) * (PI / 180.0f);
                    
                    float bx1 = relX + cos(angle1) * r;
                    float by1 = relY + sin(angle1) * r;
                    float bx2 = relX + cos(angle2) * r;
                    float by2 = relY + sin(angle2) * r;
                    
                    float sl = viruses[v].spike_length * effectiveScale;
                    float px = relX + cos(angle) * (r + sl);
                    float py = relY + sin(angle) * (r + sl);
                    
                    _canvas.fillTriangle(bx1, by1, bx2, by2, px, py, vColor);
                    _canvas.drawLine(bx1, by1, px, py, TFT_BLACK);
                    _canvas.drawLine(bx2, by2, px, py, TFT_BLACK);
                }
            }
        }
    }

    // 5. 绘制玩家
    Player* players = state.getPlayers();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active) {
            if (players[i].respawn_tick > 0 && (players[i].respawn_tick % 6 < 3)) {
                continue; 
            }
            float pX = state.getInterpX(i, alpha);
            float pY = state.getInterpY(i, alpha);
            int relX = (int)((pX - camX) * effectiveScale);
            int relY = (int)((pY - camY) * effectiveScale);
            int radius = (int)((float)state.getRealRadius(i) * effectiveScale);
            if (relX > -150 && relX < SCREEN_W + 160 && relY > -150 && relY < SCREEN_H + 165) {
                uint16_t color = GameState::getRGBfromHue(players[i].color);
                _canvas.fillCircle(relX, relY, radius, color);
                _canvas.drawCircle(relX, relY, radius + 1, TFT_BLACK);
                
                _canvas.setTextColor(TFT_BLACK); 
                _canvas.setTextDatum(middle_center);
#ifdef BOARD_ROUND_SCREEN
                _canvas.setTextSize(2);
#else
                _canvas.setTextSize(1);
#endif
                _canvas.drawString(players[i].name, relX, relY);
            }
        }
    }

    Player* local = state.getLocalPlayer();
    if (!local->active && local->respawn_tick > 0 && !state.isGameOver()) {
#ifdef BOARD_ROUND_SCREEN
        _canvas.setTextSize(6);
#else
        _canvas.setTextSize(4);
#endif
        _canvas.setTextColor(((millis() / 250) % 2 == 0) ? TFT_RED : TFT_WHITE);
        _canvas.setTextDatum(middle_center);
        _canvas.drawString(String((local->respawn_tick / 20) + 1), (int)SCREEN_CX, (int)SCREEN_CY);
        _canvas.setTextSize(1);
    }

    // 终局胜利画面
    if (state.isGameOver()) {
        uint8_t winId = state.getWinnerId();
        Player* winner = &players[winId];
#ifdef BOARD_ROUND_SCREEN
        _canvas.setTextSize(3);
#else
        _canvas.setTextSize(4);
#endif
        _canvas.setTextColor(GameState::getRGBfromHue(winner->color));
        _canvas.setTextDatum(middle_center);
        char winBuf[32];
        sprintf(winBuf, "%s WINS", winner->name);
        _canvas.drawString(winBuf, (int)SCREEN_CX, (int)SCREEN_CY);
        _canvas.setTextSize(1);
    }

    if (show_leaderboard) drawLeaderboard(state);
    if (!state.getDemoMode()) {
        drawMinimap(state, effectiveScale, camX, camY);
    }
}

void Render::drawConnectingOverlay(bool is_searching) {
    int dotCount = (millis() / 500) % 4;
    char dots[4] = {0};
    for(int i = 0; i < dotCount; i++) dots[i] = '.';

    int cx = (int)SCREEN_CX, cy = (int)SCREEN_CY;
#ifdef BOARD_ROUND_SCREEN
    drawBrushHighlight(cx - 160, cy - 40, 320, 80, 0xFFE0);
    _canvas.setTextColor(TFT_BLACK);
    _canvas.setTextDatum(middle_center);
    _canvas.setTextSize(2);
    
    char msgBuf[64];
    if (is_searching) {
        sprintf(msgBuf, "SEARCHING FOR MASTER%s", dots);
    } else {
        sprintf(msgBuf, "WAITING FOR SYNC DATA%s", dots);
    }
    
    _canvas.drawString(msgBuf, cx, cy - 12);
    _canvas.drawString("TAP TO CANCEL", cx, cy + 14);
#else
    drawBrushHighlight(cx - 95, cy - 22, 190, 44, 0xFFE0);
    _canvas.setTextColor(TFT_BLACK);
    _canvas.setTextDatum(middle_center);
    _canvas.setTextSize(1);
    
    char msgBuf[64];
    if (is_searching) {
        sprintf(msgBuf, "SEARCHING FOR MASTER%s", dots);
    } else {
        sprintf(msgBuf, "WAITING FOR SYNC DATA%s", dots);
    }
    _canvas.drawString(msgBuf, cx, cy);
#endif
}

void Render::commit() {
    if (!_display) return;
    // [V73] 无论何种屏幕，均直接由硬件 DMA 刷屏。圆屏硬件自身裁切四角，视野自然铺满整圆且无黑边
    _canvas.pushSprite(_display, 0, 0);
}

void Render::drawLeaderboard(GameState& state) {
#ifdef BOARD_ROUND_SCREEN
    // 圆屏：排行榜置于顶部圆弧内居中
    const int LB_X = (int)SCREEN_CX;
    const int LB_Y0 = 24;
    const int LB_ROW0 = 60;
    const int rowStep = 18;
    _canvas.setTextDatum(top_center);
    _canvas.setTextSize(2);
#else
    const int LB_X = 170;
    const int LB_Y0 = 5;
    const int LB_ROW0 = 26;
    const int rowStep = 9;
    _canvas.setTextDatum(top_left);
    _canvas.setTextSize(1);
#endif
    _canvas.setTextColor(TFT_BLACK);
    char totalBuf[32];
    sprintf(totalBuf, "T_MASS:%u", state.getTotalWorldMass());
    _canvas.drawString(totalBuf, LB_X, LB_Y0);
    _canvas.setTextColor(TFT_BLACK);
#ifdef BOARD_ROUND_SCREEN
    _canvas.drawString("== TOP ==", LB_X, LB_Y0 + 16);
    _canvas.drawLine(LB_X - 55, LB_Y0 + 32, LB_X + 55, LB_Y0 + 32, TFT_DARKGREY);
#else
    _canvas.drawString("== TOP ==", LB_X, LB_Y0 + 9);
    _canvas.drawLine(170, 23, 235, 23, TFT_DARKGREY);
#endif
    
    Player* players = state.getPlayers();
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active) {
            uint16_t color = GameState::getRGBfromHue(players[i].color);
            _canvas.setTextColor(color);
            char buf[32];
            sprintf(buf, "%s: %d", players[i].name, players[i].score);
            _canvas.drawString(buf, LB_X, LB_ROW0 + (count++) * rowStep);
        } else if (players[i].respawn_tick > 0) {
            _canvas.setTextColor(((millis() / 300) % 2 == 0) ? TFT_RED : TFT_DARKGREY);
            char buf[32];
            sprintf(buf, "%s: %ds", players[i].name, (players[i].respawn_tick / 20) + 1);
            _canvas.drawString(buf, LB_X, LB_ROW0 + (count++) * rowStep);
        }
    }
}

void Render::drawMinimap(GameState& state, float view_scale, float camX, float camY) {
#ifdef BOARD_ROUND_SCREEN
    int mapSize = 70;
    int startX = 16;                            // 圆屏：左侧安全区 (X: 16~86)
    int startY = (int)SCREEN_CY - mapSize / 2;  // 垂直居中 (Y: 198~268)
#else
    int mapSize = 40;
    int startX = SCREEN_W - mapSize - 5; 
    int startY = SCREEN_H - mapSize - 5;
#endif
    const float MINI_SCALE = (float)mapSize / (float)MAP_WIDTH;
    
    _canvas.drawRect(startX - 1, startY - 1, mapSize + 2, mapSize + 2, 0x8410);
    _canvas.fillRect(startX, startY, mapSize, mapSize, 0xEF7D); 

    int vwX = startX + (int)roundf(camX * MINI_SCALE);
    int vwY = startY + (int)roundf(camY * MINI_SCALE);
    int vwW = (int)roundf(((float)SCREEN_W / view_scale) * MINI_SCALE);
    int vwH = (int)roundf(((float)SCREEN_H / view_scale) * MINI_SCALE);
    _canvas.drawRect(vwX, vwY, vwW, vwH, TFT_WHITE);
    _canvas.drawRect(vwX-1, vwY-1, vwW+2, vwH+2, TFT_WHITE);
    _canvas.drawRect(vwX+1, vwY+1, vwW-2, vwH-2, TFT_BLACK); 

    Food* foods = state.getFood();
    for (int i = 0; i < MAX_FOOD; i++) {
        if (foods[i].active) {
            int px = (int)(foods[i].x * MINI_SCALE);
            int py = (int)(foods[i].y * MINI_SCALE);
            if (foods[i].size > 20) {
                _canvas.fillRect(startX + px, startY + py, 2, 2, GameState::getRGBfromHue(foods[i].color));
            } else {
                _canvas.drawPixel(startX + px, startY + py, GameState::getRGBfromHue(foods[i].color));
            }
        }
    }

    Player* players = state.getPlayers();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active) {
            int px = (int)((float)players[i].x * MINI_SCALE);
            int py = (int)((float)players[i].y * MINI_SCALE);
            int pr = (int)roundf((float)players[i].radius * MINI_SCALE); 
            if (pr < 1) pr = 1;
            uint16_t color = GameState::getRGBfromHue(players[i].color);
            _canvas.fillCircle(startX + px, startY + py, pr, color);
            if (i == state.getLocalId()) _canvas.drawCircle(startX + px, startY + py, pr, TFT_WHITE);
        }
    }
}

void Render::drawMenuUI(GameState& state, uint8_t selected_idx, bool flash, bool item_blink) {
    if (!_display) return;

    if (flash) {
        _canvas.fillScreen(TFT_WHITE);
        return;
    }

    const int cx = (int)SCREEN_CX;
    _canvas.setTextDatum(middle_center);
#ifdef BOARD_ROUND_SCREEN
    drawBrushHighlight(cx - MENU_TITLE_W / 2, MENU_TITLE_Y - 20, MENU_TITLE_W, 40, TFT_BLACK);
    _canvas.setTextColor(0x4EC0);
    _canvas.setTextSize(3);
    _canvas.drawString(TITLE_STR, cx, MENU_TITLE_Y);

    _canvas.setTextSize(2);
    _canvas.setTextColor(TFT_DARKGREY);
    _canvas.setTextDatum(top_left);
    _canvas.drawString(GAME_VERSION_STR, MENU_VER_X, MENU_VER_Y);

    _canvas.setTextDatum(middle_center);
    _canvas.setTextSize(2);
    
    bool draw_bg = !item_blink || ((millis() / 100) % 2 == 0);

    if (selected_idx == 0) {
        if (draw_bg) drawBrushHighlight(cx - MENU_OPT_W / 2, MENU_OPT1_Y - 20, MENU_OPT_W, 40, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK); 
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Create Room (Master)", cx, MENU_OPT1_Y);

    if (selected_idx == 1) {
        if (draw_bg) drawBrushHighlight(cx - MENU_OPT_W / 2, MENU_OPT2_Y - 20, MENU_OPT_W, 40, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK);
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Join Room (Guest)", cx, MENU_OPT2_Y);
#else
    drawBrushHighlight(cx - MENU_TITLE_W / 2, MENU_TITLE_Y - 12, MENU_TITLE_W, 24, TFT_BLACK);
    _canvas.setTextColor(0x4EC0);
    _canvas.setTextSize(2);
    _canvas.drawString(TITLE_STR, cx, MENU_TITLE_Y);

    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_DARKGREY);
    _canvas.setTextDatum(top_left);
    _canvas.drawString(GAME_VERSION_STR, MENU_VER_X, MENU_VER_Y);

    _canvas.setTextDatum(middle_center);
    _canvas.setTextSize(1);
    
    bool draw_bg = !item_blink || ((millis() / 100) % 2 == 0);

    if (selected_idx == 0) {
        if (draw_bg) drawBrushHighlight(cx - MENU_OPT_W / 2, MENU_OPT1_Y - 12, MENU_OPT_W, 24, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK); 
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Create Room (Master Mode)", cx, MENU_OPT1_Y);

    if (selected_idx == 1) {
        if (draw_bg) drawBrushHighlight(cx - MENU_OPT_W / 2, MENU_OPT2_Y - 12, MENU_OPT_W, 24, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK);
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Join Room (Guest Mode)", cx, MENU_OPT2_Y);
#endif
}

void Render::drawBrushHighlight(int x, int y, int w, int h, uint16_t color) {
    int midY = y + h / 2;
    _canvas.fillRect(x + 5, y + 4, w - 10, h - 8, color);

    for (int i = 0; i < 10; i++) {
        int offset = (i - 5) * (h / 8);
        int jitterL = (i * 7) % (w / 12);
        int jitterR = (i * 13) % (w / 10);
        int lineW = w - jitterL - jitterR;
        int lineX = x + jitterL;
        
        if (abs(i - 5) > 3) {
            lineW -= (w / 8); 
            lineX += (w / 16);
        }
        
        _canvas.fillRect(lineX, midY + offset, lineW, 3, color);
    }
}
