#include "../parameter.h"
#include "../ui_state.h"
#include "test_framework.h"

using namespace mutables_ui;

// Helper to print UIState
std::ostream &operator<<(std::ostream &os, const UIState &state) {
  os << static_cast<int>(state);
  return os;
}

// ============================================================================
// Navigation Tests
// ============================================================================

TEST(MenuState, NextParamWraps) {
  MenuState state;
  state.param_count = 3;
  state.selected_param = 0;

  state.NextParam(); // 1
  EXPECT_EQ(state.selected_param, 1);

  state.NextParam(); // 2
  EXPECT_EQ(state.selected_param, 2);

  state.NextParam(); // 0 (wraps)
  EXPECT_EQ(state.selected_param, 0);
}

TEST(MenuState, PrevParamWraps) {
  MenuState state;
  state.param_count = 3;
  state.selected_param = 0;

  state.PrevParam(); // 2 (wraps)
  EXPECT_EQ(state.selected_param, 2);

  state.PrevParam(); // 1
  EXPECT_EQ(state.selected_param, 1);
}

TEST(MenuState, ScrollToSelectedUpdatesOffset) {
  MenuState state;
  state.param_count = 10;
  // VISIBLE_PARAMS is typically 4. Let's assume 4 for this test or check
  // constant. If it's dynamic, we rely on the implementation. Assuming
  // VISIBLE_PARAMS = 4 based on typical OLED usage.

  state.selected_param = 5;
  state.ScrollToSelected();

  // selected (5) >= offset + 4
  // offset = 5 - 4 + 1 = 2
  EXPECT_EQ(state.scroll_offset, 2);
}

// ============================================================================
// SUB Navigation Tests
// ============================================================================

TEST(MenuState, EnterSubSetsParent) {
  Parameter children[2];
  Parameter sub = Parameter::Sub("Sub", children, 2);
  MenuState state;

  state.EnterSub(&sub, 0);
  EXPECT_TRUE(state.IsInSub());
  EXPECT_EQ(state.sub_parent, &sub);
  EXPECT_EQ(state.sub_parent_index, 0);
}

TEST(MenuState, ExitSubClearsParent) {
  Parameter children[2];
  Parameter sub = Parameter::Sub("Sub", children, 2);
  MenuState state;

  state.EnterSub(&sub, 0);
  state.ExitSub();

  EXPECT_FALSE(state.IsInSub());
  EXPECT_TRUE(state.sub_parent == nullptr);
}

TEST(MenuState, NextSubChildSkipsHidden) {
  Parameter children[3]; // 0=Vis, 1=Hidden, 2=Vis
  children[0] = Parameter::Knob("0");
  children[1] = Parameter::Knob("1");
  children[2] = Parameter::Knob("2");

  // Setup callback to hide index 1
  children[1].visibility_callback = [](const Parameter *, uint8_t,
                                       uint8_t idx) { return idx != 1; };

  Parameter sub = Parameter::Sub("Sub", children, 3);
  MenuState state;

  state.EnterSub(&sub, 0);
  // Should start at first visible (0)
  EXPECT_EQ(state.sub_child_selected, 0);

  state.NextSubChild();
  // Should skip 1 and go to 2
  EXPECT_EQ(state.sub_child_selected, 2);
}

// ============================================================================
// Submenu Tests (Context Menu)
// ============================================================================

TEST(MenuState, EnterSubmenuSetsState) {
  MenuState state;
  MappingConfig m;
  state.EnterSubmenu(0, ParamType::KNOB, m);

  EXPECT_EQ(state.state, UIState::Submenu);
  EXPECT_EQ(state.submenu_param_index, 0);
}

TEST(MenuState, ExitSubmenuRestoresNavigate) {
  MenuState state;
  MappingConfig m;
  state.EnterSubmenu(0, ParamType::KNOB, m);
  state.ExitSubmenu();

  EXPECT_EQ(state.state, UIState::Navigate);
}

TEST(MenuState, GetSubmenuItemCountForKnob) {
  MenuState state;
  MappingConfig m;
  // KNOB: Mapping, CC#, Plugged, Attenuverter, Velocity = 5 items
  EXPECT_EQ(state.GetSubmenuItemCount(ParamType::KNOB, m), 5);
}

TEST(MenuState, IsSubmenuItemVisibleForCCNumber) {
  MenuState state;
  MappingConfig m;

  // Default (No source) -> CC# (index 1) hidden
  m.source = MappingSource::NONE;
  EXPECT_FALSE(state.IsSubmenuItemVisible(ParamType::KNOB, 1, m));

  // CC Source -> CC# visible
  m.source = MappingSource::CC;
  EXPECT_TRUE(state.IsSubmenuItemVisible(ParamType::KNOB, 1, m));
}

// ============================================================================
// Preset Save (CharInput) Tests
// ============================================================================

TEST(MenuState, EnterCharInputInitializes) {
  MenuState state;
  state.EnterCharInput();

  EXPECT_EQ(state.state, UIState::CharInput);
  EXPECT_EQ(state.char_position, 0);
  EXPECT_EQ(state.char_index, 0); // 'a'
}

TEST(MenuState, ConfirmCharAddsToName) {
  MenuState state;
  state.EnterCharInput();

  // Assuming 'a' is default
  bool editing = state.ConfirmChar();
  EXPECT_TRUE(editing);
  EXPECT_STREQ(state.GetPresetName(), "a");
  EXPECT_EQ(state.char_position, 1);
}

// ============================================================================
// Preset Load (PresetList) Tests
// ============================================================================

TEST(MenuState, EnterPresetListSetsCount) {
  MenuState state;
  state.EnterPresetList(10);

  EXPECT_EQ(state.state, UIState::PresetList);
  EXPECT_EQ(state.preset_count, 10);
}

TEST(MenuState, NextPresetWrapsToTitle) {
  MenuState state;
  state.EnterPresetList(2);

  // 0 -> 1
  state.NextPreset();
  EXPECT_EQ(state.preset_selected, 1);
  EXPECT_FALSE(state.preset_title_selected);

  // 1 -> Title
  state.NextPreset();
  EXPECT_TRUE(state.preset_title_selected);

  // Title -> 0
  state.NextPreset();
  EXPECT_FALSE(state.preset_title_selected);
  EXPECT_EQ(state.preset_selected, 0);
}

// ============================================================================
// File Browser Tests
// ============================================================================

TEST(MenuState, EnterFileBrowserSetsCount) {
  MenuState state;
  state.EnterFileBrowser(0, 5);

  EXPECT_EQ(state.state, UIState::FileBrowser);
  EXPECT_EQ(state.file_count, 5);
}

TEST(MenuState, IsDefaultSelectedAtZero) {
  MenuState state;
  state.EnterFileBrowser(0, 5);

  EXPECT_EQ(state.file_selected, 0);
  EXPECT_TRUE(state.IsDefaultSelected());

  state.NextFile();
  EXPECT_EQ(state.file_selected, 1);
  EXPECT_FALSE(state.IsDefaultSelected());
}
