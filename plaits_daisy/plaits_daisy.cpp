/**
 * Plaits for Daisy Patch.Init()
 * 
 * Port of Mutable Instruments Plaits to Electrosmith Daisy Patch.Init() hardware.
 * 
 * Features:
 * - 24 synthesis engines from Plaits (3 banks of 8)
 * - 4 knobs (CV_1 to CV_4) for main parameters
 * - B7 button: short press = next engine/page, long press (>2s) = next bank/action
 * - B8 toggle switch: Play mode (up) / Parameters mode (down)
 * - LED: brightness shows engine (0-7), pulse pattern shows bank during selection
 * 
 * Play Mode (B8 up):
 * - CV_1: Frequency (pitch)
 * - CV_2: Harmonics
 * - CV_3: Timbre
 * - CV_4: Morph
 * 
 * Parameters Mode (B8 down):
 * - Page 0: Attenuverters (FM amt, Timbre mod, Morph mod, Harmonics mod)
 * - Page 1: LPG/Envelope (Decay, LPG Colour, Output Level) + long press = envelope mode
 * - Page 2: Tuning (Octave range, Fine tune)
 */

#include "daisy_patch_sm.h"
#include "daisysp.h"

// Plaits DSP includes
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

// LED controller for engine/bank display
#include "led_controller.h"

// Knob catch-up behavior for parameter pages
#include "knob_catcher.h"

// Persistent storage for settings
#include "util/PersistentStorage.h"

using namespace daisy;
using namespace patch_sm;

// =============================================================================
// Settings structure for QSPI storage
// =============================================================================

// Envelope modes
enum EnvelopeMode {
    ENV_DRONE = 0,    // Always plays, LPG bypassed
    ENV_PING = 1,     // Internal ping envelope  
    ENV_EXTERNAL = 2  // LPG follows CV_8 level
};

struct Settings {
    // Signature to validate stored data
    uint32_t signature;
    
    // Page 0: Attenuverters (-1 to +1)
    float fm_amount;           // Internal FM modulation depth
    float timbre_mod_amount;   // CV_6 → Timbre attenuverter
    float morph_mod_amount;    // CV_7 → Morph attenuverter
    float harmonics_mod_amount; // CV_8 → Harmonics attenuverter
    
    // Page 1: LPG / Envelope / Output
    float decay;               // Envelope decay (0-1)
    float lpg_colour;          // VCA to VCF blend (0-1)
    float output_level;        // Output volume (0-1)
    EnvelopeMode envelope_mode; // Drone/Ping/External
    
    // Page 2: Tuning
    int octave_range;          // 0-8 (default 4 = C4)
    float fine_tune;           // -1 to +1 (±1 semitone)
    
    // V/Oct Calibration
    // ADC reads 0.0-1.0 for -5V to +5V, so 0V = 0.5, 1V = 0.6
    // voct_offset: ADC value that corresponds to C4 (MIDI note 60)
    // voct_scale: semitones per ADC unit (ideal: 120 for 10V range)
    float voct_offset;         // ADC value for 0V (ideal: 0.5)
    float voct_scale;          // Semitones per ADC unit (ideal: 120)
    
    // Default values
    void Defaults() {
        signature = 0x504C5401;  // "PLT\x01"
        fm_amount = 0.0f;
        timbre_mod_amount = 0.0f;
        morph_mod_amount = 0.0f;
        harmonics_mod_amount = 0.0f;
        decay = 0.5f;
        lpg_colour = 0.5f;
        output_level = 0.7f;
        envelope_mode = ENV_PING;
        octave_range = 4;  // C4 (Middle C)
        fine_tune = 0.0f;
        // Default calibration (ideal values)
        voct_offset = 0.5f;      // 0V = 0.5 ADC
        voct_scale = 120.0f;     // 12 semitones per volt, 10V range = 120 semitones
    }
    
    // Required by PersistentStorage for change detection
    bool operator!=(const Settings& other) const {
        return signature != other.signature ||
               fm_amount != other.fm_amount ||
               timbre_mod_amount != other.timbre_mod_amount ||
               morph_mod_amount != other.morph_mod_amount ||
               harmonics_mod_amount != other.harmonics_mod_amount ||
               decay != other.decay ||
               lpg_colour != other.lpg_colour ||
               output_level != other.output_level ||
               envelope_mode != other.envelope_mode ||
               octave_range != other.octave_range ||
               fine_tune != other.fine_tune ||
               voct_offset != other.voct_offset ||
               voct_scale != other.voct_scale;
    }
};

constexpr uint32_t kSettingsSignature = 0x504C5401;  // "PLT\x01"

// =============================================================================
// UI Mode and Page State
// =============================================================================

