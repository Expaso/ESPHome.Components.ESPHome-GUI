#include "gui.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome
{
  namespace gui
  {
    static const char *const TAG = "gui";

    using namespace display;
    using namespace touchscreen;

    // A shim to allow lvgl to work nice with the ESPHome display buffer and display drivers
    namespace lv_shim
    {
      static TaskHandle_t lvglTickTaskHandle = nullptr;      
      static lv_disp_drv_t lv_disp_drv;
      static lv_disp_draw_buf_t lv_disp_buf;
      static lv_indev_drv_t lv_touch_drv;
      //static volatile UBaseType_t  uxHighWaterMark;

#if LV_USE_LOG
      static void lv_esp_log(const char *buf)
      {
        ESP_LOGW("LVGL", buf);
      }
#endif

      static void flush_ready() {
        lv_disp_flush_ready(&lv_disp_drv);
      }

      // A background task used to let lvgl render it's thing.
      static void IRAM_ATTR lvgl_tick_task(void *pv_params) {
        while(true) {
          //uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
          auto sleep = lv_timer_handler();
          vTaskDelay(sleep * portTICK_PERIOD_MS);
        }
      }

      // Callback from lvgl to update the display
      static void lv_drv_refresh(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
      {      
        auto component = (DisplayBuffer *)disp_drv->user_data;
        component->updateArea(area->x1, area->y1, area->x2, area->y2, (uint16_t*) disp_drv->draw_buf->buf_act, lv_shim::flush_ready);
      }


      // Callback from lvgl to read the touchscreen
      static void lv_touchscreen_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
      {
        auto gui = (GuiComponent *) indev_driver->user_data;
        auto tp = gui->get_lastTouchPoint();
        data->point.x = tp.x;
        data->point.y = tp.y;
        data->state = (lv_indev_state_t) tp.state;
        data->continue_reading = false;
      }

      // LVGL Inistalization
      lv_disp_t *init_lv_drv(GuiComponent *gui)
      {
        auto db = gui->get_displayBuffer();

        uint32_t len = db->get_width() * db->get_height(); // For 16 bit color depth


        #ifdef ESP32
            auto *buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            auto *buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        #else
            auto *buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * len);
            auto *buf2 = (lv_color_t *)malloc(sizeof(lv_color_t) * len);
        #endif

        // Initialize LVGL
        lv_init();

        // Initialize the display buffer and driver
        lv_disp_draw_buf_init(&lv_disp_buf, buf1, buf2, len);
        lv_disp_drv_init(&lv_disp_drv);

        // Set the viewport area according to the display orientation
        lv_disp_drv.hor_res = lv_disp_drv.rotated == 0 || lv_disp_drv.rotated == 2 ? db->get_width() : db->get_height();
        lv_disp_drv.ver_res = lv_disp_drv.rotated == 0 || lv_disp_drv.rotated == 2 ? db->get_height() : db->get_width();
        lv_disp_drv.direct_mode = false;
        lv_disp_drv.full_refresh = false; // Will trigger the watchdog if set.
        lv_disp_drv.flush_cb = lv_drv_refresh;
        lv_disp_drv.draw_buf = &lv_disp_buf;
        lv_disp_drv.user_data = db;

        // Create a display
        lv_disp_t *disp = lv_disp_drv_register(&lv_disp_drv);
        lv_disp_set_bg_color(disp, lv_color_black());
        lv_disp_set_rotation(disp, gui->get_lv_rotation());

        // If we have a touchscreen, register it with lvgl!
        if (gui->get_touchScreen() != nullptr)
        {
          // Register this component as a touch listener to ESPHome
          gui->get_touchScreen()->register_listener(gui);

          // And create an input device as a pointer device
          lv_indev_drv_init(&lv_touch_drv);
          lv_touch_drv.type = LV_INDEV_TYPE_POINTER;
          lv_touch_drv.read_cb = lv_touchscreen_read;
          lv_touch_drv.user_data = gui;
          lv_indev_drv_register(&lv_touch_drv);
        }

#if LV_USE_LOG
        lv_log_register_print_cb(lv_esp_log);
#endif
        //Run on the Application Core, on prio (Display driver's affinity is on IO core 0, slightly lower prio due to being nice to Wifi and BT)
        //xTaskCreatePinnedToCore(lv_shim::lvgl_tick_task, "lvgl_tick", 4096, NULL, 10, &lvglTickTaskHandle, 1); //tskNO_AFFINITY
        return disp;
      }

    } // namespace lv_shim

    void GuiComponent::setup()
    {
      // Register this instance of the GUI component with the lvgl driver
      this->lv_disp_ = lv_shim::init_lv_drv(this);

      if (this->lv_disp_ == nullptr)
      {
        this->mark_failed();
        return;
      }

      this->high_freq_.start();
    }

    void GuiComponent::loop()
    {
      //Handle LVGL takss during the loop
      if (this->lv_disp_ != nullptr)
      {
        lv_timer_handler_run_in_period(8);
        //lv_timer_handler();
      }
    }

    void GuiComponent::dump_config()
    {
      auto drv = this->lv_disp_->driver;
      ESP_LOGCONFIG(TAG, "LVGL driver.hor_res: %i", drv->hor_res);
      ESP_LOGCONFIG(TAG, "LVGL driver.ver_res: %i", drv->ver_res);
      ESP_LOGCONFIG(TAG, "LVGL driver.rotation: %i", drv->rotated);
    }

    lv_disp_rot_t GuiComponent::get_lv_rotation()
    {
      if (this->display_ != nullptr)
      {
        auto rot = this->display_->get_rotation();
        switch (rot)
        {
        case DISPLAY_ROTATION_0_DEGREES:
          return LV_DISP_ROT_NONE;
        case DISPLAY_ROTATION_90_DEGREES:
          return LV_DISP_ROT_90;
        case DISPLAY_ROTATION_180_DEGREES:
          return LV_DISP_ROT_180;
        case DISPLAY_ROTATION_270_DEGREES:
          return LV_DISP_ROT_270;
        }
      }
      return LV_DISP_ROT_NONE;
    }

    void HOT GuiComponent::touch(TouchPoint tp)
    {
      this->lastTouchpoint_ = tp;
      this->lastTouchpoint_.state = LV_INDEV_STATE_PR;
    }

    void GuiComponent::release()
    {
      this->lastTouchpoint_.state = LV_INDEV_STATE_REL;
    }

  } // namespace gui
} // namespace esphome
