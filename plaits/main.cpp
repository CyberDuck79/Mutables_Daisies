#include "daisy_patch.h"
#include "daisysp.h"
#include "plaits_port.h"
#include "user_data_manager.h"
#include "logo_bitmap.h"
#include "encoder_handlers.h"
#include "cpu_monitor.h"
#include "../common/parameter.h"
#include "../common/ui_state.h"
#include "../common/cv_input.h"
#include "../common/display.h"
#include "../common/preset_manager.h"
#include "../common/constants.h"

using namespace daisy;
using namespace daisysp;
using namespace mutables_ui;
using namespace mutables_plaits;
using namespace mutables;

// Hardware
DaisyPatch hw;

// Module
PlaitsPort plaits_module;

// UI
MenuState menu;
Display display;
CVInputBank cv_inputs;

// SD Card / Presets / User Data
SdmmcHandler sdmmc;
FatFSInterface fsi;
PresetManager preset_manager;
UserDataManager user_data_manager;

// CPU Monitor
mutables::CpuMonitor cpu_monitor;

// Debug logger
daisy::Logger<daisy::LOGGER_INTERNAL> logger;
daisy::Logger<daisy::LOGGER_INTERNAL>* g_logger = &logger;

// Encoder state
bool encoder_button_last = false;
uint32_t encoder_press_time = 0;
const uint32_t LONG_PRESS_MS = 500;

// CC values storage (0.0 to 1.0)
float cc_values[128] = {0.0f};

// Sample-and-hold for Bank/Engine CV mapping
// When CV mapped with plugged=true, value is sampled on NoteOn
int bank_held_index = 0;
int engine_held_index = 0;
bool sample_hold_pending = false;  // Flag set on NoteOn to trigger sampling

// File browser state for USER_DATA selection
static constexpr int MAX_USER_DATA_FILES = 32;
static char user_data_files[MAX_USER_DATA_FILES][32];
static int user_data_file_count = 0;

// Callback for file browser display
const char* GetUserDataFileNameCallback(int index) {
    if (index >= 0 && index < user_data_file_count) {
        return user_data_files[index];
    }
    return nullptr;
}

// CV Mapping Cache - rebuild when mappings change
struct CVMappingCache {
    mutables_ui::Parameter* mapped_params[8];  // Max 8 params per CV
    uint8_t count;
    
    CVMappingCache() : count(0) {
        for (int i = 0; i < 8; i++) mapped_params[i] = nullptr;
    }
};

static CVMappingCache cv_mappings_[4];  // One per CV input
static CVMappingCache cc_mappings_[128];  // One per CC number
static bool mapping_cache_dirty_ = true;  // Rebuild on first audio callback

// Cached CV values (read once per block)
static float cached_cv_values_[4];

// Cached DAC values (write only when changed)
static uint16_t last_dac_1_ = 0;
static uint16_t last_dac_2_ = 0;

// Audio buffers
float* audio_in[4];
float* audio_out[4];

// Calculate parameter value with mapping applied
float CalculateMappedValue(const mutables_ui::Parameter& param, float base_value, const CVInputBank& cv_inputs) {
    const MappingConfig& m = param.mapping;
    
    if (m.source == MappingSource::NONE) {
        return base_value;
    }
    
    if (m.IsCVSource()) {
        float cv_value = cv_inputs.GetFiltered(m.GetCVIndex());
        
        if (m.plugged) {
            // Attenuverter emulation: cv_signal = current - offset
            float cv_signal = cv_value - m.offset;
            return std::clamp(m.offset + (cv_signal * m.attenuverter), 0.0f, 1.0f);
        } else {
            // Direct CV: just use the value
            return cv_value;
        }
    }
    
    // CC mapping would be handled elsewhere (MIDI callback)
    return base_value;
}

