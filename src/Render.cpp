#include "Render.h"
#include <M5Cardputer.h>

Render::Render() : _display(nullptr), smoothCamX(0), smoothCamY(0) {
}

void Render::init(M5GFX* display) {
    _display = display;
    _canvas.createSprite(240, 135);
}

void Render::draw(GameState& state, bool show_leaderboard, float alpha, float VIEW_SCALE) {
    if (!_display) return;

    _canvas.fillScreen(0xF7DF); 
    uint8_t local_id = state.getLocalId();

    // [V46] 相机位置保护：如果尚未初始化（未收到主数据），相机锁定在地图中心
    float targetX, targetY;
    if (state.isInitialized()) {
        targetX = state.getInterpX(local_id, alpha);
        targetY = state.getInterpY(local_id, alpha);
    } else {
        targetX = MAP_WIDTH / 2.0f;
        targetY = MAP_HEIGHT / 2.0f;
    }

    // [V36] 相机平滑跟随逻辑 (80% 历史 + 20% 当前目标)
    if (abs(smoothCamX - targetX) > 1000.0f) {
        smoothCamX = targetX;
        smoothCamY = targetY;
    } else {
        smoothCamX = smoothCamX * 0.8f + targetX * 0.2f;
        smoothCamY = smoothCamY * 0.8f + targetY * 0.2f;
    }

    float camX = smoothCamX - (120.0f / VIEW_SCALE);
    float camY = smoothCamY - (67.5f / VIEW_SCALE);

    int mapLeft = (int)((0.0f - camX) * VIEW_SCALE);
    int mapTopSize = (int)((0.0f - camY) * VIEW_SCALE);
    int mapWidth = (int)((float)MAP_WIDTH * VIEW_SCALE);
    int mapHeight = (int)((float)MAP_HEIGHT * VIEW_SCALE);
    for (int b = 0; b < 2; b++) {
        _canvas.drawRect(mapLeft - b, mapTopSize - b, mapWidth + 2*b, mapHeight + 2*b, TFT_BLACK); 
    }

    // 1. 绘制网格
    _canvas.setClipRect(mapLeft, mapTopSize, mapWidth, mapHeight);
    float gridStep = 50.0f * VIEW_SCALE;
    for (float x = fmod(-camX * VIEW_SCALE, gridStep); x < 240.0f; x += gridStep)
        _canvas.drawFastVLine((int)x, 0, 135, 0xCE59);
    for (float y = fmod(-camY * VIEW_SCALE, gridStep); y < 135.0f; y += gridStep)
        _canvas.drawFastHLine(0, (int)y, 240, 0xCE59);
    _canvas.clearClipRect();

    // 2. 绘制食物
    Food* food = state.getFood();
    for (int i = 0; i < MAX_FOOD; i++) {
        if (food[i].active) {
            int relX = (int)(((float)food[i].x - camX) * VIEW_SCALE);
            int relY = (int)(((float)food[i].y - camY) * VIEW_SCALE);
            if (relX > -15 && relX < 255 && relY > -15 && relY < 150) {
                _canvas.fillCircle(relX, relY, (int)((food[i].size + 1) * VIEW_SCALE + 1), GameState::getRGBfromHue(food[i].color));
            }
        }
    }

    // 3. 绘制动态碎肉
    Pellet* pellets = state.getPellets();
    for (int i = 0; i < MAX_PELLETS; i++) {
        if (pellets[i].active) {
            int relX = (int)(((float)pellets[i].x - camX) * VIEW_SCALE);
            int relY = (int)(((float)pellets[i].y - camY) * VIEW_SCALE);
            if (relX > -15 && relX < 255 && relY > -15 && relY < 150) {
                _canvas.fillCircle(relX, relY, (int)((float)pellets[i].size * VIEW_SCALE), GameState::getRGBfromHue(pellets[i].color));
            }
        }
    }

    // 4. [V17] 原初几何刺球渲染 (24 齿轮替半径算法)
    Virus* viruses = state.getViruses();
    // 原版配色方案对齐
    const uint16_t COLOR_VIRUS_FILL = 0x37E6;   // #33FF33
    const uint16_t COLOR_VIRUS_STROKE = 0x1A23; // #19D119 (深绿)
    
    for (int v = 0; v < MAX_VIRUSES; v++) {
        if (viruses[v].active) {
            int relX = (viruses[v].x - camX) * VIEW_SCALE;
            int relY = (viruses[v].y - camY) * VIEW_SCALE;
            float r = (float)viruses[v].radius * VIEW_SCALE;
            
            if (relX > -100 && relX < 340 && relY > -100 && relY < 235) {
                uint16_t vColor = GameState::getRGBfromHue(viruses[v].color);
                _canvas.fillCircle(relX, relY, r, vColor);
                _canvas.drawCircle(relX, relY, r, TFT_BLACK);

                // [V25] 渲染动态数量的针刺
                int spikes = viruses[v].spike_count;
                for (int i = 0; i < spikes; i++) {
                    float angle = (i * 360.0f / spikes) * (PI / 180.0f);
                    float angle1 = angle - (float)(100.0f / (spikes * r)) * (PI / 180.0f); // 根据刺数量微调底座宽度
                    float angle2 = angle + (float)(100.0f / (spikes * r)) * (PI / 180.0f);
                    
                    float bx1 = relX + cos(angle1) * r;
                    float by1 = relY + sin(angle1) * r;
                    float bx2 = relX + cos(angle2) * r;
                    float by2 = relY + sin(angle2) * r;
                    
                    float sl = viruses[v].spike_length * VIEW_SCALE;
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
            // [V39] 核心：使用插值后的高精度渲染位置
            float pX = state.getInterpX(i, alpha);
            float pY = state.getInterpY(i, alpha);
            int relX = (int)((pX - camX) * VIEW_SCALE);
            int relY = (int)((pY - camY) * VIEW_SCALE);
            int radius = (int)((float)state.getRealRadius(i) * VIEW_SCALE);
            if (relX > -150 && relX < 400 && relY > -150 && relY < 300) {
                uint16_t color = GameState::getRGBfromHue(players[i].color);
                _canvas.fillCircle(relX, relY, radius, color);
                _canvas.drawCircle(relX, relY, radius + 1, TFT_BLACK);
                
                _canvas.setTextColor(TFT_BLACK); 
                _canvas.setTextDatum(middle_center);
                _canvas.drawString(players[i].name, relX, relY);
            }
        }
    }

    Player* local = state.getLocalPlayer();
    if (!local->active && local->respawn_tick > 0 && !state.isGameOver()) {
        _canvas.setTextSize(4);
        _canvas.setTextColor(((millis() / 250) % 2 == 0) ? TFT_RED : TFT_WHITE);
        _canvas.setTextDatum(middle_center);
        _canvas.drawString(String((local->respawn_tick / 20) + 1), 120, 67);
        _canvas.setTextSize(1);
    }

    // [V29] 终局胜利画面
    if (state.isGameOver()) {
        uint8_t winId = state.getWinnerId();
        Player* winner = &players[winId];
        _canvas.setTextSize(4);
        _canvas.setTextColor(GameState::getRGBfromHue(winner->color));
        _canvas.setTextDatum(middle_center);
        char winBuf[32];
        sprintf(winBuf, "%s WINS", winner->name);
        _canvas.drawString(winBuf, 120, 67);
        _canvas.setTextSize(1);
    }

    if (show_leaderboard) drawLeaderboard(state);
    // [V41.1] 菜单演示模式下移除小地图，保持界面极简
    if (!state.getDemoMode()) {
        drawMinimap(state, VIEW_SCALE, camX, camY);
    }

    if (!state.getDemoMode()) {
        drawMinimap(state, VIEW_SCALE, camX, camY);
    }
}

void Render::drawConnectingOverlay(bool is_searching) {
    // [V47] 居中黄色笔触弹窗 + 三位点动态动画 (三秒一循环)
    int dotCount = (millis() / 500) % 4;
    char dots[4] = {0};
    for(int i = 0; i < dotCount; i++) dots[i] = '.';

    drawBrushHighlight(120 - 95, 67 - 22, 190, 44, 0xFFE0); // 居中黄色笔触底框
    _canvas.setTextColor(TFT_BLACK);
    _canvas.setTextDatum(middle_center);
    _canvas.setTextSize(1);
    
    char msgBuf[64];
    if (is_searching) {
        sprintf(msgBuf, "SEARCHING FOR MASTER%s", dots);
    } else {
        sprintf(msgBuf, "WAITING FOR SYNC DATA%s", dots);
    }
    
    _canvas.drawString(msgBuf, 120, 67);
}

void Render::commit() {
    if (!_display) return;
    _canvas.pushSprite(_display, 0, 0);
}

void Render::drawLeaderboard(GameState& state) {
    _canvas.setTextDatum(top_left);
    _canvas.setTextColor(TFT_BLACK);
    _canvas.setTextSize(1);
    char totalBuf[32];
    sprintf(totalBuf, "T_MASS:%u", state.getTotalWorldMass());
    _canvas.drawString(totalBuf, 170, 5);
    _canvas.setTextColor(TFT_BLACK);
    _canvas.drawString("== TOP ==", 170, 14);
    _canvas.drawLine(170, 23, 235, 23, TFT_DARKGREY);
    
    Player* players = state.getPlayers();
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active) {
            uint16_t color = GameState::getRGBfromHue(players[i].color);
            _canvas.setTextColor(color);
            char buf[32];
            sprintf(buf, "%s: %d", players[i].name, players[i].score);
            _canvas.drawString(buf, 170, 26 + (count++) * 9);
        } else if (players[i].respawn_tick > 0) {
            _canvas.setTextColor(((millis() / 300) % 2 == 0) ? TFT_RED : TFT_DARKGREY);
            char buf[32];
            sprintf(buf, "%s: %ds", players[i].name, (players[i].respawn_tick / 20) + 1);
            _canvas.drawString(buf, 170, 26 + (count++) * 9);
        }
    }
}

