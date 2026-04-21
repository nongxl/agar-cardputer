#ifndef RENDER_H
#define RENDER_H

#include <Arduino.h>
#include <M5Unified.h>
#include "GameState.h"

class Render {
public:
    Render();
    void init(M5GFX* display);
    void draw(GameState& state, bool show_leaderboard, float alpha, float view_scale = 0.6f);
    void drawMenuUI(GameState& state, uint8_t selected_idx, bool flash = false, bool item_blink = false);
    void drawConnectingOverlay(bool is_searching); // [V47] 动态连接提示 Overlay
    void commit();

private:
    void drawBrushHighlight(int x, int y, int w, int h, uint16_t color);
    M5GFX* _display;   
    M5Canvas _canvas;  

    void drawLeaderboard(GameState& state);
    void drawMinimap(GameState& state, float view_scale, float camX, float camY);
    float smoothCamX, smoothCamY;
};

#endif
