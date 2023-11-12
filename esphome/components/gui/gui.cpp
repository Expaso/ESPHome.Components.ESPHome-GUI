#include "gui.h"

#include <string_view>

#ifdef USE_SQUARELINE
#include "ui.h"
#endif

namespace esphome {
namespace gui {

static const char *const TAG = "gui";

using namespace display;

namespace lv_shim {
  static lv_disp_drv_t lv_disp_drv; // lvgl 8.3.4
  static lv_disp_draw_buf_t lv_disp_buf; // lvgl 8.3.4
  
  #if LV_USE_LOG
  static void lv_esp_log(const char * buf) {
    ESP_LOGW("LVGL", buf);
  }
  #endif

  static void lv_drv_refresh(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {

    auto component = (DisplayBuffer*) disp_drv->user_data; 
    component->update();
    lv_disp_flush_ready(disp_drv);
  }

  lv_disp_t *init_lv_drv(DisplayBuffer *db, lv_disp_rot_t rotation) {

    uint8_t *buf = db->get_buffer();
    uint32_t len = db->get_width() * db->get_height(); //For 16 bit color depth

    lv_init();
    
    lv_disp_draw_buf_init(&lv_disp_buf, buf, NULL, len);
    lv_disp_drv_init(&lv_disp_drv);

    //Set the viewport area according to the display orientation
    lv_disp_drv.hor_res =  lv_disp_drv.rotated == 0 || lv_disp_drv.rotated == 2 ? db->get_width(): db->get_height();
    lv_disp_drv.ver_res = lv_disp_drv.rotated == 0 || lv_disp_drv.rotated == 2 ? db->get_height(): db->get_width();
    //lv_disp_drv.color_chroma_key = lv_color_black();
    lv_disp_drv.direct_mode = true;
    lv_disp_drv.full_refresh = false;  // Will trigger the watchdog if set.
    lv_disp_drv.flush_cb = lv_drv_refresh;
    lv_disp_drv.draw_buf = &lv_disp_buf;
    lv_disp_drv.user_data = db;

    lv_disp_t *disp = lv_disp_drv_register(&lv_disp_drv);
    lv_disp_set_bg_color(disp, lv_color_black());
    lv_disp_set_rotation(disp, rotation);
 
  #if LV_USE_LOG
    lv_log_register_print_cb(lv_esp_log);
  #endif

    return disp;
  }
}  // namespace lv_shim

void GuiComponent::setup() {
  // Register this instance of the GUI component with the lvgl driver
  this->lv_disp_ = lv_shim::init_lv_drv(this->display_, this->get_lv_rotation());
  // this->high_freq_.start();

//Init the UI
#ifdef USE_SQUARELINE
  ui_init();
#endif

}

void GuiComponent::loop() 
{ 
  if (this->lv_disp_ != nullptr) {
    lv_timer_handler();
    lv_task_handler(); 
  }
}

void GuiComponent::dump_config() {

  auto drv = this->lv_disp_->driver;
  ESP_LOGCONFIG(TAG, "LVGL driver.hor_res: %i", drv->hor_res);
  ESP_LOGCONFIG(TAG, "LVGL driver.ver_res: %i", drv->ver_res);
  ESP_LOGCONFIG(TAG, "LVGL driver.rotation: %i", drv->rotated);
}

lv_disp_rot_t GuiComponent::get_lv_rotation() {
  if (this->display_ != nullptr) {
    auto rot = this->display_->get_rotation();
    switch (rot) {
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

}  // namespace gui
}  // namespace esphome