// Rebuild CV/CC mapping cache (call when mappings change)
void RebuildMappingCache() {
    // Clear all caches
    for (int i = 0; i < 4; i++) {
        cv_mappings_[i].count = 0;
    }
    for (int i = 0; i < 128; i++) {
        cc_mappings_[i].count = 0;
    }
    
    auto params = plaits_module.GetParameters();
    size_t param_count = plaits_module.GetParameterCount();
    
    // Build cache from main parameters
    for (size_t i = 0; i < param_count; i++) {
        auto& param = params[i];
        
        if (param.mapping.IsCVSource()) {
            int cv_idx = param.mapping.GetCVIndex();
            auto& cache = cv_mappings_[cv_idx];
            if (cache.count < 8) {
                cache.mapped_params[cache.count++] = &param;
            }
        } else if (param.mapping.source == MappingSource::CC) {
            int cc_num = param.mapping.cc_number;
            auto& cache = cc_mappings_[cc_num];
            if (cache.count < 8) {
                cache.mapped_params[cache.count++] = &param;
            }
        }
        
        // Process SUB children
        if (param.type == ParamType::SUB && param.children) {
            for (int j = 0; j < param.child_count; j++) {
                auto& child = param.children[j];
                
                if (child.mapping.IsCVSource()) {
                    int cv_idx = child.mapping.GetCVIndex();
                    auto& cache = cv_mappings_[cv_idx];
                    if (cache.count < 8) {
                        cache.mapped_params[cache.count++] = &child;
                    }
                } else if (child.mapping.source == MappingSource::CC) {
                    int cc_num = child.mapping.cc_number;
                    auto& cache = cc_mappings_[cc_num];
                    if (cache.count < 8) {
                        cache.mapped_params[cache.count++] = &child;
                    }
                }
            }
        }
    }
    
    mapping_cache_dirty_ = false;
}

