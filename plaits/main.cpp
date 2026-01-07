#include "daisy_patch.h"
#include "daisysp.h"
#include "plaits_port.h"
#include "../common/parameter.h"
#include "../common/ui_state.h"
#include "../common/cv_input.h"
#include "../common/display.h"

using namespace daisy;
using namespace daisysp;
using namespace mutables_ui;
using namespace mutables_plaits;

// Hardware
DaisyPatch hw;

// Module
PlaitsPort plaits_module;

// UI
MenuState menu;
Display display;
CVInputBank cv_inputs;

// Encoder state
bool encoder_button_last = false;
uint32_t encoder_press_time = 0;
const uint32_t LONG_PRESS_MS = 500;

// Audio buffers
float* audio_in[4];
float* audio_out[4];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    // Update CV inputs (knobs + CV)
    // DaisyPatch knobs are indexed 0-3, CV inputs are on ADC channels 0-3
    float cv1 = hw.GetKnobValue(DaisyPatch::CTRL_1);
    float cv2 = hw.GetKnobValue(DaisyPatch::CTRL_2);
    float cv3 = hw.GetKnobValue(DaisyPatch::CTRL_3);
    float cv4 = hw.GetKnobValue(DaisyPatch::CTRL_4);
    
    cv_inputs.UpdateRawValues(
        std::clamp(cv1, 0.0f, 1.0f),
        std::clamp(cv2, 0.0f, 1.0f),
        std::clamp(cv3, 0.0f, 1.0f),
        std::clamp(cv4, 0.0f, 1.0f)
    );
    
    // Update parameters from CV mappings
    auto params = plaits_module.GetParameters();
    size_t param_count = plaits_module.GetParameterCount();
    for (size_t i = 0; i < param_count; i++) {
        auto& param = params[i];
        if (param.cv_mapping.active && param.cv_mapping.cv_input >= 0) {
            // Read actual hardware knob position (knob + CV on DaisyPatch)
            // Filtering is already applied in cv_inputs.GetFiltered()
            float knob_value = cv_inputs.GetFiltered(param.cv_mapping.cv_input);
            // Use minimal hysteresis to prevent noise (0.1% threshold)
            param.SetNormalizedWithHysteresis(knob_value, 0.001f);
        }
    }
    
    // Process gate inputs
    plaits_module.ProcessGate(0, hw.gate_input[0].State());
    
    // Setup audio pointers
    for (size_t i = 0; i < 4; i++) {
        audio_in[i] = (float*)in[i];
        audio_out[i] = out[i];
    }
    
    // Process audio
    plaits_module.Process(audio_in, audio_out, size);
}

void UpdateEncoder() {
    auto params = plaits_module.GetParameters();
    int encoder_increment = hw.encoder.Increment();
    bool encoder_button = hw.encoder.RisingEdge();
    bool encoder_held = hw.encoder.Pressed();
    
    // Track long press
    if (encoder_button && !encoder_button_last) {
        encoder_press_time = System::GetNow();
    }
    
    bool long_press = false;
    if (!encoder_held && encoder_button_last) {
        long_press = (System::GetNow() - encoder_press_time) > LONG_PRESS_MS;
    }
    
    encoder_button_last = encoder_held;
    
    // Handle encoder based on state
    switch (menu.state) {
        case UIState::Navigate:
            if (encoder_increment > 0) menu.NextParam();
            if (encoder_increment < 0) menu.PrevParam();
            
            if (encoder_button && !long_press) {
                menu.state = UIState::EditValue;
            } else if (long_press) {
                menu.EnterSubmenu(menu.selected_param);
            }
            break;
            
        case UIState::EditValue: {
            auto& param = params[menu.selected_param];
            
            if (encoder_increment != 0) {
                float step = 0.01f;
                if (param.type == ParamType::Enum || param.type == ParamType::Integer) {
                    step = 1.0f;
                }
                
                param.value += encoder_increment * step;
                param.value = std::clamp(param.value, param.min, param.max);
            }
            
            if (encoder_button) {
                menu.state = UIState::Navigate;
            }
            break;
        }
            
        case UIState::Submenu:
            // TODO: Implement submenu navigation
            if (long_press) {
                menu.ExitSubmenu();
            }
            break;
            
        case UIState::SubmenuEdit:
            // TODO: Implement submenu editing
            break;
    }
}

void UpdateDisplay() {
    auto params = plaits_module.GetParameters();
    
    if (menu.IsInSubmenu() && menu.submenu_param_index >= 0) {
        display.RenderSubmenu(menu, params[menu.submenu_param_index]);
    } else {
        display.RenderMenu(menu, params);
    }
}

int main(void) {
    // Initialize hardware
    hw.Init();
    hw.SetAudioBlockSize(24); // Plaits block size
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    // Initialize module
    plaits_module.Init(48000.0f);
    
    // Initialize UI
    menu.param_count = plaits_module.GetParameterCount();
    display.Init(&hw);
    
    // Start audio
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    
    // Main loop
    while(1) {
        // Process hardware controls (encoder, gates, etc.)
        hw.ProcessAllControls();
        
        // Update encoder
        UpdateEncoder();
        
        // Update display at ~60Hz
        UpdateDisplay();
        
        // Small delay to prevent overwhelming the display
        System::Delay(16);
    }
}