void Render::drawMinimap(GameState& state, float view_scale, float camX, float camY) {
    int mapSize = 40;
    int startX = 240 - mapSize - 5; 
    int startY = 135 - mapSize - 5;
    const float MINI_SCALE = (float)mapSize / (float)MAP_WIDTH;
    
    _canvas.drawRect(startX - 1, startY - 1, mapSize + 2, mapSize + 2, 0x8410);
    _canvas.fillRect(startX, startY, mapSize, mapSize, 0xEF7D); 

    // [V30.2] 使用浮点比例与 round() 修正视野框偏移
    int vwX = startX + (int)roundf(camX * MINI_SCALE);
    int vwY = startY + (int)roundf(camY * MINI_SCALE);
    int vwW = (int)roundf((240.0f / view_scale) * MINI_SCALE);
    int vwH = (int)roundf((135.0f / view_scale) * MINI_SCALE);
    _canvas.drawRect(vwX, vwY, vwW, vwH, TFT_WHITE);
    _canvas.drawRect(vwX-1, vwY-1, vwW+2, vwH+2, TFT_WHITE);
    _canvas.drawRect(vwX+1, vwY+1, vwW-2, vwH-2, TFT_BLACK); 

    Food* foods = state.getFood();
    for (int i = 0; i < MAX_FOOD; i++) {
        if (foods[i].active) {
            int px = (int)(foods[i].x * MINI_SCALE);
            int py = (int)(foods[i].y * MINI_SCALE);
            // 大号食物在小地图上显示一个小点，极小粒食物保持单点
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
            if (pr < 1) pr = 1; // [V30.3] 恢复真实比例，保底 1px
            uint16_t color = GameState::getRGBfromHue(players[i].color);
            _canvas.fillCircle(startX + px, startY + py, pr, color);
            if (i == state.getLocalId()) _canvas.drawCircle(startX + px, startY + py, pr, TFT_WHITE);
        }
    }
}

