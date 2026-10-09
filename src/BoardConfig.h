#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// 设备差异集中配置：逻辑画布尺寸、UI 布局、HUD 位置。
// TARGET_STOPWATCH：M5 StopWatch (1.75" 466x466 圆形 AMOLED 触摸屏)
//   采用 233x233 逻辑画布，推送时 2x 放大到物理屏幕 (方案 A)。
//   逻辑画布宽度与 Cardputer (240) 接近，因此世界可视范围基本一致，保持竞技公平。

#define GAME_VERSION_STR "v1.4"

#ifdef TARGET_STOPWATCH
    #define SCREEN_W 233
    #define SCREEN_H 233
    #define PUSH_ZOOM 2.0f
    #define BOARD_ROUND_SCREEN 1

    #define TITLE_STR "AGAR-WATCH"
    #define MENU_TITLE_Y 60
    #define MENU_TITLE_W 150
    #define MENU_VER_X (SCREEN_W / 2 - 10)
    #define MENU_VER_Y 76
    #define MENU_OPT1_Y 125
    #define MENU_OPT2_Y 165
    #define MENU_OPT_W 170

    // 触摸参数 (逻辑坐标)
    #define TOUCH_DEADZONE 14.0f
    #define TOUCH_DOUBLE_TAP_MS 320
    #define TOUCH_CENTER_RADIUS 40.0f
#else
    #define SCREEN_W 240
    #define SCREEN_H 135
    #define PUSH_ZOOM 1.0f

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