// Calculate ENUM index from CV value with attenuverter
int CalculateEnumFromCV(const mutables_ui::Parameter& param, const CVInputBank& cv_inputs) {
    const MappingConfig& m = param.mapping;
    
    if (!m.IsCVSource()) return param.GetIndex();
    
    float cv_value = cv_inputs.GetFiltered(m.GetCVIndex());
    
    // If plugged, use offset-based attenuverter (like KNOB)
    float scaled;
    if (m.plugged) {
        float cv_signal = cv_value - m.offset;
        scaled = 0.5f + cv_signal * m.attenuverter;
    } else {
        // Without plugged, simple centered scaling
        scaled = 0.5f + (cv_value - 0.5f) * m.attenuverter;
    }
    scaled = std::clamp(scaled, 0.0f, 1.0f);
    
    // Quantize to enum count
    int index = static_cast<int>(scaled * param.enum_count);
    return std::clamp(index, 0, static_cast<int>(param.enum_count) - 1);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    // Start CPU measurement
    cpu_monitor.OnBlockStart();
    
    // Rebuild mapping cache if needed (happens when mappings change)
    if (mapping_cache_dirty_) {
        RebuildMappingCache();
    }
    
    // Update CV inputs with raw ADC values (no pot scaling or processing)
    // This preserves precision for V/Oct and accurate offset capture
    // Invert values since ADC reads are inverted on Daisy Patch (0V = 1.0, 5V = 0.0)
    float cv1 = 1.0f - hw.controls[DaisyPatch::CTRL_1].GetRawFloat();
    float cv2 = 1.0f - hw.controls[DaisyPatch::CTRL_2].GetRawFloat();
    float cv3 = 1.0f - hw.controls[DaisyPatch::CTRL_3].GetRawFloat();
    float cv4 = 1.0f - hw.controls[DaisyPatch::CTRL_4].GetRawFloat();
    
    cv_inputs.UpdateRawValues(
        std::clamp(cv1, 0.0f, 1.0f),
        std::clamp(cv2, 0.0f, 1.0f),
        std::clamp(cv3, 0.0f, 1.0f),
        std::clamp(cv4, 0.0f, 1.0f)
    );
    
    // Cache filtered CV values (read once per block)
    cached_cv_values_[0] = cv_inputs.GetFiltered(0);
    cached_cv_values_[1] = cv_inputs.GetFiltered(1);
    cached_cv_values_[2] = cv_inputs.GetFiltered(2);
    cached_cv_values_[3] = cv_inputs.GetFiltered(3);
    
    auto params = plaits_module.GetParameters();
    size_t param_count = plaits_module.GetParameterCount();
    
    // Calculate CV signals for Plaits modulation inputs
    // These are the raw CV signals (current - offset) that Plaits will attenuate
    float frequency_cv = 0.0f;
    float timbre_cv = 0.0f;
    float morph_cv = 0.0f;
    
    // Process CC-mapped parameters using cache
    for (int cc = 1; cc < 128; cc++) {
        auto& cache = cc_mappings_[cc];
        if (cache.count == 0) continue;
        
        float cc_value = cc_values[cc];
        
        for (uint8_t i = 0; i < cache.count; i++) {
            auto* param = cache.mapped_params[i];
            
            if (param->type == ParamType::KNOB) {
                param->SetNormalizedWithHysteresis(cc_value, kCVHysteresis);
            } else if (param->type == ParamType::ENUM) {
                int index = static_cast<int>(cc_value * param->enum_count);
                index = std::clamp(index, 0, static_cast<int>(param->enum_count) - 1);
                param->SetIndex(index);
            }
        }
    }
    
    // Process CV-mapped parameters using cache
    for (int cv = 0; cv < 4; cv++) {
        auto& cache = cv_mappings_[cv];
        if (cache.count == 0) continue;
        
        float cv_value = cached_cv_values_[cv];  // Already filtered, read once
        
        for (uint8_t i = 0; i < cache.count; i++) {
            auto* param = cache.mapped_params[i];
            
            if (param->type == ParamType::KNOB) {
                if (param->mapping.plugged) {
                    // Calculate CV signal for Plaits (raw signal without attenuverter)
                    float cv_signal = cv_value - param->mapping.offset;
                    
                    // Store CV signals for specific parameters that Plaits handles
                    // Frequency (index 2) -> frequency modulation
                    // Timbre (index 4) -> timbre modulation  
                    // Morph (index 5) -> morph modulation
                    if (param == &params[2]) frequency_cv = cv_signal;
                    else if (param == &params[4]) timbre_cv = cv_signal;
                    else if (param == &params[5]) morph_cv = cv_signal;
                }
                
                // For display purposes, update param.value with full calculation
                float mapped = CalculateMappedValue(*param, param->value, cv_inputs);
                param->SetNormalizedWithHysteresis(mapped, kCVHysteresis);
            }
            else if (param->type == ParamType::CV) {
                // CV type - direct read from CV input (no attenuverter emulation)
                param->SetNormalizedWithHysteresis(cv_value, kCVHysteresis);
            }
            else if (param->type == ParamType::ENUM) {
                // Bank (params[0]) and Engine (params[1]): Sample-and-hold on NoteOn when plugged
                if ((param == &params[0] || param == &params[1]) && param->mapping.plugged) {
                    // Sample on NoteOn, otherwise use held value
                    if (sample_hold_pending) {
                        int new_index = CalculateEnumFromCV(*param, cv_inputs);
                        if (param == &params[0]) bank_held_index = new_index;
                        else engine_held_index = new_index;
                        param->SetIndex(new_index);
                    } else {
                        // Use held value
                        param->SetIndex(param == &params[0] ? bank_held_index : engine_held_index);
                    }
                } else {
                    // Normal continuous CV control for other ENUMs or unplugged Bank/Engine
                    int new_index = CalculateEnumFromCV(*param, cv_inputs);
                    param->SetIndex(new_index);
                }
            }
        }
    }
    
    // Handle unmapped CV type parameters - set to 0
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].type == ParamType::CV && !params[i].mapping.IsCVSource()) {
            params[i].SetNormalizedWithHysteresis(0.0f, kCVHysteresis);
        }
        // Also check children in SUB menus
        if (params[i].type == ParamType::SUB && params[i].children) {
            for (int j = 0; j < params[i].child_count; j++) {
                auto& child = params[i].children[j];
                if (child.type == ParamType::CV && !child.mapping.IsCVSource()) {
                    child.SetNormalizedWithHysteresis(0.0f, kCVHysteresis);
                }
            }
        }
    }
    
    // Clear sample-and-hold pending flag after processing
    sample_hold_pending = false;
    
    // Pass CV modulation values to Plaits
    plaits_module.SetCVModulations(frequency_cv, timbre_cv, morph_cv);
    
    // Pass raw CV values for S&H source
    plaits_module.SetRawCVInputs(
        std::clamp(cv1, 0.0f, 1.0f),
        std::clamp(cv2, 0.0f, 1.0f),
        std::clamp(cv3, 0.0f, 1.0f),
        std::clamp(cv4, 0.0f, 1.0f)
    );
    
    // Process gate inputs for module
    // Gate 1: Trigger input for AD envelopes (and MIDI note triggers)
    // Gate 2: Clock input for LFO sync
    bool gate1_state = hw.gate_input[0].State();
    bool gate2_state = hw.gate_input[1].State();
    plaits_module.ProcessGate(0, gate1_state);
    plaits_module.ProcessGate(1, gate2_state);
    
    // Setup audio pointers
    for (size_t i = 0; i < 4; i++) {
        audio_in[i] = (float*)in[i];
        audio_out[i] = out[i];
    }
    
    // Clear all outputs first
    for (size_t i = 0; i < size; i++) {
        out[0][i] = 0.0f;
        out[1][i] = 0.0f;
        out[2][i] = 0.0f;
        out[3][i] = 0.0f;
    }
    
    // Process audio
    plaits_module.Process(audio_in, audio_out, size);
    
    // Update sample counter for MIDI clock tracking
    plaits_module.UpdateSampleCounter(size);
    
    // Write CV modulator outputs to DAC (0-1 float -> 0-4095 DAC value)
    // Channel 1 = CV Out 1, Channel 2 = CV Out 2
    // Only write if values changed (reduce SPI traffic)
    float cv_out_1 = plaits_module.GetCVOutput(0);
    float cv_out_2 = plaits_module.GetCVOutput(1);
    uint16_t dac_1 = static_cast<uint16_t>(std::clamp(cv_out_1, 0.0f, 1.0f) * 4095.0f);
    uint16_t dac_2 = static_cast<uint16_t>(std::clamp(cv_out_2, 0.0f, 1.0f) * 4095.0f);
    
    if (dac_1 != last_dac_1_) {
        hw.seed.dac.WriteValue(DacHandle::Channel::ONE, dac_1);
        last_dac_1_ = dac_1;
    }
    if (dac_2 != last_dac_2_) {
        hw.seed.dac.WriteValue(DacHandle::Channel::TWO, dac_2);
        last_dac_2_ = dac_2;
    }
    
    // Write Gate Output
    hw.gate_output.Write(plaits_module.GetGateOutput());
    
    // End CPU measurement
    cpu_monitor.OnBlockEnd();
}

