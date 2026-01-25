#include "../controllers/encoder_controller.h"
#include "../cv_mapping_processor.h"
#include "../parameter.h"
#include "../ui_state.h"
#include "test_framework.h"
#include <array>

using namespace mutables_ui;

// Validates the EncoderController class logic

TEST(EncoderController, Navigate) {
  Parameter params[2];
  params[0] = Parameter::Knob("P1", 0.0f, 1.0f, 0.0f);
  params[1] = Parameter::Knob("P2", 0.0f, 1.0f, 0.0f);

  MenuState menu;
  menu.param_count = 2;
  menu.selected_param = 0;

  CVMappingProcessor cv_proc; // Dummy
  EncoderController controller(cv_proc);

  // Simulate Encoder +1
  EncoderHardwareState hw = {1, false, false, 0};
  controller.Update(hw, menu, params, 2);

  EXPECT_EQ(menu.selected_param, 1);

  // Simulate Encoder -1
  hw.increment = -1;
  controller.Update(hw, menu, params, 2);

  EXPECT_EQ(menu.selected_param, 0);
}

TEST(EncoderController, EnterEditMode) {
  Parameter params[1];
  params[0] = Parameter::Knob("P1", 0.0f, 1.0f, 0.5f);

  MenuState menu;
  menu.param_count = 1;
  menu.state = UIState::Navigate;

  CVMappingProcessor cv_proc;
  EncoderController controller(cv_proc);

  // Short Press (Simulated via press_duration=50ms and rising_edge logic
  // assumed handled or simple flag check) Our Controller logic: just_released
  // && duration < thresh We simulate "Release" frame with press_duration set.

  EncoderHardwareState hw = {0, true, true, 0}; // Pressed
  controller.Update(hw, menu, params, 1);       // Processes press start

  hw = {0, false, false, 50}; // Released, 50ms duration
  controller.Update(hw, menu, params, 1);

  EXPECT_EQ(static_cast<int>(menu.state), static_cast<int>(UIState::EditValue));
}

TEST(EncoderController, EditValue) {
  Parameter params[1];
  params[0] = Parameter::Knob("P1", 0.0f, 1.0f, 0.5f);

  MenuState menu;
  menu.param_count = 1;
  menu.state = UIState::EditValue;

  CVMappingProcessor cv_proc;
  EncoderController controller(cv_proc);

  // Increment
  EncoderHardwareState hw = {1, false, false, 0};
  controller.Update(hw, menu, params, 1);

  // Should increase by 0.01 (Knob step)
  EXPECT_FLOAT_EQ(params[0].value, 0.51f);
}

TEST(EncoderController, SubmenuNavigation) {
  Parameter params[1];
  params[0] = Parameter::Knob("P1", 0.0f, 1.0f, 0.5f);

  MenuState menu;
  menu.param_count = 1;
  // Enter submenu manually for test setup
  menu.EnterSubmenu(0, ParamType::KNOB, params[0].mapping);

  CVMappingProcessor cv_proc;
  EncoderController controller(cv_proc);

  // Increment -> Next Item
  EncoderHardwareState hw = {1, false, false, 0};
  controller.Update(hw, menu, params, 1);

  // Item 0 is Mapping, 1 is CC (hidden if not CC), 2 is Plugged (hidden if not
  // CV), 3 is Attenuverter (hidden if not CV/CC), 4 Velocity Default mapping is
  // NONE. Visible items: Mapping(0), Velocity(4). Let's see
  // isSubmenuItemVisible logic in ui_state.h. Item 0: true Item 1 (CC): false
  // Item 2 (Plugged): false
  // Item 3 (Atten): false
  // Item 4 (Vel): true (unless param index 3)

  // So Next should go to 3 (Attenuverter is always visible for KNOB)
  EXPECT_EQ(menu.submenu_selected_item, 3);
}
