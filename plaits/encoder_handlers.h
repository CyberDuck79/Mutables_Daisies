#pragma once
//=============================================================================
// Encoder state handlers - extracted from UpdateEncoder() for readability
// Each handler processes one UIState case
//=============================================================================

#include "daisy_patch.h"
#include "../common/parameter.h"
#include "../common/ui_state.h"
#include "../common/display.h"
#include "../common/preset_manager.h"
#include "../common/cv_input.h"
#include "../common/constants.h"
#include "user_data_manager.h"
#include "plaits_port.h"

namespace encoder_handlers {

using namespace mutables_ui;
using namespace mutables_plaits;
using namespace mutables;

//=============================================================================
// Helper function to cycle through mapping sources
//=============================================================================
inline void CycleMappingSource(mutables_ui::Parameter& param, int direction) {
    int current = static_cast<int>(param.mapping.source);
    
    if (param.type == ParamType::KNOB) {
        // KNOB: None, CV1-4, CC (skip Gate1, Gate2)
        do {
            current += direction;
            if (current < 0) current = static_cast<int>(MappingSource::CC);
            if (current > static_cast<int>(MappingSource::CC)) current = 0;
        } while (current == static_cast<int>(MappingSource::GATE1) || 
                 current == static_cast<int>(MappingSource::GATE2));
    } else if (param.type == ParamType::CV) {
        // CV: None, CV1-4 only
        current += direction;
        if (current < 0) current = static_cast<int>(MappingSource::CV4);
        if (current > static_cast<int>(MappingSource::CV4)) current = 0;
    } else if (param.type == ParamType::ENUM) {
        // ENUM: None, CV1-4, CC (skip Gate1, Gate2)
        do {
            current += direction;
            if (current < 0) current = static_cast<int>(MappingSource::CC);
            if (current > static_cast<int>(MappingSource::CC)) current = 0;
        } while (current == static_cast<int>(MappingSource::GATE1) || 
                 current == static_cast<int>(MappingSource::GATE2));
    }
    
    param.mapping.source = static_cast<MappingSource>(current);
}

//=============================================================================
// UIState::Navigate handler
//=============================================================================
inline void HandleNavigate(
    daisy::DaisyPatch& hw,
    MenuState& menu,
    Display& display,
    PlaitsPort& module,
    PresetManager& preset_manager,
    UserDataManager& user_data_manager,
    mutables_ui::Parameter* params,
    int encoder_increment,
    bool short_press,
    bool long_press,
    char user_data_files[][32],
    int& user_data_file_count,
    int max_user_data_files
) {
    if (menu.IsInSub()) {
        // Use visibility-aware navigation for SUB children
        if (encoder_increment > 0) menu.NextSubChild();
        if (encoder_increment < 0) menu.PrevSubChild();
        menu.selected_param = menu.sub_child_selected;
    } else {
        if (encoder_increment > 0) menu.NextParam();
        if (encoder_increment < 0) menu.PrevParam();
    }
    
    // Get the currently active parameter array
    mutables_ui::Parameter* current_params = menu.IsInSub() && menu.sub_parent 
        ? menu.sub_parent->children 
        : params;
    auto& current_param = current_params[menu.selected_param];
    
    if (short_press) {
        // Check if title is selected in SUB (acts as back)
        if (menu.IsInSub() && menu.IsSubTitleSelected()) {
            // Exit SUB back to root menu
            int parent_idx = menu.sub_parent_index;
            menu.ExitSub();
            menu.param_count = module.GetParameterCount();
            menu.selected_param = (parent_idx >= 0) ? parent_idx : 0;
            menu.ScrollToSelected();
        } else if (current_param.IsEditable()) {
            menu.state = UIState::EditValue;
        } else if (current_param.type == ParamType::SAVE) {
            // Enter preset save mode
            if (preset_manager.IsSDAvailable()) {
                menu.EnterCharInput();
            } else {
                display.RenderMessage("Error", "No SD Card");
                hw.display.Update();
                daisy::System::Delay(kMessageDisplayDelayMs);
            }
        } else if (current_param.type == ParamType::LOAD) {
            // Enter preset load mode
            if (preset_manager.IsSDAvailable()) {
                int count = preset_manager.ScanPresets();
                if (count > 0) {
                    menu.EnterPresetList(count);
                } else {
                    display.RenderMessage("Error", "No presets");
                    hw.display.Update();
                    daisy::System::Delay(kMessageDisplayDelayMs);
                }
            } else {
                display.RenderMessage("Error", "No SD Card");
                hw.display.Update();
                daisy::System::Delay(kMessageDisplayDelayMs);
            }
        } else if (current_param.type == ParamType::SUB) {
            // Enter SUB's children as new menu
            if (current_param.children && current_param.child_count > 0) {
                menu.EnterSub(&current_param, menu.selected_param);
                menu.param_count = current_param.child_count;
                menu.selected_param = menu.sub_child_selected >= 0 ? menu.sub_child_selected : 0;
            }
        } else if (current_param.type == ParamType::USER_DATA) {
            // Enter file browser for user data selection
            if (user_data_manager.IsInitialized()) {
                UserDataManager::Target target = static_cast<UserDataManager::Target>(current_param.user_data_target);
                user_data_file_count = user_data_manager.ListFiles(target, user_data_files, max_user_data_files);
                menu.EnterFileBrowser(menu.selected_param, user_data_file_count);
            } else {
                display.RenderMessage("Error", "No SD Card");
                hw.display.Update();
                daisy::System::Delay(kMessageDisplayDelayMs);
            }
        }
    } else if (long_press) {
        if (menu.IsInSub() && !menu.IsSubTitleSelected()) {
            // In SUB with a child selected - enter mapping submenu if param has mapping
            if (current_param.HasMapping()) {
                menu.EnterSubmenu(menu.sub_child_selected, 
                                 current_param.type,
                                 current_param.mapping);
            }
        } else if (current_param.HasMapping()) {
            // Enter mapping submenu for params that have one (KNOB, CV, ENUM)
            menu.EnterSubmenu(menu.selected_param, 
                             current_param.type,
                             current_param.mapping);
        }
    }
}

//=============================================================================
// UIState::EditValue handler
//=============================================================================
inline void HandleEditValue(
    MenuState& menu,
    mutables_ui::Parameter* params,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Get the currently active parameter array
    mutables_ui::Parameter* current_params = menu.IsInSub() && menu.sub_parent 
        ? menu.sub_parent->children 
        : params;
    auto& param = current_params[menu.selected_param];
    
    if (encoder_increment != 0) {
        // Block editing if parameter is CV-mapped and plugged
        bool is_cv_plugged = param.mapping.IsCVSource() && param.mapping.plugged;
        bool is_cc_mapped = param.mapping.source == MappingSource::CC;
        
        if (!is_cv_plugged && !is_cc_mapped) {
            float step = 0.01f;
            if (param.type == ParamType::ENUM || param.type == ParamType::MIDI) {
                step = 1.0f;
            }
            
            param.value += encoder_increment * step;
            param.value = std::clamp(param.value, param.min, param.max);
        }
    }
    
    if (short_press || long_press) {
        menu.state = UIState::Navigate;
    }
}

//=============================================================================
// UIState::Submenu handler
//=============================================================================
inline void HandleSubmenu(
    MenuState& menu,
    mutables_ui::Parameter* params,
    CVInputBank& cv_inputs,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Get the correct parameter - from SUB children if we're in a SUB, otherwise root params
    mutables_ui::Parameter* submenu_params = menu.IsInSub() && menu.sub_parent 
        ? menu.sub_parent->children 
        : params;
    auto& param = submenu_params[menu.submenu_param_index];
    
    // Navigate submenu items
    if (encoder_increment > 0) {
        menu.NextSubmenuItem(param.type, param.mapping);
    }
    if (encoder_increment < 0) {
        menu.PrevSubmenuItem(param.type, param.mapping);
    }
    
    if (short_press) {
        int item = menu.submenu_selected_item;
        
        if (param.type == ParamType::KNOB) {
            if (item == 2 && param.mapping.IsCVSource()) {
                // Plugged toggle - special handling
                param.mapping.plugged = !param.mapping.plugged;
                if (param.mapping.plugged) {
                    int cv_idx = param.mapping.GetCVIndex();
                    if (cv_idx >= 0) {
                        param.mapping.offset = cv_inputs.GetFiltered(cv_idx);
                    }
                }
            } else {
                menu.state = UIState::SubmenuEdit;
            }
        } else if (param.type == ParamType::ENUM) {
            if (item == 2 && param.mapping.IsCVSource()) {
                // Plugged toggle - special handling
                param.mapping.plugged = !param.mapping.plugged;
                if (param.mapping.plugged) {
                    int cv_idx = param.mapping.GetCVIndex();
                    if (cv_idx >= 0) {
                        param.mapping.offset = cv_inputs.GetFiltered(cv_idx);
                    }
                }
            } else {
                menu.state = UIState::SubmenuEdit;
            }
        } else {
            menu.state = UIState::SubmenuEdit;
        }
    }
    
    if (long_press) {
        menu.ExitSubmenu();
    }
}

//=============================================================================
// UIState::SubmenuEdit handler
//=============================================================================
inline void HandleSubmenuEdit(
    MenuState& menu,
    mutables_ui::Parameter* params,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Get the correct parameter - from SUB children if we're in a SUB, otherwise root params
    mutables_ui::Parameter* submenu_params = menu.IsInSub() && menu.sub_parent 
        ? menu.sub_parent->children 
        : params;
    auto& param = submenu_params[menu.submenu_param_index];
    int item = menu.submenu_selected_item;
    
    if (encoder_increment != 0) {
        if (param.type == ParamType::KNOB) {
            switch (item) {
                case 0:  // Mapping
                    CycleMappingSource(param, encoder_increment);
                    break;
                case 1:  // CC Number (if CC mapped)
                    if (param.mapping.source == MappingSource::CC) {
                        param.mapping.cc_number += encoder_increment;
                        param.mapping.cc_number = std::clamp(param.mapping.cc_number, 1, 127);
                    }
                    break;
                case 3:  // Attenuverter
                    param.mapping.attenuverter += encoder_increment * kEncoderStepMedium;
                    param.mapping.attenuverter = std::clamp(param.mapping.attenuverter, -1.0f, 1.0f);
                    break;
                case 4:  // Velocity
                    param.mapping.velocity_amount += encoder_increment * kEncoderStepMedium;
                    param.mapping.velocity_amount = std::clamp(param.mapping.velocity_amount, -1.0f, 1.0f);
                    break;
            }
        } else if (param.type == ParamType::CV) {
            if (item == 0) {  // Mapping
                CycleMappingSource(param, encoder_increment);
            }
        } else if (param.type == ParamType::ENUM) {
            switch (item) {
                case 0:  // Mapping
                    CycleMappingSource(param, encoder_increment);
                    break;
                case 1:  // CC Number (if CC mapped)
                    if (param.mapping.source == MappingSource::CC) {
                        param.mapping.cc_number += encoder_increment;
                        param.mapping.cc_number = std::clamp(param.mapping.cc_number, 1, 127);
                    }
                    break;
                case 3:  // Attenuverter (if CV or CC mapped)
                    if (param.mapping.IsCVSource() || param.mapping.source == MappingSource::CC) {
                        param.mapping.attenuverter += encoder_increment * kEncoderStepMedium;
                        param.mapping.attenuverter = std::clamp(param.mapping.attenuverter, -1.0f, 1.0f);
                    }
                    break;
                case 4:  // Trigger (if Gate mapped)
                    if (param.mapping.IsGateSource()) {
                        int t = static_cast<int>(param.mapping.trigger) + encoder_increment;
                        t = std::clamp(t, 0, 2);
                        param.mapping.trigger = static_cast<TriggerMode>(t);
                        // Reset action if now invalid
                        if (!menu.IsActionValidForTrigger(param.mapping.action, param.mapping.trigger)) {
                            param.mapping.action = EnumAction::INCREMENT;
                        }
                    }
                    break;
                case 5:  // Action (if Gate mapped)
                    if (param.mapping.IsGateSource()) {
                        int max_action = (param.mapping.trigger == TriggerMode::RISE_AND_FALL) ? 3 : 1;
                        int a = static_cast<int>(param.mapping.action) + encoder_increment;
                        a = std::clamp(a, 0, max_action);
                        param.mapping.action = static_cast<EnumAction>(a);
                    }
                    break;
            }
        }
    }
    
    if (short_press || long_press) {
        menu.state = UIState::Submenu;
    }
}

//=============================================================================
// UIState::CharInput handler (preset save)
//=============================================================================
inline void HandleCharInput(
    daisy::DaisyPatch& hw,
    MenuState& menu,
    Display& display,
    PlaitsPort& module,
    PresetManager& preset_manager,
    mutables_ui::Parameter* params,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Rotate through character set (and title)
    if (encoder_increment > 0) menu.NextChar();
    if (encoder_increment < 0) menu.PrevChar();
    
    if (short_press) {
        if (menu.char_title_selected) {
            // Title selected: cancel and exit
            menu.ExitCharInput();
        } else {
            // Confirm character and move to next (or backspace if space)
            menu.ConfirmChar();
        }
    }
    
    if (long_press) {
        // Save preset if name is valid (including current unconfirmed char)
        if (menu.IsFinalPresetNameValid()) {
            // Get final name including current character
            char final_name[MenuState::MAX_PRESET_NAME_LEN + 1];
            menu.GetFinalPresetName(final_name, sizeof(final_name));
            
            // Stop audio during SD write to prevent interrupt contention
            hw.StopAudio();
            
            bool success = preset_manager.SavePreset(
                final_name, 
                params, 
                module.GetParameterCount()
            );
            
            // Restart audio (caller must provide AudioCallback)
            // Note: This will be restarted by the caller
            
            if (success) {
                display.RenderMessage("Saved!", final_name);
            } else {
                display.RenderMessage("Error", "Save failed");
            }
            hw.display.Update();
            daisy::System::Delay(kMessageDisplayDelayMs);
        }
        menu.ExitCharInput();
    }
}

//=============================================================================
// UIState::PresetList handler (preset load)
//=============================================================================
inline void HandlePresetList(
    daisy::DaisyPatch& hw,
    MenuState& menu,
    Display& display,
    PlaitsPort& module,
    PresetManager& preset_manager,
    UserDataManager& user_data_manager,
    mutables_ui::Parameter* params,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Navigate preset list
    if (encoder_increment > 0) menu.NextPreset();
    if (encoder_increment < 0) menu.PrevPreset();
    
    if (short_press) {
        if (menu.preset_title_selected) {
            // Title selected: cancel and exit
            menu.ExitPresetList();
        } else if (menu.preset_count > 0) {
            const char* preset_name = preset_manager.GetPresetName(menu.GetSelectedPreset());
            if (preset_name) {
                // Stop audio during SD read to prevent interrupt contention
                hw.StopAudio();
                
                bool success = preset_manager.LoadPreset(
                    preset_name,
                    params,
                    module.GetParameterCount()
                );
                
                // After loading preset, reload user data based on filenames
                if (success) {
                    // Find the User Data SUB param and reload each target
                    for (size_t i = 0; i < module.GetParameterCount(); i++) {
                        if (params[i].type == ParamType::SUB && params[i].children) {
                            for (size_t c = 0; c < params[i].child_count; c++) {
                                auto& child = params[i].children[c];
                                if (child.type == ParamType::USER_DATA) {
                                    UserDataManager::Target target = 
                                        static_cast<UserDataManager::Target>(child.user_data_target);
                                    if (child.user_data_filename[0]) {
                                        user_data_manager.LoadTarget(target, child.user_data_filename);
                                    } else {
                                        user_data_manager.LoadDefaultForTarget(target);
                                    }
                                }
                            }
                        }
                    }
                    module.ReloadUserData();
                }
                
                // Note: Audio will be restarted by the caller
                
                if (success) {
                    display.RenderMessage("Loaded!", preset_name);
                } else {
                    display.RenderMessage("Error", "Load failed");
                }
                hw.display.Update();
                daisy::System::Delay(kMessageDisplayDelayMs);
            }
        }
        menu.ExitPresetList();
    }
    
    if (long_press) {
        // Exit without loading
        menu.ExitPresetList();
    }
}

//=============================================================================
// UIState::FileBrowser handler (user data selection)
//=============================================================================
inline void HandleFileBrowser(
    daisy::DaisyPatch& hw,
    MenuState& menu,
    Display& display,
    PlaitsPort& module,
    UserDataManager& user_data_manager,
    mutables_ui::Parameter* params,
    char user_data_files[][32],
    int user_data_file_count,
    int encoder_increment,
    bool short_press,
    bool long_press
) {
    // Navigate file list
    if (encoder_increment > 0) menu.NextFile();
    if (encoder_increment < 0) menu.PrevFile();
    
    if (short_press) {
        // Get the USER_DATA parameter we're editing
        mutables_ui::Parameter* current_params = menu.IsInSub() && menu.sub_parent 
            ? menu.sub_parent->children 
            : params;
        auto& user_data_param = current_params[menu.file_browser_param_idx];
        
        if (menu.IsDefaultSelected()) {
            // Clear filename to use firmware default
            user_data_param.SetUserDataFile("");
        } else {
            // Get selected file name (index - 1 because 0 is Default)
            int file_idx = menu.GetSelectedFile() - 1;
            if (file_idx >= 0 && file_idx < user_data_file_count) {
                user_data_param.SetUserDataFile(user_data_files[file_idx]);
            }
        }
        
        // Load the user data file
        UserDataManager::Target target = static_cast<UserDataManager::Target>(user_data_param.user_data_target);
        const char* filename = user_data_param.user_data_filename[0] ? user_data_param.user_data_filename : nullptr;
        
        // Stop audio during SD read
        hw.StopAudio();
        
        bool success;
        if (filename) {
            success = user_data_manager.LoadTarget(target, filename);
        } else {
            // Load default (no file - use firmware built-in)
            success = user_data_manager.LoadDefaultForTarget(target);
        }
        
        // Reload user data in voice
        module.ReloadUserData();
        
        // Note: Audio will be restarted by the caller
        
        if (success) {
            display.RenderMessage("Loaded!", filename ? filename : "Default");
        } else {
            display.RenderMessage("Error", "Load failed");
        }
        hw.display.Update();
        daisy::System::Delay(kMessageDisplayDelayMs);
        
        menu.ExitFileBrowser();
    }
    
    if (long_press) {
        // Exit without changing
        menu.ExitFileBrowser();
    }
}

} // namespace encoder_handlers
