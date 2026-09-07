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
    if (state.error) lv_label_set_text_fmt(s_status, "WIRELESS UNAVAILABLE\n\nUSB STILL WORKS\n\nERROR %d", state.error);
    else if (!state.paired) lv_label_set_text(s_status, "PAIR ONCE VIA USB\n\nRUN DESKTOP PAIRING\n\nTHEN UNPLUG AND PLAY");
    else lv_label_set_text_fmt(s_status, "%s\n\nAIPet-%s\n\n%s\n\nUSB NOT REQUIRED",
        state.authenticated ? "DELIVERING LUNCH" : state.connected ? "CHECKING COMPANION" : "WAITING FOR LUNCH",
        state.id, state.ready ? "ENCRYPTED APP LINK" : "STARTING RADIO");
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc);
}

void demo_ble_enter(void)
{
    s_scr = ui_pixel_screen_create("PET LINK");
    s_battery = ui_pixel_label(s_scr, "--%", &lv_font_montserrat_14, UI_PAPER);
    lv_obj_set_pos(s_battery, 169, 29);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 60, 216, 213, UI_PAPER);
    s_status = ui_pixel_label(panel, "STARTING...", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_width(s_status, 192);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_status);
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
