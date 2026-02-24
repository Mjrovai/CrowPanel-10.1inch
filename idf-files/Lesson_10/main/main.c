// main.c
#include "main.h"
#include "bsp_dht20.h"

/* Log monitor on Panel - For Debug Only */
static lv_obj_t *s_log_label = NULL;

/* Status Window */
static bool s_led_on = false;
static lv_obj_t *s_led_status_label = NULL;

/* DHT20 label */
static lv_obj_t *s_dht20_label = NULL;

/* LDO channel handle */
static esp_ldo_channel_handle_t ldo3 = NULL;
static esp_ldo_channel_handle_t ldo4 = NULL;

/* Forward declarations */
static void update_led_status_label(void);
static void update_dht20_value(float temperature, float humidity);
static void dht20_read_task(void *param);
static void ui_log(const char *msg);

/* -------------------------------------------------------------------------- */
/* Button callbacks                                                           */
/* -------------------------------------------------------------------------- */

static void btn_on_click_event(lv_event_t *e)
{
    (void)e;
    gpio_extra_set_level(true);
    s_led_on = true;
    update_led_status_label();
    ui_log("LED turned ON");
}

static void btn_off_click_event(lv_event_t *e)
{
    (void)e;
    gpio_extra_set_level(false);
    s_led_on = false;
    update_led_status_label();
    ui_log("LED turned OFF");
}

/* -------------------------------------------------------------------------- */
/* UI creation                                                                */
/* -------------------------------------------------------------------------- */