enum UIMode {
    MODE_PLAY = 0,
    MODE_PARAMETERS = 1,
    MODE_CALIBRATION = 2  // V/Oct calibration mode
};

enum ParameterPage {
    PAGE_ATTENUVERTERS = 0,
    PAGE_LPG_ENVELOPE = 1,
    PAGE_TUNING = 2,
    PAGE_COUNT = 3
};

// Calibration steps
enum CalibrationStep {
    CAL_WAITING_LOW = 0,   // Waiting for user to send 1V and press button
    CAL_WAITING_HIGH = 1,  // Waiting for user to send 3V and press button
    CAL_DONE = 2           // Calibration complete, waiting for exit
};

// Current UI state
UIMode current_mode = MODE_PLAY;
ParameterPage current_page = PAGE_ATTENUVERTERS;
bool was_in_parameters_mode = false;  // Track mode changes for save trigger

// Calibration state
CalibrationStep cal_step = CAL_WAITING_LOW;
float cal_low_voltage = 0.0f;   // ADC reading at 1V
float cal_high_voltage = 0.0f;  // ADC reading at 3V

// =============================================================================
// Hardware
// =============================================================================

DaisyPatchSM hw;

// Button B7 for engine/bank selection (or page navigation in Parameters mode)
Switch button_b7;

// Toggle switch B8 for mode selection
Switch toggle_b8;

// Persistent storage for settings
PersistentStorage<Settings> storage(hw.qspi);
Settings* settings_ptr = nullptr;  // Will point to storage's settings after Init
#define settings (*settings_ptr)   // Macro for backwards compatibility
bool settings_dirty = false;  // Track if settings need saving

// LED controller for engine/bank display
led_controller::LedController led;

// Plaits Voice
plaits::Voice       voice;
plaits::Patch       patch;
plaits::Modulations modulations;

// Shared memory buffer for Plaits engines
// Plaits needs 16KB of shared buffer for its engines
char shared_buffer[16384];

// Audio block size - Plaits works with frames, we'll match Daisy's block size
constexpr size_t kBlockSize = 16;

// Output buffer for conversion from int16 to float
plaits::Voice::Frame output_frames[kBlockSize];

// Pot value filtering (low-pass filtered knob readings)
// Using same LP coefficients as original Plaits
float knob_lp[4] = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kKnobLpCoeff[4] = {
    0.005f,  // CV_1: Frequency (slow, smooth)
    0.005f,  // CV_2: Harmonics (slow, smooth)
    0.01f,   // CV_3: Timbre (medium)
    0.01f    // CV_4: Morph (medium)
};

// Knob catcher for play mode (when switching back from parameters mode)
plaits_daisy::KnobCatcherBank<4> play_mode_catchers;

// Knob catcher for parameters mode (when switching pages)
plaits_daisy::KnobCatcherBank<4> param_mode_catchers;

// Track previous page for detecting page changes
ParameterPage previous_page = PAGE_ATTENUVERTERS;

// Flag to prevent race condition: only write settings when catchers are synced with current page
volatile bool param_catchers_ready = true;

// Saved Play mode parameter values (stored when entering Parameters mode)
float saved_play_mode_values[4] = {0.5f, 0.5f, 0.5f, 0.5f};

// CV Input filtering (CV_5-8) - LP filtered
// CV_5: V/Oct, CV_6: Timbre CV, CV_7: Morph CV, CV_8: Harmonics/Level CV
float cv_lp[4] = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float kCvLpCoeff = 0.01f;  // Medium filtering for CV inputs

// Transposition parameter (for frequency knob, range -1 to +1)
float transposition = 0.0f;

// Output gain - combined with settings.output_level
constexpr float kBaseOutputGain = 0.15f;  // Base attenuation for non-eurorack

// Button timing for long press detection
constexpr float kLongPressThresholdMs = 2000.0f;  // 2 seconds for bank/action
bool button_was_pressed = false;
bool long_press_triggered = false;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Get current parameter values for a page (normalized 0-1)
 * Used when switching pages to set up knob catchers
 */
void GetPageParameterValues(ParameterPage page, float* values) {
    switch (page) {
        case PAGE_ATTENUVERTERS:
            // Attenuverters are -1 to +1, convert to 0-1
            values[0] = (settings.fm_amount + 1.0f) * 0.5f;
            values[1] = (settings.timbre_mod_amount + 1.0f) * 0.5f;
            values[2] = (settings.morph_mod_amount + 1.0f) * 0.5f;
            values[3] = (settings.harmonics_mod_amount + 1.0f) * 0.5f;
            break;
        case PAGE_LPG_ENVELOPE:
            values[0] = settings.decay;
            values[1] = settings.lpg_colour;
            values[2] = settings.output_level;
            values[3] = 0.5f;  // Reserved
            break;
        case PAGE_TUNING:
            values[0] = settings.octave_range / 8.99f;
            values[1] = (settings.fine_tune + 1.0f) * 0.5f;
            values[2] = 0.5f;  // Reserved
            values[3] = 0.5f;  // Reserved
            break;
        default:
            for (int i = 0; i < 4; i++) values[i] = 0.5f;
            break;
    }
}

