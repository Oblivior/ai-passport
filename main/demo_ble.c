/* Connection page only. The pet worker owns the app-lifetime NimBLE stack. */
#include "demo.h"
#include "pet_ble.h"
#include "ui_pixel.h"
#include "bsp_battery.h"

static lv_obj_t *s_scr, *s_status, *s_battery;
static lv_timer_t *s_timer;

static void tick(lv_timer_t *timer)
{
    (void)timer;
    pet_ble_status_t state;
    pet_ble_status(&state);
    if (state.error) lv_label_set_text_fmt(s_status, "无线连接暂不可用\n\n可改用 USB 同步\n\n错误 %d", state.error);
    else if (!state.paired) lv_label_set_text(s_status, "首次请用 USB 连接\n\n在电脑上完成配对\n\n之后就能拔线使用");
    else lv_label_set_text_fmt(s_status, "%s\n\nAIPet-%s\n\n%s\n\n无需连接 USB",
        state.authenticated ? "已连接，准备送餐" : state.connected ? "正在验证电脑" : "等待电脑送来饭盒",
        state.id, state.ready ? "加密连接已就绪" : "正在启动无线连接");
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc);
}

void demo_ble_enter(void)
{
    s_scr = ui_pixel_screen_create("无线饭盒");
    s_battery = ui_pixel_label(s_scr, "--%", &passport_zh_14, UI_PAPER);
    lv_obj_set_pos(s_battery, 169, 29);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 55, 216, 224, UI_PAPER);
    s_status = ui_pixel_label(panel, "正在启动...", &passport_zh_14, UI_INK);
    lv_obj_set_width(s_status, 192);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_status);
    lv_obj_t *hint = ui_pixel_label(s_scr, "长按确定：返回菜单", &passport_zh_14, UI_INK);
    lv_obj_set_width(hint, 192);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 293);
    s_timer = lv_timer_create(tick, 500, NULL);
    tick(NULL);
    lv_screen_load(s_scr);
}

void demo_ble_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_status = s_battery = NULL;
}

void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev) { (void)btn; (void)ev; }