static void create_led_control_ui(void)
{
    if (!lvgl_port_lock(0)) {
        MAIN_ERROR("LVGL lock failed in create_led_control_ui");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Title */
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "HOME Panel Controller");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

    /* LED ON button */
    lv_obj_t *btn_on = lv_btn_create(scr);
    lv_obj_set_size(btn_on, 120, 50);
    lv_obj_align(btn_on, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(btn_on, btn_on_click_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_on = lv_label_create(btn_on);
    lv_label_set_text(label_on, "LED ON");

    /* LED OFF button */
    lv_obj_t *btn_off = lv_btn_create(scr);
    lv_obj_set_size(btn_off, 120, 50);
    lv_obj_align(btn_off, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(btn_off, btn_off_click_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_off = lv_label_create(btn_off);
    lv_label_set_text(label_off, "LED OFF");

    /* Status window */
    static lv_style_t status_style;
    static bool status_style_inited = false;
    if (!status_style_inited) {
        lv_style_init(&status_style);
        lv_style_set_border_width(&status_style, 2);
        lv_style_set_border_color(&status_style, LV_COLOR_BLACK);
        lv_style_set_border_opa(&status_style, LV_OPA_COVER);
        lv_style_set_pad_all(&status_style, 8);
        lv_style_set_bg_opa(&status_style, LV_OPA_TRANSP);
        status_style_inited = true;
    }

    lv_obj_t *status_cont = lv_obj_create(scr);
    lv_obj_add_style(status_cont, &status_style, LV_PART_MAIN);
    lv_obj_set_size(status_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(status_cont, LV_ALIGN_BOTTOM_MID, 0, -20);

    s_led_status_label = lv_label_create(status_cont);
    lv_label_set_text(s_led_status_label, "LED Status: OFF");
    lv_obj_center(s_led_status_label);

    /* Log label at bottom-left */
    s_log_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_log_label, lv_color_hex(0x000000), 0);
    lv_label_set_text(s_log_label, "Log: ready");
    lv_obj_align(s_log_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    /* DHT20 label*/
    s_dht20_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_dht20_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_dht20_label, lv_color_hex(0x000000), 0);
    lv_label_set_text(s_dht20_label,
                      "Temperature = 0.0 C  Humidity = 0.0 %");
    lv_obj_align(s_dht20_label, LV_ALIGN_CENTER, 0, -150);

    lvgl_port_unlock();
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static void update_led_status_label(void)
{
    if (!s_led_status_label) return;

    if (s_led_on) {
        lv_label_set_text(s_led_status_label, "LED Status: ON");
    } else {
        lv_label_set_text(s_led_status_label, "LED Status: OFF");
    }
}

static void update_dht20_value(float temperature, float humidity)
{
    if (!s_dht20_label) return;

    char buffer[64];
    snprintf(buffer, sizeof(buffer),
             "Temperature = %.1f C  Humidity = %.1f %%",
             temperature, humidity);  // note the double %%
    lv_label_set_text(s_dht20_label, buffer);
}

static void ui_log(const char *msg)
{
    if (!s_log_label) return;

    if (lvgl_port_lock(0)) {
        lv_label_set_text(s_log_label, msg);
        lvgl_port_unlock();
    }
}

/* -------------------------------------------------------------------------- */
/* Init error handler                                                         */
/* -------------------------------------------------------------------------- */

static void init_fail_handler(const char *module_name, esp_err_t err)
{
    while (1) {
        MAIN_ERROR("[%s] init failed: %s", module_name, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------------------------------------------------------------------------- */
/* System init                                                                */
/* -------------------------------------------------------------------------- */

static void system_init(void)
{
    esp_err_t err = ESP_OK;

    /* 1. LDOs */
    esp_ldo_channel_config_t ldo3_cof = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    err = esp_ldo_acquire_channel(&ldo3_cof, &ldo3);
    if (err != ESP_OK) init_fail_handler("ldo3", err);

    esp_ldo_channel_config_t ldo4_cof = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    err = esp_ldo_acquire_channel(&ldo4_cof, &ldo4);
    if (err != ESP_OK) init_fail_handler("ldo4", err);
    ui_log("LDO3 and LDO4 init success");

    /* 2. I2C */
    ui_log("Initializing I2C...");
    err = i2c_init();
    if (err != ESP_OK) init_fail_handler("I2C", err);
    ui_log("I2C init success");

    /* 3. Touch */
    ui_log("Initializing touch panel...");
    err = touch_init();
    if (err != ESP_OK) init_fail_handler("Touch", err);
    ui_log("Touch panel init success");

    /* 4. DHT20 sensor */
    ui_log("Initializing DHT20 sensor...");
    err = dht20_begin();
    if (err != ESP_OK) init_fail_handler("DHT20", err);
    ui_log("DHT20 init success");

    /* 5. Display + LVGL */
    err = display_init();
    if (err != ESP_OK) init_fail_handler("LCD", err);
    ui_log("LCD init success");

    /* 6. Backlight */
    err = set_lcd_blight(100);
    if (err != ESP_OK) init_fail_handler("LCD Backlight", err);
    ui_log("LCD backlight opened (100)");

    /* 7. LED GPIO */
    ui_log("Initializing GPIO48 for LED...");
    err = gpio_extra_init();
    if (err != ESP_OK) init_fail_handler("GPIO48", err);
    gpio_extra_set_level(false);
    s_led_on = false;
    ui_log("LED initialized to OFF");

    /* 8. UI */
    create_led_control_ui();
    ui_log("UI created");
    update_led_status_label();

    /* 9. DHT20 task */
    xTaskCreate(dht20_read_task,
                "dht20_task",
                4096,
                NULL,
                configMAX_PRIORITIES - 5,
                NULL);
    ui_log("DHT20 task started");
}

/* -------------------------------------------------------------------------- */
/* DHT20 task                                                                 */
/* -------------------------------------------------------------------------- */

static void dht20_read_task(void *param)
{
    (void)param;
    dht20_data_t measurements;

    while (1) {
        if (dht20_is_calibrated() != ESP_OK) {
            ui_log("DHT20 not calibrated, reinit...");
            if (dht20_begin() != ESP_OK) {
                MAIN_ERROR("dht20 init again failed");
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        if (dht20_read_data(&measurements) != ESP_OK) {
            MAIN_ERROR("dht20 read data error");
            if (lvgl_port_lock(0)) {
                if (s_dht20_label) {
                    lv_label_set_text(s_dht20_label,
                                      "dht20 read data error");
                }
                lvgl_port_unlock();
            }
            ui_log("DHT20 read error");
        } else {
            if (lvgl_port_lock(0)) {
                update_dht20_value(measurements.temperature,
                                   measurements.humidity);
                lvgl_port_unlock();
            }

            char msg[64];
            snprintf(msg, sizeof(msg), "T=%.1fC H=%.1f%%",
                     measurements.temperature, measurements.humidity);
            ui_log(msg);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------------------------------------------------------------------------- */
/* app_main                                                                   */
/* -------------------------------------------------------------------------- */

void app_main(void)
{
    MAIN_INFO("Starting LED + DHT20 control application...");
    system_init();
    MAIN_INFO("System initialized");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