// Forward declaration for AudioCallback restart
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);


void UpdateEncoder() {
    auto params = plaits_module.GetParameters();
    int encoder_increment = hw.encoder.Increment();
    bool encoder_rising = hw.encoder.RisingEdge();
    bool encoder_held = hw.encoder.Pressed();
    
    // Track press time for long press
    static uint32_t press_start = 0;
    if (encoder_rising) {
        press_start = System::GetNow();
    }
    
    bool long_press_detected = false;
    bool short_press = false;
    
    if (!encoder_held && encoder_button_last) {
        // Button just released
        uint32_t press_duration = System::GetNow() - press_start;
        if (press_duration >= LONG_PRESS_MS) {
            long_press_detected = true;
        } else {
            short_press = true;
        }
    }
    
    encoder_button_last = encoder_held;
    
    // Handle encoder based on state
    switch (menu.state) {
        case UIState::Navigate: {
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
                    menu.param_count = plaits_module.GetParameterCount();
                    menu.selected_param = (parent_idx >= 0) ? parent_idx : 0;
                    menu.ScrollToSelected();
                } else if (current_param.IsEditable()) {
                    menu.state = UIState::EditValue;
                } else if (current_param.type == ParamType::SAVE) {
                    // Enter preset save mode
                    if (preset_manager.IsSDAvailable()) {
                        menu.EnterCharInput();
                    } else {
                        // Show error - no SD card
                        display.RenderMessage("Error", "No SD Card");
                        hw.display.Update();
                        System::Delay(kMessageDisplayDelayMs);
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
                            System::Delay(kMessageDisplayDelayMs);
                        }
                    } else {
                        display.RenderMessage("Error", "No SD Card");
                        hw.display.Update();
                        System::Delay(kMessageDisplayDelayMs);
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
                        // Get the list of .bin files for this target
                        UserDataManager::Target target = static_cast<UserDataManager::Target>(current_param.user_data_target);
                        user_data_file_count = user_data_manager.ListFiles(target, user_data_files, MAX_USER_DATA_FILES);
                        
                        // Enter file browser mode
                        menu.EnterFileBrowser(menu.selected_param, user_data_file_count);
                    } else {
                        display.RenderMessage("Error", "No SD Card");
                        hw.display.Update();
                        System::Delay(kMessageDisplayDelayMs);
                    }
                }
            } else if (long_press_detected) {
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
            break;
        }
            
        case UIState::EditValue: {
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
            
            if (short_press || long_press_detected) {
                menu.state = UIState::Navigate;
            }
            break;
        }
            
        case UIState::Submenu: {
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
                        mapping_cache_dirty_ = true;  // Rebuild cache
                    } else {
                        // Enter edit mode for other items
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
                        mapping_cache_dirty_ = true;  // Rebuild cache
                    } else {
                        // Enter edit mode for other items
                        menu.state = UIState::SubmenuEdit;
                    }
                } else {
                    // Enter edit mode for this item
                    menu.state = UIState::SubmenuEdit;
                }
            }
            
            if (long_press_detected) {
                menu.ExitSubmenu();
            }
            break;
        }
            
        case UIState::SubmenuEdit: {
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
                            encoder_handlers::CycleMappingSource(param, encoder_increment);
                            mapping_cache_dirty_ = true;  // Rebuild cache
                            break;
                        case 1:  // CC Number (if CC mapped)
                            if (param.mapping.source == MappingSource::CC) {
                                param.mapping.cc_number += encoder_increment;
                                param.mapping.cc_number = std::clamp(param.mapping.cc_number, 1, 127);
                                mapping_cache_dirty_ = true;  // Rebuild cache for new CC
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
                        encoder_handlers::CycleMappingSource(param, encoder_increment);
                        mapping_cache_dirty_ = true;  // Rebuild cache
                    }
                } else if (param.type == ParamType::ENUM) {
                    switch (item) {
                        case 0:  // Mapping
                            encoder_handlers::CycleMappingSource(param, encoder_increment);
                            mapping_cache_dirty_ = true;  // Rebuild cache
                            break;
                        case 1:  // CC Number (if CC mapped)
                            if (param.mapping.source == MappingSource::CC) {
                                param.mapping.cc_number += encoder_increment;
                                param.mapping.cc_number = std::clamp(param.mapping.cc_number, 1, 127);
                                mapping_cache_dirty_ = true;  // Rebuild cache for new CC
                            }
                            break;
                        // case 2: Plugged - handled on short press, not editable
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
            
            if (short_press || long_press_detected) {
                menu.state = UIState::Submenu;
            }
            break;
        }
        
        case UIState::CharInput: {
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
            
            if (long_press_detected) {
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
                        plaits_module.GetParameterCount()
                    );
                    
                    // Restart audio
                    hw.StartAudio(AudioCallback);
                    
                    if (success) {
                        display.RenderMessage("Saved!", final_name);
                    } else {
                        display.RenderMessage("Error", "Save failed");
                    }
                    hw.display.Update();
                    System::Delay(kMessageDisplayDelayMs);
                }
                menu.ExitCharInput();
            }
            break;
        }
        
        case UIState::PresetList: {
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
                            plaits_module.GetParameterCount()
                        );
                        
                        // After loading preset, reload user data based on filenames
                        if (success) {
                            // Find the User Data SUB param and reload each target
                            for (size_t i = 0; i < plaits_module.GetParameterCount(); i++) {
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
                            plaits_module.ReloadUserData();
                            mapping_cache_dirty_ = true;  // Rebuild cache after preset load
                        }
                        
                        // Restart audio
                        hw.StartAudio(AudioCallback);
                        
                        if (success) {
                            display.RenderMessage("Loaded!", preset_name);
                        } else {
                            display.RenderMessage("Error", "Load failed");
                        }
                        hw.display.Update();
                        System::Delay(kMessageDisplayDelayMs);
                    }
                }
                menu.ExitPresetList();
            }
            
            if (long_press_detected) {
                // Exit without loading
                menu.ExitPresetList();
            }
            break;
        }
        
        case UIState::FileBrowser: {
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
                plaits_module.ReloadUserData();
                
                // Restart audio
                hw.StartAudio(AudioCallback);
                
                if (success) {
                    display.RenderMessage("Loaded!", filename ? filename : "Default");
                } else {
                    display.RenderMessage("Error", "Load failed");
                }
                hw.display.Update();
                System::Delay(kMessageDisplayDelayMs);
                
                menu.ExitFileBrowser();
            }
            
            if (long_press_detected) {
                // Exit without changing
                menu.ExitFileBrowser();
            }
            break;
        }
    }
}

