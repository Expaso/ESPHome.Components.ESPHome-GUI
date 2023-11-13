#pragma once

#include "esphome.h"
#include "lvgl.h"
#include "gui_objects.h"
#include "esphome/components/touchscreen/touchscreen.h"

namespace esphome
{

  // forward declare DisplayBuffer
  namespace display
  {
    class DisplayBuffer;
  } // namespace display


  namespace gui
  {

    using namespace display;
    using namespace touchscreen;

    class GuiComponent : public Component, public TouchListener
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;
      float get_setup_priority() const override
      {
        return setup_priority::PROCESSOR;
      }

      void set_display(DisplayBuffer *display) { this->display_ = display; }
      void set_touchscreen(Touchscreen *touchscreen) { this->touchscreen_ = touchscreen; }

      // TouchListener implementation
      void touch(TouchPoint tp) override;
      void release() override;

      TouchPoint get_lastTouchPoint() { return this->lastTouchpoint_; }
      DisplayBuffer *get_displayBuffer() { return this->display_; }
      Touchscreen *get_touchScreen() { return this->touchscreen_; }
      lv_disp_rot_t get_lv_rotation();

    protected:
      DisplayBuffer *display_{nullptr};
      Touchscreen *touchscreen_{nullptr};
      lv_disp_t *lv_disp_{nullptr};
      TouchPoint lastTouchpoint_;

    private:
      HighFrequencyLoopRequester high_freq_;
    };

  } // namespace gui
} // namespace esphome