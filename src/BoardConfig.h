#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// 设备差异集中配置：逻辑画布尺寸、UI 布局、HUD 位置。
// TARGET_STOPWATCH：M5 StopWatch (1.75" 466x466 圆形 AMOLED 触摸屏)
//   采用 466x466 原生高刷画布，直接走硬件 DMA pushSprite，画质清晰细腻且帧率极高；
//   通过 VIEW_SCALE_FACTOR 自动等比缩放世界坐标，保持多设备竞技视野完全一致。

#define GAME_VERSION_STR "v1.5"

#ifdef TARGET_STOPWATCH
    #define SCREEN_W 466
    #define SCREEN_H 466
    #define BOARD_ROUND_SCREEN 1
    #define VIEW_SCALE_FACTOR (466.0f / 240.0f)
    #define UI_FONT_SCALE 2

    #define TITLE_STR "AGAR-WATCH"
    #define MENU_TITLE_Y 110
    #define MENU_TITLE_W 320
    #define MENU_VER_X (SCREEN_W / 2 + 70)
    #define MENU_VER_Y 135
    #define MENU_OPT1_Y 240
    #define MENU_OPT2_Y 320
    #define MENU_OPT_W 340

    // 触摸参数 (466x466 坐标)
    #define TOUCH_DEADZONE 25.0f
    #define TOUCH_DOUBLE_TAP_MS 320
    #define TOUCH_CENTER_RADIUS 70.0f
#else
    #define SCREEN_W 240
    #define SCREEN_H 135
    #define VIEW_SCALE_FACTOR 1.0f
    #define UI_FONT_SCALE 1

    #define TITLE_STR "AGAR-CARDPUTER"
    #define MENU_TITLE_Y 35
    #define MENU_TITLE_W 180
    #define MENU_VER_X (120 + 85)
    #define MENU_VER_Y 24
    #define MENU_OPT1_Y 95
    #define MENU_OPT2_Y 115
    #define MENU_OPT_W 180
#endif

#define SCREEN_CX (SCREEN_W / 2.0f)
#define SCREEN_CY (SCREEN_H / 2.0f)

#endif // BOARD_CONFIG_H