/**
 * Get current play mode parameter values (normalized 0-1)
 */
void GetPlayModeParameterValues(float* values) {
    // These are already 0-1 in patch
    values[0] = (transposition + 1.0f) * 0.5f;  // Frequency knob position
    values[1] = patch.harmonics;
    values[2] = patch.timbre;
    values[3] = patch.morph;
}

/**
 * Apply settings to Plaits patch
 */
void ApplySettingsToPatch() {
    patch.frequency_modulation_amount = settings.fm_amount;
    patch.timbre_modulation_amount = settings.timbre_mod_amount;
    patch.morph_modulation_amount = settings.morph_mod_amount;
    patch.decay = settings.decay;
    patch.lpg_colour = settings.lpg_colour;
}

/**
 * Update modulations based on envelope mode
 */
void UpdateEnvelopeMode() {
    switch (settings.envelope_mode) {
        case ENV_DRONE:
            // Always plays, LPG bypassed
            modulations.trigger_patched = false;
            modulations.level_patched = false;
            break;
        case ENV_PING:
            // Internal ping envelope
            modulations.trigger_patched = true;
            modulations.level_patched = false;
            break;
        case ENV_EXTERNAL:
            // LPG follows CV_8 level
            modulations.trigger_patched = true;
            modulations.level_patched = true;
            break;
    }
}

/**
 * Cycle to next envelope mode
 */
void CycleEnvelopeMode() {
    settings.envelope_mode = static_cast<EnvelopeMode>(
        (settings.envelope_mode + 1) % 3
    );
    UpdateEnvelopeMode();
    settings_dirty = true;
}

/**
 * Initialize Plaits default patch settings
 */
void InitPatch() {
    patch.note = 48.0f;  // Base note (will be overwritten by controls)
    patch.harmonics = 0.5f;
    patch.timbre = 0.5f;
    patch.morph = 0.5f;
    patch.engine = 0;  // First engine (Virtual Analog VCF)
    
    // Apply stored settings
    ApplySettingsToPatch();
    
    // Sync LED controller with patch
    led.SetGlobalEngine(patch.engine);
}

/**
 * Initialize Plaits modulations (external CV inputs)
 */
void InitModulations() {
    modulations.engine = 0.0f;
    modulations.note = 0.0f;
    modulations.frequency = 0.0f;
    modulations.harmonics = 0.0f;
    modulations.timbre = 0.0f;
    modulations.morph = 0.0f;
    modulations.trigger = 0.0f;
    modulations.level = 1.0f;
    
    // Patched states - we always send CV so mark as patched
    modulations.frequency_patched = false;
    modulations.timbre_patched = true;   // CV_6 → Timbre
    modulations.morph_patched = true;    // CV_7 → Morph
    
    // Envelope mode determines trigger/level patched state
    UpdateEnvelopeMode();
}