void Render::drawMenuUI(GameState& state, uint8_t selected_idx, bool flash, bool item_blink) {
    if (!_display) return;

    // [V44.3] 缓冲区同步闪烁：如果是确认闪烁状态，直接填充满画布，防止被旧帧覆盖
    if (flash) {
        _canvas.fillScreen(TFT_WHITE);
        return;
    }

    _canvas.setTextDatum(middle_center);
    // [V44] 增加标题背景笔触：黑色涂鸦风格
    drawBrushHighlight(120 - 90, 35 - 12, 180, 24, TFT_BLACK); // [V44.2] Y 45->35
    _canvas.setTextColor(0x4EC0); // [V43] 亮绿色品牌标题 (Vivid Green)
    _canvas.setTextSize(2);
    _canvas.drawString("AGAR-CARDPUTER", 120, 35);

    // [V67] 增加版本号显示并修复对齐重置
    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_DARKGREY);
    _canvas.setTextDatum(top_left);
    _canvas.drawString("v1.2", 120 + 85, 24);

    _canvas.setTextDatum(middle_center); // 关键：恢复居中对齐，防止后续菜单选项偏移
    _canvas.setTextSize(1);
    
    // [V53] 选项背景闪烁逻辑：(millis() / 100) % 2 == 0 时绘制
    bool draw_bg = !item_blink || ((millis() / 100) % 2 == 0);

    // 选项 1: Create Room
    if (selected_idx == 0) {
        if (draw_bg) drawBrushHighlight(120 - 90, 95 - 12, 180, 24, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK); 
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Create Room (Master Mode)", 120, 95);

    // 选项 2: Join Room
    if (selected_idx == 1) {
        if (draw_bg) drawBrushHighlight(120 - 90, 115 - 12, 180, 24, 0xFB56); 
        _canvas.setTextColor(draw_bg ? TFT_WHITE : TFT_BLACK);
    } else {
        _canvas.setTextColor(TFT_BLACK);
    }
    _canvas.drawString("Join Room (Guest Mode)", 120, 115);
}

void Render::drawBrushHighlight(int x, int y, int w, int h, uint16_t color) {
    // [V44.1] 算法升级：密实填充核心 + 碎片化边缘。防止高对比度时切碎文字。
    int midY = y + h / 2;
    
    // 1. 绘制核心实心层 (覆盖文字主体区域)
    _canvas.fillRect(x + 5, y + 4, w - 10, h - 8, color);

    // 2. 绘制碎片化边缘 (模拟画笔飞白效果)
    for (int i = 0; i < 10; i++) {
        int offset = (i - 5) * 2; // 线条间距减小，增加重叠
        // 外部抖动：两端长度随机，但保持中心闭合
        int jitterL = (i * 7) % 12;
        int jitterR = (i * 13) % 15;
        int lineW = w - jitterL - jitterR;
        int lineX = x + jitterL;
        
        // 分级渲染：中间的线更宽，边缘的线随机缩短
        if (abs(i - 5) > 3) {
            lineW -= 20; 
            lineX += 10;
        }
        
        _canvas.fillRect(lineX, midY + offset, lineW, 3, color); // 线粗增加到 3px
    }
}
