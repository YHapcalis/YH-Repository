/*
 * layout_defines.h — Home 屏 & Mode 屏布局常量
 *
 * 所有位置 (X/Y) 和尺寸 (W/H) 的魔法数字集中于此。
 * 修改布局时只需改此文件的宏，无须在 setup_scr_*.c 中逐一定位。
 *
 * 命名规则:  {SCREEN}_{WIDGET}_{X|Y|W|H}
 *   SCREEN  = HOME / MODE
 *   WIDGET  = 对应 guider_ui 结构体成员名（去掉 screen_ 前缀）
 */

#ifndef __LAYOUT_DEFINES_H__
#define __LAYOUT_DEFINES_H__

/* ================================================================
 * Home 屏
 * ================================================================ */

/* home_arc_needle — 速度表弧 */
#define HOME_ARC_NEEDLE_X   233
#define HOME_ARC_NEEDLE_Y   93
#define HOME_ARC_NEEDLE_W   333
#define HOME_ARC_NEEDLE_H   333

/* home_label_digit — 速度数字（28px LiberationSans） */
#define HOME_LABEL_DIGIT_X  355
#define HOME_LABEL_DIGIT_Y  285
#define HOME_LABEL_DIGIT_W  80
#define HOME_LABEL_DIGIT_H  60

/* home_label_trip_num — 里程数（53px LiberationSans） */
#define HOME_LABEL_TRIP_X   70
#define HOME_LABEL_TRIP_Y   160
#define HOME_LABEL_TRIP_W   160
#define HOME_LABEL_TRIP_H   65

/* home_label_power_num — 功率数 */
#define HOME_LABEL_POWER_X  600
#define HOME_LABEL_POWER_Y  315
#define HOME_LABEL_POWER_W  135
#define HOME_LABEL_POWER_H  65

/* home_digital_clock_time — 数字时钟 */
#define HOME_CLOCK_X        16
#define HOME_CLOCK_Y        19
#define HOME_CLOCK_W        220
#define HOME_CLOCK_H        60

/* home_bar_battery — 电池条 */
#define HOME_BATTERY_X      735
#define HOME_BATTERY_Y      14
#define HOME_BATTERY_W      43
#define HOME_BATTERY_H      24

/* home_label_ODO_num — ODO 数 */
#define HOME_ODO_X          76
#define HOME_ODO_Y          310
#define HOME_ODO_W          160
#define HOME_ODO_H          61

/* home_img_right — 右转向灯 */
#define HOME_RIGHT_X        528
#define HOME_RIGHT_Y        56
#define HOME_RIGHT_W        50
#define HOME_RIGHT_H        42

/* home_img_left — 左转向灯 */
#define HOME_LEFT_X         216
#define HOME_LEFT_Y         56
#define HOME_LEFT_W         50
#define HOME_LEFT_H         42

/* home_label_WIFI — WIFI 图标 */
#define HOME_WIFI_X         666
#define HOME_WIFI_Y         14
#define HOME_WIFI_W         80
#define HOME_WIFI_H         60

/* home_label_BT — 蓝牙图标 */
#define HOME_BT_X           623
#define HOME_BT_Y           14
#define HOME_BT_W           80
#define HOME_BT_H           60

/* home_img_moto — 发动机图标 */
#define HOME_MOTO_X         610
#define HOME_MOTO_Y         180
#define HOME_MOTO_W         41
#define HOME_MOTO_H         37

/* home_img_light — 大灯图标 */
#define HOME_LIGHT_X        683
#define HOME_LIGHT_Y        120
#define HOME_LIGHT_W        46
#define HOME_LIGHT_H        46

/* home_img_high_light — 远光灯图标 */
#define HOME_HIGH_LIGHT_X   683
#define HOME_HIGH_LIGHT_Y   218
#define HOME_HIGH_LIGHT_W   46
#define HOME_HIGH_LIGHT_H   46

/* home_btn_mode_a / f / e / c — 底部模式按钮 */
#define HOME_BTN_X_A        260
#define HOME_BTN_X_F        331
#define HOME_BTN_X_E        475
#define HOME_BTN_X_C        403
#define HOME_BTN_Y          421
#define HOME_BTN_W          80
#define HOME_BTN_H          60

/* home_img_red / home_img_green — kmbg 背景图 */
#define HOME_RED_X          316
#define HOME_RED_Y          151
#define HOME_RED_W          350
#define HOME_RED_H          350
#define HOME_GREEN_X        355
#define HOME_GREEN_Y        174
#define HOME_GREEN_W        350
#define HOME_GREEN_H        350

/* home — 全屏 */
#define HOME_W              800
#define HOME_H              480

/* ================================================================
 * Mode 屏
 * ================================================================ */

/* mode — 全屏 */
#define MODE_W              800
#define MODE_H              480

/* mode_tileview */
#define MODE_TILEVIEW_X     0
#define MODE_TILEVIEW_Y     0
#define MODE_TILEVIEW_W     800
#define MODE_TILEVIEW_H     480

/* Tile 标题标签 (A / F / C / E) — 共用一个 Y, 各自 X */
#define MODE_TITLE_Y        30
#define MODE_TITLE_H        60
#define MODE_TITLE_A_X      370
#define MODE_TITLE_A_W      120
#define MODE_TITLE_F_X      370
#define MODE_TITLE_F_W      140
#define MODE_TITLE_C_X      370
#define MODE_TITLE_C_W      240
#define MODE_TITLE_E_X      350
#define MODE_TITLE_E_W      180

/* mode_img_nav — 指南针 */
#define MODE_NAV_X          342
#define MODE_NAV_Y          80
#define MODE_NAV_W          116
#define MODE_NAV_H          116

/* mode_img_temper — 温度计图标 */
#define MODE_TEMPER_X       100
#define MODE_TEMPER_Y       120
#define MODE_TEMPER_W       91
#define MODE_TEMPER_H       123

/* mode_label_temp_num — 温度数值 */
#define MODE_TEMP_NUM_X     250
#define MODE_TEMP_NUM_Y     140
#define MODE_TEMP_NUM_W     200
#define MODE_TEMP_NUM_H     80

/* mode_label_tempter — 湿度数值（动态创建） */
#define MODE_TEMPTER_X      250
#define MODE_TEMPTER_Y      230
#define MODE_TEMPTER_W      300
#define MODE_TEMPTER_H      60

/* mode_img_weather — 天气图标 */
#define MODE_WEATHER_X      200
#define MODE_WEATHER_Y      120
#define MODE_WEATHER_W      66
#define MODE_WEATHER_H      66

/* mode_label_day — 天气文字 */
#define MODE_DAY_X          300
#define MODE_DAY_Y          130
#define MODE_DAY_W          200
#define MODE_DAY_H          90

/* mode_img_music — 音乐封面 */
#define MODE_MUSIC_X        308
#define MODE_MUSIC_Y        80
#define MODE_MUSIC_W        183
#define MODE_MUSIC_H        183

/* mode_label_name — 歌曲名 */
#define MODE_NAME_X         300
#define MODE_NAME_Y         280
#define MODE_NAME_W         300
#define MODE_NAME_H         60

/* mode_img_home — 返回首页按钮 */
#define MODE_HOME_X         5
#define MODE_HOME_Y         5
#define MODE_HOME_W         43
#define MODE_HOME_H         43

#endif /* __LAYOUT_DEFINES_H__ */