/**
 * Audio callback - processes audio at sample rate
 */
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Process controls (read knobs)
    hw.ProcessAllControls();
    
    // Read Gate In 1 for trigger input
    // Plaits expects: >0.3 = trigger high, <0.1 = trigger low
    bool gate_state = hw.gate_in_1.State();
    modulations.trigger = gate_state ? 1.0f : 0.0f;
    
    // Apply low-pass filtering to knob readings (smoothing)
    for (int i = 0; i < 4; i++) {
        float raw = hw.GetAdcValue(CV_1 + i);
        knob_lp[i] += (raw - knob_lp[i]) * kKnobLpCoeff[i];
    }
    
    // Apply low-pass filtering to CV inputs (CV_5-8)
    for (int i = 0; i < 4; i++) {
        float raw = hw.GetAdcValue(CV_5 + i);
        cv_lp[i] += (raw - cv_lp[i]) * kCvLpCoeff;
    }
    
    // Convert CV inputs from 0.0-1.0 to bipolar -1.0 to +1.0
    // Patch.Init CV inputs: -5V to +5V mapped to 0.0-1.0, center = 0.5
    // Note: cv5_voct now uses calibration data
    float cv6_timbre = (cv_lp[1] - 0.5f) * 2.0f;   // Timbre mod
    float cv7_morph = (cv_lp[2] - 0.5f) * 2.0f;    // Morph mod  
    float cv8_harm_lvl = (cv_lp[3] - 0.5f) * 2.0f; // Harmonics or Level
    
    // Calculate V/Oct semitones using calibration
    // voct_offset is ADC value for 0V, voct_scale is semitones per ADC unit
    float voct_semitones = (cv_lp[0] - settings.voct_offset) * settings.voct_scale;
    
    // ==========================================================================
    // Mode-dependent knob handling with catch-up behavior
    // ==========================================================================
    
    if (current_mode == MODE_PLAY) {
        // --- PLAY MODE ---
        // Process knobs through catcher (handles mode transitions)
        float knob_val[4];
        for (int i = 0; i < 4; i++) {
            knob_val[i] = play_mode_catchers.Process(i, knob_lp[i]);
        }
        
        // Knob 1: Frequency/Transposition (range -1 to +1)
        transposition = knob_val[0] * 2.0f - 1.0f;
        
        // Calculate note based on octave range setting
        if (settings.octave_range < 8) {
            // Settings 0-7: Root note (C0-C7) with ±7 semitone knob range
            float root_note = 12.0f + (settings.octave_range * 12.0f);
            float knob_offset = transposition * 7.0f;  // ±7 semitones
            
            // Fine tune: ±1 semitone
            float fine_offset = settings.fine_tune;
            
            patch.note = root_note + knob_offset + voct_semitones + fine_offset;
        } else {
            // Setting 8: Full range C0 to C8
            patch.note = 12.0f + knob_val[0] * 96.0f;
        }
        
        // Knob 2-4: Harmonics, Timbre, Morph (0 to 1)
        patch.harmonics = knob_val[1];
        patch.timbre = knob_val[2];
        patch.morph = knob_val[3];
        
        // Apply CV modulation with attenuverters
        // CV_6 → Timbre (with attenuverter from settings)
        modulations.timbre = cv6_timbre * settings.timbre_mod_amount;
        
        // CV_7 → Morph (with attenuverter from settings)
        modulations.morph = cv7_morph * settings.morph_mod_amount;
        
        // CV_8 → Harmonics OR Level depending on envelope mode
        if (settings.envelope_mode == ENV_EXTERNAL) {
            // External mode: CV_8 controls level (0 to 1)
            modulations.level = (cv8_harm_lvl + 1.0f) * 0.5f;  // Convert -1..+1 to 0..1
            modulations.harmonics = 0.0f;
        } else {
            // Drone/Ping mode: CV_8 controls harmonics
            modulations.harmonics = cv8_harm_lvl * settings.harmonics_mod_amount;
            modulations.level = 1.0f;
        }
        
    } else {
        // --- PARAMETERS MODE ---
        // Process knobs through catcher for current page
        float knob_val[4];
        for (int i = 0; i < 4; i++) {
            knob_val[i] = param_mode_catchers.Process(i, knob_lp[i]);
        }
        
        // Only update settings if catchers are synchronized with current page
        // (prevents race condition during page transitions)
        if (!param_catchers_ready) {
            // Catchers not ready - don't write settings, just keep current values
        } else {
            switch (current_page) {
                case PAGE_ATTENUVERTERS:
                    // Page 0: Attenuverters
                    // CV_1: FM Amount (-1 to +1)
                    settings.fm_amount = knob_val[0] * 2.0f - 1.0f;
                    // CV_2: Timbre Mod attenuverter (-1 to +1)
                    settings.timbre_mod_amount = knob_val[1] * 2.0f - 1.0f;
                    // CV_3: Morph Mod attenuverter (-1 to +1)
                    settings.morph_mod_amount = knob_val[2] * 2.0f - 1.0f;
                    // CV_4: Harmonics Mod attenuverter (-1 to +1)
                    settings.harmonics_mod_amount = knob_val[3] * 2.0f - 1.0f;
                    break;
                    
                case PAGE_LPG_ENVELOPE:
                    // Page 1: LPG / Envelope / Output
                    // CV_1: Decay (0 to 1)
                    settings.decay = knob_val[0];
                    // CV_2: LPG Colour (0 to 1)
                    settings.lpg_colour = knob_val[1];
                    // CV_3: Output Level (0 to 1)
                    settings.output_level = knob_val[2];
                    // CV_4: Reserved
                    break;
                    
                case PAGE_TUNING:
                    // Page 2: Tuning
                    // CV_1: Octave Range (0 to 8, quantized)
                    settings.octave_range = static_cast<int>(knob_val[0] * 8.99f);
                    // CV_2: Fine Tune (-1 to +1 semitone)
                    settings.fine_tune = knob_val[1] * 2.0f - 1.0f;
                    // CV_3, CV_4: Reserved
                    break;
                    
                default:
                    break;
            }
        }
        
        // Apply settings to patch in real-time
        ApplySettingsToPatch();
        
        // In parameters mode, keep playing but use stored harmonics/timbre/morph
        // Calculate note same as play mode
        transposition = 0.5f * 2.0f - 1.0f;  // Center position
        float root_note = 12.0f + (settings.octave_range * 12.0f);
        patch.note = root_note + settings.fine_tune;
        
        // Set neutral modulations
        modulations.timbre = 0.0f;
        modulations.morph = 0.0f;
        modulations.harmonics = 0.0f;
    }
    
    // Apply internal FM amount from settings
    patch.frequency_modulation_amount = settings.fm_amount;
    
    // Calculate final output gain
    float output_gain = kBaseOutputGain * (0.1f + settings.output_level * 0.9f);
    
    // Render Plaits voice
    size_t frames_rendered = 0;
    while (frames_rendered < size) {
        size_t frames_to_render = std::min(kBlockSize, size - frames_rendered);
        
        voice.Render(patch, modulations, output_frames, frames_to_render);
        
        // Convert from int16 to float and write to output
        for (size_t i = 0; i < frames_to_render; i++) {
            out[0][frames_rendered + i] = static_cast<float>(output_frames[i].out) / 32768.0f * output_gain;
            out[1][frames_rendered + i] = static_cast<float>(output_frames[i].aux) / 32768.0f * output_gain;
        }
        
        frames_rendered += frames_to_render;
    }
}

