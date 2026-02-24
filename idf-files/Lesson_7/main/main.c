/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "bsp_illuminate.h"  // Include LCD initialization and backlight control interface
#include "lvgl.h"         // Include LVGL graphics library API
#include "freertos/FreeRTOS.h"  // Include FreeRTOS core header
#include "freertos/task.h"      // Include FreeRTOS task API
#include "esp_ldo_regulator.h"  // Include LDO (Low Dropout Regulator) API
#include "esp_log.h"       // Include LOG printing interface

/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Macro definition—————————————————————————————————————————*/
#define MAIN_TAG "MAIN"  // Define log tag for this module
#define MAIN_INFO(fmt, ...) ESP_LOGI(MAIN_TAG, fmt, ##__VA_ARGS__)   // Info level log macro
#define MAIN_ERROR(fmt, ...) ESP_LOGE(MAIN_TAG, fmt, ##__VA_ARGS__)  // Error level log macro
/*———————————————————————————————————————Macro definition end———————————————————————————————————————*/

static esp_ldo_channel_handle_t ldo4 = NULL;  // Handle for LDO channel 4
static esp_ldo_channel_handle_t ldo3 = NULL;  // Handle for LDO channel 3

/*—————————————————————————————————————————Functional function———————————————————————————————————————————*/
/**
 * @brief LVGL text display function (display "Hello Elecrow")
 */
static void lvgl_show_hello_elecrow(void)
{
    if (lvgl_port_lock(0) != true) {
        MAIN_ERROR("LVGL lock failed");
        return;
    }

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, LV_COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    static lv_style_t label_style;
    static bool style_inited = false;
    if (!style_inited) {
        lv_style_init(&label_style);
        lv_style_set_text_font(&label_style, &lv_font_montserrat_42);
        lv_style_set_text_color(&label_style, LV_COLOR_BLACK);
        lv_style_set_bg_opa(&label_style, LV_OPA_TRANSP);
        style_inited = true;
    }

    static lv_style_t rect_style;
    static bool rect_style_inited = false;
    if (!rect_style_inited) {
        lv_style_init(&rect_style);
        lv_style_set_border_width(&rect_style, 4);
        lv_style_set_border_color(&rect_style, LV_COLOR_BLACK);
        lv_style_set_border_opa(&rect_style, LV_OPA_COVER);
        lv_style_set_radius(&rect_style, 10);
        lv_style_set_pad_all(&rect_style, 20);
        lv_style_set_bg_opa(&rect_style, LV_OPA_TRANSP);
        rect_style_inited = true;
    }

    // Container for both lines
    lv_obj_t *rect = lv_obj_create(screen);
    lv_obj_add_style(rect, &rect_style, LV_PART_MAIN);
    lv_obj_set_size(rect, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(rect);

    // First line INSIDE rectangle
    lv_obj_t *label1 = lv_label_create(rect);
    lv_obj_add_style(label1, &label_style, LV_PART_MAIN);
    lv_label_set_text(label1, "Hello Elecrow");
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, 0);

    // Second line INSIDE rectangle
    lv_obj_t *label2 = lv_label_create(rect);
    lv_obj_add_style(label2, &label_style, LV_PART_MAIN);
    lv_label_set_text(label2, "Greetings from the south of the world.");
    lv_obj_align_to(label2, label1, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lvgl_port_unlock();
}


/**
 * @brief Initialization failure handler (print error message repeatedly)
 */
static void init_fail_handler(const char *module_name, esp_err_t err) {
    while (1) {  // Infinite loop
        MAIN_ERROR("[%s] init failed: %s", module_name, esp_err_to_name(err));  // Print error with module name
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second between logs
    }
}

/**
 * @brief System initialization (LCD + Backlight)
 */
static void system_init(void) {
    esp_err_t err = ESP_OK;  // Error variable initialized to OK
    esp_ldo_channel_config_t ldo3_cof = {  // LDO3 configuration
        .chan_id = 3,         // LDO channel ID = 3
        .voltage_mv = 2500,   // Set output voltage = 2500 mV
    };
    err = esp_ldo_acquire_channel(&ldo3_cof, &ldo3);  // Acquire LDO3 channel
    if (err != ESP_OK)  // Check error
        init_fail_handler("ldo3", err);  // Handle failure
    esp_ldo_channel_config_t ldo4_cof = {  // LDO4 configuration
        .chan_id = 4,         // LDO channel ID = 4
        .voltage_mv = 3300,   // Set output voltage = 3300 mV
    };
    err = esp_ldo_acquire_channel(&ldo4_cof, &ldo4);  // Acquire LDO4 channel
    if (err != ESP_OK)  // Check error
        init_fail_handler("ldo4", err);  // Handle failure

    // 1. Initialize LCD hardware and LVGL (important: must init before enabling backlight)
    err = display_init();
    if (err != ESP_OK) {  // Check error
        init_fail_handler("LCD", err);  // Handle failure
    }
    MAIN_INFO("LCD init success");  // Print success log

    // 2. Turn on LCD backlight (brightness set to 100 = maximum)
    err = set_lcd_blight(100);  // Enable backlight
    if (err != ESP_OK) {  // Check error
        init_fail_handler("LCD Backlight", err);  // Handle failure
    }
    MAIN_INFO("LCD backlight opened (brightness: 100)");  // Print success log
}

/*———————————————————————————————————————Functional function end—————————————————————————————————————————*/

/*——————————————————————————————————————————Main function—————————————————————————————————————————*/
void app_main(void) {
    MAIN_INFO("Start Hello Elecrow Display Demo");  // Print start log

    // 1. System initialization (LCD + Backlight)
    system_init();
    // 2. Show "Hello Elecrow" text
    lvgl_show_hello_elecrow();
    MAIN_INFO("Show 'Hello Elecrow' success");  // Print success log

}
/*———————————————————————————————————————Main function end———————————————————————————————————————*/