void ProcessMidi() {
    while (hw.midi.HasEvents()) {
        MidiEvent event = hw.midi.PopEvent();
        
        // Handle MIDI clock (system realtime, not channel-dependent)
        if (event.type == SystemRealTime) {
            // MIDI Clock = 0xF8
            if (event.srt_type == TimingClock) {
                plaits_module.OnMIDIClock();
            }
        }
        
        // MIDI Thru: Forward all events to output regardless of channel
        // Reconstruct raw MIDI bytes and send
        if (event.type == NoteOn || event.type == NoteOff || event.type == ControlChange) {
            uint8_t status_byte = 0;
            if (event.type == NoteOn) status_byte = 0x90;
            else if (event.type == NoteOff) status_byte = 0x80;
            else if (event.type == ControlChange) status_byte = 0xB0;
            
            uint8_t bytes[3] = {
                static_cast<uint8_t>(status_byte | event.channel),
                event.data[0],
                event.data[1]
            };
            hw.midi.SendMessage(bytes, 3);
        }
        
        // Get MIDI channel filter setting: 0 = Omni, 1-16 = specific channel
        int midi_channel = plaits_module.GetMidiChannel();
        
        // Check if this event should be processed (Omni or matching channel)
        // Note: event.channel is 0-15, our setting is 0=Omni, 1-16=channel
        bool channel_match = (midi_channel == 0) || (event.channel == midi_channel - 1);
        
        if (!channel_match) continue;
        
        if (event.type == NoteOn) {
            NoteOnEvent note = event.AsNoteOn();
            if (note.velocity > 0) {
                // Trigger sample-and-hold for Bank/Engine CV
                sample_hold_pending = true;
                plaits_module.NoteOn(note.note, note.velocity);
            } else {
                plaits_module.NoteOff(note.note, 0);
            }
        } else if (event.type == NoteOff) {
            NoteOffEvent note = event.AsNoteOff();
            plaits_module.NoteOff(note.note, note.velocity);
        } else if (event.type == ControlChange) {
            ControlChangeEvent cc = event.AsControlChange();
            if (cc.control_number < 128) {
                cc_values[cc.control_number] = cc.value / 127.0f;
            }
        }
    }
}