// Debug: state names for logging
const char* StateToString(plaits_daisy::KnobState state) {
    switch (state) {
        case plaits_daisy::KNOB_TRACKING: return "TRK";
        case plaits_daisy::KNOB_WAITING: return "WAI";
        case plaits_daisy::KNOB_CATCHING_UP: return "CAT";
        default: return "???";
    }
}

// Engine names (24 engines, 3 banks of 8)
const char* kEngineNames[24] = {
    // Bank 0: Classic synthesis
    "VA",      // 0: Virtual Analog
    "WSHE",    // 1: Waveshaping
    "FM",      // 2: FM
    "GRAIN",   // 3: Grain
    "ADTV",    // 4: Additive
    "WT",      // 5: Wavetable
    "CHRD",    // 6: Chord
    "VOWL",    // 7: Vowel/Speech
    // Bank 1: Noise and percussion  
    "SWM",     // 8: Swarm
    "NOIS",    // 9: Noise
    "PART",    // 10: Particle
    "STR",     // 11: String (Karplus)
    "MODL",    // 12: Modal
    "BD",      // 13: Bass drum
    "SD",      // 14: Snare drum
    "HH",      // 15: Hi-hat
    // Bank 2: Special
    "VA2",     // 16: Virtual analog 2
    "WS2",     // 17: Waveshaping 2
    "FM2",     // 18: 2-op FM
    "GRN2",    // 19: Granular formant
    "ADD2",    // 20: Harmonic
    "WT2",     // 21: Wavetable 2
    "CHD2",    // 22: Chord 2
    "VOW2"     // 23: Vowel 2
};

const char* GetEngineName(int engine) {
    if (engine >= 0 && engine < 24) return kEngineNames[engine];
    return "??";
}

int main(void)
{
    // Initialize Daisy Patch.Init() hardware
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    // Start USB serial logging for debug output
    // Connect USB cable and use 'screen /dev/tty.usbmodem*' on macOS
    hw.StartLog(false);  // false = don't wait for terminal to connect
    
    // Initialize B7 button (momentary)
    // B7 is on GPIOB Pin 8
    button_b7.Init(Pin(PORTB, 8), 1000.0f, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED);
    
    // Initialize B8 toggle switch
    // B8 is on GPIOB Pin 9
    toggle_b8.Init(Pin(PORTB, 9), 1000.0f, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL);
    
    // Initialize persistent storage
    Settings default_settings;
    default_settings.Defaults();
    storage.Init(default_settings);
    
    // Get pointer to settings inside storage (so changes are saved)
    settings_ptr = &storage.GetSettings();
    
    // Validate signature - if invalid, use defaults
    if (settings.signature != kSettingsSignature) {
        settings.Defaults();
        settings.signature = kSettingsSignature;
        storage.Save();  // Save defaults with correct signature
    }
    
    // Initialize Plaits voice with buffer allocator
    stmlib::BufferAllocator allocator(shared_buffer, sizeof(shared_buffer));
    voice.Init(&allocator);
    
    // Initialize patch and modulations (uses loaded settings)
    InitPatch();
    InitModulations();
    
    // Initialize knob catchers
    // Play mode: use catch-up/skew for smooth transitions (important for performance)
    play_mode_catchers.Init(0.01f, true);
    // Parameters mode: no catch-up, just wait then track directly (simpler for editing)
    param_mode_catchers.Init(0.01f, false);
    
    // Start audio processing
    hw.StartAudio(AudioCallback);
    
    // Track previous mode for detecting mode changes
    UIMode previous_mode = MODE_PLAY;
    
    // Debug: periodic print timer
    uint32_t last_debug_print = 0;
    const uint32_t kDebugPrintIntervalMs = 200;  // Print every 200ms
    
    hw.PrintLine("Plaits Daisy started!");
    hw.PrintLine("Mode: PLAY, Page: 0");
    
    // Main loop - handle UI
    while(1) {
        uint32_t now_ms = System::GetNow();
        
        // Debounce controls
        button_b7.Debounce();
        toggle_b8.Debounce();
        
        // =======================================================================
        // Mode detection (B8 toggle)
        // =======================================================================
        // B8 UP = Play mode, B8 DOWN = Parameters mode
        // Note: Don't change mode if we're in calibration mode
        if (current_mode != MODE_CALIBRATION) {
            bool b8_is_down = toggle_b8.Pressed();
            current_mode = b8_is_down ? MODE_PARAMETERS : MODE_PLAY;
        }
        
        // Handle mode transitions for knob catching
        if (current_mode != previous_mode && current_mode != MODE_CALIBRATION) {
            // Get current knob ADC values for proper catcher initialization
            float current_adc[4] = {
                hw.GetAdcValue(CV_1),
                hw.GetAdcValue(CV_2),
                hw.GetAdcValue(CV_3),
                hw.GetAdcValue(CV_4)
            };
            
            if (current_mode == MODE_PLAY) {
                // Entering Play mode from Parameters mode
                // Use the SAVED play mode values (from when we entered Parameters mode)
                play_mode_catchers.OnPageChange(saved_play_mode_values, current_adc);
                
                // Save settings to QSPI
                storage.Save();
                settings_dirty = false;
            } else {
                // Entering Parameters mode from Play mode
                // SAVE current play mode values first!
                GetPlayModeParameterValues(saved_play_mode_values);
                
                // Set up param mode catchers with current page values
                float page_values[4];
                GetPageParameterValues(current_page, page_values);
                param_mode_catchers.OnPageChange(page_values, current_adc);
                param_catchers_ready = true;  // Catchers synced
            }
            previous_mode = current_mode;
        }
        
        // Handle page transitions in Parameters mode
        if (current_mode == MODE_PARAMETERS && current_page != previous_page) {
            // Page changed - set up catchers for new page
            float page_values[4];
            GetPageParameterValues(current_page, page_values);
            float current_adc[4] = {
                hw.GetAdcValue(CV_1),
                hw.GetAdcValue(CV_2),
                hw.GetAdcValue(CV_3),
                hw.GetAdcValue(CV_4)
            };
            param_mode_catchers.OnPageChange(page_values, current_adc);
            previous_page = current_page;
            param_catchers_ready = true;  // Catchers now synced with current page
        }
        
        // Track for save detection (legacy)
        was_in_parameters_mode = (current_mode == MODE_PARAMETERS);
        
        // =======================================================================
        // Button handling depends on mode
        // =======================================================================
        
        if (button_b7.RisingEdge()) {
            button_was_pressed = true;
            long_press_triggered = false;
        }
        
        if (button_was_pressed && button_b7.Pressed()) {
            if (!long_press_triggered && button_b7.TimeHeldMs() >= kLongPressThresholdMs) {
                long_press_triggered = true;
                
                if (current_mode == MODE_PLAY) {
                    // Long press in Play mode: change bank
                    led.NextBank();
                    patch.engine = led.GetGlobalEngine();
                } else if (current_mode == MODE_PARAMETERS) {
                    // Long press in Parameters mode: page-specific action
                    switch (current_page) {
                        case PAGE_ATTENUVERTERS:
                            // Reset all attenuverters to 0
                            settings.fm_amount = 0.0f;
                            settings.timbre_mod_amount = 0.0f;
                            settings.morph_mod_amount = 0.0f;
                            settings.harmonics_mod_amount = 0.0f;
                            // Put catchers in WAITING state with reset values (0.5 = center)
                            {
                                float reset_values[4] = {0.5f, 0.5f, 0.5f, 0.5f};
                                float current_adc[4] = {
                                    hw.GetAdcValue(CV_1),
                                    hw.GetAdcValue(CV_2),
                                    hw.GetAdcValue(CV_3),
                                    hw.GetAdcValue(CV_4)
                                };
                                param_mode_catchers.OnPageChange(reset_values, current_adc);
                            }
                            break;
                        case PAGE_LPG_ENVELOPE:
                            // Cycle envelope mode
                            CycleEnvelopeMode();
                            break;
                        case PAGE_TUNING:
                            // Enter V/Oct calibration mode
                            current_mode = MODE_CALIBRATION;
                            cal_step = CAL_WAITING_LOW;
                            hw.PrintLine("CALIBRATION: Send 1V to CV5, press button");
                            break;
                        default:
                            break;
                    }
                } else if (current_mode == MODE_CALIBRATION) {
                    // Long press in Calibration mode: exit without saving
                    current_mode = MODE_PARAMETERS;
                    hw.PrintLine("Calibration cancelled");
                }
            }
        }
        
        if (button_b7.FallingEdge()) {
            if (button_was_pressed && !long_press_triggered) {
                // Short press
                if (current_mode == MODE_PLAY) {
                    // Short press in Play mode: next engine
                    led.NextEngine();
                    patch.engine = led.GetGlobalEngine();
                } else if (current_mode == MODE_PARAMETERS) {
                    // Short press in Parameters mode: next page
                    param_catchers_ready = false;  // Prevent race condition
                    current_page = static_cast<ParameterPage>(
                        (current_page + 1) % PAGE_COUNT
                    );
                } else if (current_mode == MODE_CALIBRATION) {
                    // Short press in Calibration mode: capture voltage and advance
                    float cv5_adc = hw.GetAdcValue(CV_5);
                    
                    switch (cal_step) {
                        case CAL_WAITING_LOW:
                            // Capture 1V reading
                            cal_low_voltage = cv5_adc;
                            cal_step = CAL_WAITING_HIGH;
                            hw.PrintLine("Captured 1V: %d/1000", (int)(cal_low_voltage * 1000));
                            hw.PrintLine("Now send 3V to CV5, press button");
                            break;
                            
                        case CAL_WAITING_HIGH:
                            // Capture 3V reading
                            cal_high_voltage = cv5_adc;
                            hw.PrintLine("Captured 3V: %d/1000", (int)(cal_high_voltage * 1000));
                            
                            // Calculate calibration
                            // Between 1V and 3V there are 2 octaves = 24 semitones
                            // ADC difference should be ~0.2 (2V / 10V range)
                            {
                                float adc_delta = cal_high_voltage - cal_low_voltage;
                                if (adc_delta > 0.05f && adc_delta < 0.5f) {
                                    // Valid calibration
                                    // Scale: semitones per ADC unit
                                    // 24 semitones over adc_delta
                                    settings.voct_scale = 24.0f / adc_delta;
                                    
                                    // Offset: ADC value that corresponds to 0V
                                    // At 1V, ADC reads cal_low_voltage
                                    // 0V is 1V below that, so offset = cal_low_voltage - (1V in ADC)
                                    // 1V in ADC = 24 semitones / scale = adc_delta / 2
                                    float one_volt_adc = adc_delta / 2.0f;
                                    settings.voct_offset = cal_low_voltage - one_volt_adc;
                                    
                                    hw.PrintLine("Calibration OK!");
                                    hw.PrintLine("  Offset: %d/1000", (int)(settings.voct_offset * 1000));
                                    hw.PrintLine("  Scale: %d semi/unit", (int)settings.voct_scale);
                                    
                                    // Save calibration
                                    storage.Save();
                                } else {
                                    hw.PrintLine("ERROR: Invalid voltages!");
                                    hw.PrintLine("  Delta: %d/1000 (expected ~200)", (int)(adc_delta * 1000));
                                }
                            }
                            cal_step = CAL_DONE;
                            break;
                            
                        case CAL_DONE:
                            // Exit calibration mode
                            current_mode = MODE_PARAMETERS;
                            hw.PrintLine("Exited calibration");
                            break;
                    }
                }
            }
            button_was_pressed = false;
            long_press_triggered = false;
        }
        
        // =======================================================================
        // LED feedback
        // =======================================================================
        // In Play mode: show engine/bank
        // In Parameters mode: show page number (blink pattern)
        // In Calibration mode: special pattern for each step
        
        float led_voltage;
        if (current_mode == MODE_PLAY) {
            led_voltage = led.Update(now_ms);
        } else if (current_mode == MODE_CALIBRATION) {
            // Calibration mode: distinctive patterns
            switch (cal_step) {
                case CAL_WAITING_LOW:
                    // Single slow pulse
                    led_voltage = ((now_ms % 1500) < 200) ? 5.0f : 1.5f;
                    break;
                case CAL_WAITING_HIGH:
                    // Double pulse
                    {
                        uint32_t phase = now_ms % 1500;
                        led_voltage = (phase < 150 || (phase > 300 && phase < 450)) ? 5.0f : 1.5f;
                    }
                    break;
                case CAL_DONE:
                    // Steady bright
                    led_voltage = 5.0f;
                    break;
            }
        } else {
            // Parameters mode: different LED pattern for each page
            // Page 0: slow blink, Page 1: medium blink, Page 2: fast blink
            uint32_t period;
            switch (current_page) {
                case PAGE_ATTENUVERTERS: period = 1000; break;  // 1Hz
                case PAGE_LPG_ENVELOPE:  period = 500;  break;  // 2Hz
                case PAGE_TUNING:        period = 250;  break;  // 4Hz
                default:                 period = 1000; break;
            }
            bool led_on = ((now_ms % period) < (period / 2));
            led_voltage = led_on ? 3.5f : 1.5f;  // Toggle between dim and bright
        }
        hw.WriteCvOut(CV_OUT_2, led_voltage);
        
        // =======================================================================
        // Debug output (periodic)
        // =======================================================================
        if (now_ms - last_debug_print >= kDebugPrintIntervalMs) {
            last_debug_print = now_ms;
            
            // Get current knob ADC values
            float k1 = hw.GetAdcValue(CV_1);
            float k2 = hw.GetAdcValue(CV_2);
            float k3 = hw.GetAdcValue(CV_3);
            float k4 = hw.GetAdcValue(CV_4);
            
            if (current_mode == MODE_PLAY) {
                // Play mode: show bank, engine, knobs, states, and engine params
                int bank = patch.engine / 8;
                int eng_in_bank = patch.engine % 8;
                hw.PrintLine("PLAY B%d E%d(%s) | K:%d,%d,%d,%d | %s,%s,%s,%s",
                    bank, eng_in_bank, GetEngineName(patch.engine),
                    (int)(k1 * 100), (int)(k2 * 100), (int)(k3 * 100), (int)(k4 * 100),
                    StateToString(play_mode_catchers.GetState(0)),
                    StateToString(play_mode_catchers.GetState(1)),
                    StateToString(play_mode_catchers.GetState(2)),
                    StateToString(play_mode_catchers.GetState(3)));
                // Second line: actual engine parameter values (as integers to avoid float printf crash)
                hw.PrintLine("  Note:%d Harm:%d Timb:%d Morph:%d",
                    (int)patch.note, (int)(patch.harmonics * 100), 
                    (int)(patch.timbre * 100), (int)(patch.morph * 100));
            } else if (current_mode == MODE_CALIBRATION) {
                // Calibration mode: show CV_5 value and step
                float cv5 = hw.GetAdcValue(CV_5);
                const char* step_names[3] = {"1V", "3V", "DONE"};
                hw.PrintLine("CALIBRATE step:%s | CV5:%d/1000", 
                    step_names[cal_step], (int)(cv5 * 1000));
                hw.PrintLine("  Offset:%d Scale:%d", 
                    (int)(settings.voct_offset * 1000), (int)settings.voct_scale);
            } else {
                // Parameters mode: show page, knobs, states, and current page values
                const char* page_names[3] = {"ATTEN", "LPG", "TUNE"};
                hw.PrintLine("PARAM P%d(%s) | K:%d,%d,%d,%d | %s,%s,%s,%s",
                    current_page, page_names[current_page],
                    (int)(k1 * 100), (int)(k2 * 100), (int)(k3 * 100), (int)(k4 * 100),
                    StateToString(param_mode_catchers.GetState(0)),
                    StateToString(param_mode_catchers.GetState(1)),
                    StateToString(param_mode_catchers.GetState(2)),
                    StateToString(param_mode_catchers.GetState(3)));
                // Second line: current page parameter values (as integers)
                switch (current_page) {
                    case PAGE_ATTENUVERTERS:
                        // Values are -1 to +1, show as -100 to +100
                        hw.PrintLine("  FM:%d TiMod:%d MoMod:%d HaMod:%d",
                            (int)(settings.fm_amount * 100), (int)(settings.timbre_mod_amount * 100),
                            (int)(settings.morph_mod_amount * 100), (int)(settings.harmonics_mod_amount * 100));
                        break;
                    case PAGE_LPG_ENVELOPE:
                        // Values are 0 to 1, show as 0 to 100
                        hw.PrintLine("  Decay:%d LPG:%d Level:%d EnvMode:%d",
                            (int)(settings.decay * 100), (int)(settings.lpg_colour * 100),
                            (int)(settings.output_level * 100), settings.envelope_mode);
                        break;
                    case PAGE_TUNING:
                        // Octave is 0-8, fine tune is -1 to +1
                        hw.PrintLine("  Octave:%d FineTune:%d | Cal:%d,%d",
                            settings.octave_range, (int)(settings.fine_tune * 100),
                            (int)(settings.voct_offset * 1000), (int)settings.voct_scale);
                        break;
                    default:
                        break;
                }
            }
        }
        
        // Small delay
        System::DelayUs(500);
    }
}