// Helper function for preset list display
const char* GetPresetNameCallback(int index) {
    return preset_manager.GetPresetName(index);
}

void UpdateDisplay() {
    auto params = plaits_module.GetParameters();
    
    // Update CPU overload status for display alert
    display.SetCpuOverload(cpu_monitor.IsOverloaded());
    
    if (menu.state == UIState::CharInput) {
        display.RenderCharInput(menu);
    } else if (menu.state == UIState::PresetList) {
        display.RenderPresetList(menu, GetPresetNameCallback);
    } else if (menu.state == UIState::FileBrowser) {
        // File browser for USER_DATA selection
        // Get the title from the parameter being edited
        mutables_ui::Parameter* current_params = menu.IsInSub() && menu.sub_parent 
            ? menu.sub_parent->children 
            : params;
        const char* title = current_params[menu.file_browser_param_idx].name;
        display.RenderFileBrowser(menu, title, GetUserDataFileNameCallback);
    } else if (menu.IsInSubmenu() && menu.submenu_param_index >= 0) {
        // Get the correct parameter - either from SUB children or root params
        mutables_ui::Parameter* submenu_params = menu.IsInSub() && menu.sub_parent 
            ? menu.sub_parent->children 
            : params;
        display.RenderSubmenu(menu, submenu_params[menu.submenu_param_index]);
    } else if (menu.IsInSub() && menu.sub_parent) {
        // Browsing SUB's children with visibility support
        display.RenderSubMenu(menu, menu.sub_parent);
    } else {
        display.RenderMenu(menu, params);
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(24);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    // Initialize USB serial logger for debug
    logger.StartLog(false);  // Don't wait for PC
    logger.PrintLine("Plaits starting...");
    
    // Initialize SD card
    // Using official libDaisy initialization sequence with DMA
    // Buffer alignment handled in UserDataManager with DMA_BUFFER_MEM_SECTION
    {
        SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();  // FAST speed (50MHz), 4-bit width
        sdmmc.Init(sd_cfg);
        
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        System::Delay(100);  // Give card time to settle
        
        FRESULT fr = f_mount(&fsi.GetSDFileSystem(), "/", 1);
        if (fr == FR_OK) {
            logger.PrintLine("SD: Ready");
        } else {
            logger.PrintLine("SD: Mount failed (%d)", (int)fr);
        }
    }
    
    plaits_module.Init(48000.0f);
    
    // Initialize CPU monitor
    cpu_monitor.Init(48000.0f, hw.AudioBlockSize());
    
    // Initialize preset manager
    preset_manager.Init(sdmmc, fsi, plaits_module.GetShortName());
    logger.PrintLine("Preset manager initialized");
    
    // Try to load "default" preset if it exists
    if (preset_manager.IsSDAvailable()) {
        bool loaded = preset_manager.LoadPreset(
            "default",
            plaits_module.GetParameters(),
            plaits_module.GetParameterCount()
        );
        if (loaded) {
            logger.PrintLine("Loaded 'default' preset");
            // Rebuild CV mapping cache after loading preset
            mapping_cache_dirty_ = true;
        } else {
            logger.PrintLine("No 'default' preset found");
        }
    }
    
    // Initialize user data manager and load defaults from SD card
    user_data_manager.Init(fsi, plaits_module.GetShortName());
    user_data_manager.CreateDirectories();  // Create dirs if they don't exist
    user_data_manager.LoadDefaults();  // Logs internally
    
    // Sync user_data_params_ filenames with what was loaded
    // Find the User Data SUB param and update each child's filename
    auto params = plaits_module.GetParameters();
    for (size_t i = 0; i < plaits_module.GetParameterCount(); i++) {
        if (params[i].type == ParamType::SUB && params[i].children) {
            for (size_t c = 0; c < params[i].child_count; c++) {
                auto& child = params[i].children[c];
                if (child.type == ParamType::USER_DATA) {
                    UserDataManager::Target target = 
                        static_cast<UserDataManager::Target>(child.user_data_target);
                    const char* loaded_file = user_data_manager.GetCurrentFile(target);
                    if (loaded_file && loaded_file[0]) {
                        child.SetUserDataFile(loaded_file);
                    }
                }
            }
        }
    }
    
    // Register user data manager as the global provider for Plaits
    plaits::g_user_data_provider = &user_data_manager;
    plaits::g_user_data_provider = &user_data_manager;
    
    menu.param_count = plaits_module.GetParameterCount();
    display.Init(&hw, DrawPlaitsLogo);  // Pass logo drawing function
    
    display.RenderBootScreen("Plaitsy");
    System::Delay(3000);
    
    // Enable CPU logging now that init is complete
    cpu_monitor.EnableLogging(true);
    
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    hw.midi.StartReceive();
    
    uint32_t last_display_update = 0;
    while(1) {
        hw.midi.Listen();
        ProcessMidi();
        
        hw.ProcessAllControls();
        UpdateEncoder();
        
        uint32_t now = System::GetNow();
        
        // Periodic CPU logging (every 2 seconds in debug builds)
        cpu_monitor.Update(now, g_logger);
        
        if (now - last_display_update >= 16) {
            last_display_update = now;
            UpdateDisplay();
        }
        
        System::Delay(1);
    }
}
