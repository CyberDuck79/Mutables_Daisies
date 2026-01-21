#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "../common/constants.h"

namespace mutables_plaits {

using namespace mutables;

// CV Output modulator modes
enum class CVOutMode {
    LPG_ENV = 0,   // Replicate internal LPG envelope
    AD,            // AD envelope
    LFO,           // Low Frequency Oscillator
    FOLLOW_3,      // Follow audio input 3 envelope
    FOLLOW_4       // Follow audio input 4 envelope
};

// Gate output sub-modes
enum class GateMode {
    MIDI_GATE = 0, // Pass through MIDI gate (high when note on)
    END_ENV,       // Trigger when internal envelope finishes
    TRIGGER,       // Short trigger pulse on each note
    CLK_DIV        // Clock divider output
};

// LFO sync modes
enum class SyncMode {
    FREE = 0,      // Free running (rate in Hz)
    MIDI,          // Synced to MIDI clock
    GATE2          // Synced to Gate 2 clock input
};

// LFO waveform shapes
enum class LFOShape {
    SINE = 0,
    TRI,
    SAW,
    SQUARE,
    SH,           // Sample & Hold
    RAND_SMOOTH   // Random with slew
};

// S&H source options
enum class SHSource {
    RANDOM = 0,
    CV1,
    CV2,
    CV3,
    CV4
};

// Clock sync ratios
// T = triplet (2/3 of normal), D = dotted (3/2 of normal)
enum class ClockRatio {
    DIV_16 = 0,    // 1/16 = 0.0625
    DIV_16T,       // 1/16T = 0.0625 * 2/3 = 0.0417
    DIV_16D,       // 1/16D = 0.0625 * 1.5 = 0.09375
    DIV_8,         // 1/8 = 0.125
    DIV_8T,        // 1/8T = 0.125 * 2/3 = 0.0833
    DIV_8D,        // 1/8D = 0.125 * 1.5 = 0.1875
    DIV_4,         // 1/4 = 0.25
    DIV_4T,        // 1/4T = 0.25 * 2/3 = 0.167
    DIV_4D,        // 1/4D = 0.25 * 1.5 = 0.375
    DIV_2,         // 1/2 = 0.5
    DIV_2T,        // 1/2T = 0.5 * 2/3 = 0.333
    DIV_2D,        // 1/2D = 0.5 * 1.5 = 0.75
    DIV_1,         // 1 bar = 1.0
    DIV_2X,        // 2 bars = 2.0
    DIV_4X,        // 4 bars = 4.0
    DIV_8X,        // 8 bars = 8.0
    NUM_RATIOS
};

// Ratio values in quarter notes
inline constexpr float kClockRatioValues[] = {
    0.0625f,                   // 1/16
    0.0625f * 2.0f / 3.0f,     // 1/16T
    0.0625f * 1.5f,            // 1/16D
    0.125f,                    // 1/8
    0.125f * 2.0f / 3.0f,      // 1/8T
    0.125f * 1.5f,             // 1/8D
    0.25f,                     // 1/4
    0.25f * 2.0f / 3.0f,       // 1/4T
    0.25f * 1.5f,              // 1/4D
    0.5f,                      // 1/2
    0.5f * 2.0f / 3.0f,        // 1/2T
    0.5f * 1.5f,               // 1/2D
    1.0f,                      // 1 bar
    2.0f,                      // 2 bars
    4.0f,                      // 4 bars
    8.0f                       // 8 bars
};

class CVModulator {
public:
    CVModulator() { Init(); }
    
    void Init() {
        mode_ = CVOutMode::LPG_ENV;
        
        // AD params (log-scaled: 0.5ms-200ms attack, 5ms-2000ms release)
        attack_ = 0.01f;   // ~10ms default
        release_ = 0.3f;   // ~300ms default
        
        // LFO params
        lfo_shape_ = LFOShape::SINE;
        sync_mode_ = SyncMode::FREE;
        rate_ = 1.0f;      // 1 Hz default (or ratio index)
        amp_ = 1.0f;       // Full amplitude
        phase_offset_ = 0.0f;  // 0-1 normalized
        slew_amount_ = 0.5f;   // RndSmth slew amount (0=instant, 1=very slow)
        sh_source_ = SHSource::RANDOM;  // S&H source
        retrig_ = false;   // Reset LFO phase on note trigger
        
        // Follow mode params
        follow_scale_3_ = 1.0f;  // 0.0x to 2.0x scaling for IN3
        follow_scale_4_ = 1.0f;  // 0.0x to 2.0x scaling for IN4
        audio_env_value_3_ = 0.0f;
        audio_env_value_4_ = 0.0f;
        
        // State
        env_value_ = 0.0f;
        env_stage_ = 0;  // 0 = idle, 1 = attack, 2 = release
        lfo_phase_ = 0.0f;
        output_ = 0.0f;
        slew_output_ = 0.0f;
        
        // S&H / Random state
        sh_value_ = 0.0f;
        random_target_ = 0.0f;
        
        // Slew coefficient (will be set based on sample rate)
        slew_coeff_ = 0.1f;
        
        sample_rate_ = 48000.0f;
        block_rate_ = 2000.0f;  // 48000 / 24
    }
    
    void SetSampleRate(float sample_rate, size_t block_size) {
        sample_rate_ = sample_rate;
        block_rate_ = sample_rate / static_cast<float>(block_size);
        
        // Slew time ~2ms at block rate
        float slew_time = 0.002f;
        slew_coeff_ = 1.0f - expf(-1.0f / (slew_time * block_rate_));
        
        // Recalculate envelope coefficients with new block rate
        float attack_time = 0.0005f * powf(400.0f, attack_);
        cached_attack_coeff_ = 1.0f - expf(-1.0f / (attack_time * block_rate_));
        float release_time = 0.005f * powf(400.0f, release_);
        cached_release_coeff_ = 1.0f - expf(-1.0f / (release_time * block_rate_));
    }
    
    // Trigger for AD envelope and LFO retrig
    void Trigger() {
        if (mode_ == CVOutMode::AD) {
            env_stage_ = 1;  // Start attack
        }
        if (mode_ == CVOutMode::LFO && retrig_) {
            lfo_phase_ = 0.0f;  // Reset to start (phase_offset_ is applied during output)
        }
    }
    
    // Set audio envelope values for FOLLOW modes (0-1)
    void SetAudioEnvelope3(float env) {
        audio_env_value_3_ = env;
    }
    void SetAudioEnvelope4(float env) {
        audio_env_value_4_ = env;
    }
    
    // Process one block - returns output value (0.0 to 1.0)
    // cv_values: array of 4 CV input values (0-1) for S&H source
    float Process(float lpg_gain, float midi_clock_hz, float gate2_clock_hz, const float* cv_values = nullptr) {
        float raw_output = 0.0f;
        
        switch (mode_) {
            case CVOutMode::LPG_ENV:
                raw_output = lpg_gain;
                break;
                
            case CVOutMode::AD:
                raw_output = ProcessAD();
                break;
                
            case CVOutMode::LFO:
                raw_output = ProcessLFO(midi_clock_hz, gate2_clock_hz, cv_values);
                break;
                
            case CVOutMode::FOLLOW_3:
                // Output audio envelope 3 scaled
                raw_output = std::min(1.0f, audio_env_value_3_ * follow_scale_3_);
                break;
                
            case CVOutMode::FOLLOW_4:
                // Output audio envelope 4 scaled
                raw_output = std::min(1.0f, audio_env_value_4_ * follow_scale_4_);
                break;
        }
        
        // Apply slew to avoid stepping
        slew_output_ += slew_coeff_ * (raw_output - slew_output_);
        output_ = slew_output_;
        
        return output_ * amp_;
    }
    
    // Setters
    void SetMode(CVOutMode mode) { mode_ = mode; }
    void SetAttack(float attack) { 
        if (attack != last_attack_) {
            attack_ = attack;
            float attack_time = 0.0005f * powf(400.0f, attack_);
            cached_attack_coeff_ = 1.0f - expf(-1.0f / (attack_time * block_rate_));
            last_attack_ = attack;
        }
    }
    void SetRelease(float release) { 
        if (release != last_release_) {
            release_ = release;
            float release_time = 0.005f * powf(400.0f, release_);
            cached_release_coeff_ = 1.0f - expf(-1.0f / (release_time * block_rate_));
            last_release_ = release;
        }
    }
    void SetLFOShape(LFOShape shape) { lfo_shape_ = shape; }
    void SetSyncMode(SyncMode mode) { sync_mode_ = mode; }
    void SetRate(float rate) { rate_ = rate; }
    void SetAmp(float amp) { amp_ = amp; }
    void SetPhaseOffset(float phase) { phase_offset_ = phase; }
    void SetSlewAmount(float slew) { slew_amount_ = slew; }
    void SetSHSource(SHSource source) { sh_source_ = source; }
    void SetRetrig(bool retrig) { retrig_ = retrig; }
    void SetFollowScale3(float scale) { follow_scale_3_ = scale; }
    void SetFollowScale4(float scale) { follow_scale_4_ = scale; }
    
    // Getters
    CVOutMode GetMode() const { return mode_; }
    float GetOutput() const { return output_ * amp_; }
    
private:
    // AD envelope processing
    float ProcessAD() {
        if (env_stage_ == 0) {
            return 0.0f;
        }
        
        // Use cached coefficients (computed in SetAttack/SetRelease)
        // This avoids expensive powf/expf every block
        
        if (env_stage_ == 1) {
            // Attack phase
            env_value_ += cached_attack_coeff_ * (1.0f - env_value_);
            if (env_value_ >= 0.99f) {
                env_value_ = 1.0f;
                env_stage_ = 2;  // Move to release
            }
        } else if (env_stage_ == 2) {
            // Release phase
            env_value_ -= cached_release_coeff_ * env_value_;
            if (env_value_ <= kEnvNearZero) {
                env_value_ = 0.0f;
                env_stage_ = 0;  // Done
            }
        }
        
        return env_value_;
    }
    
    // LFO processing
    float ProcessLFO(float midi_clock_hz, float gate2_clock_hz, const float* cv_values) {
        // Calculate frequency based on sync mode
        float freq;
        float clock_hz = 0.0f;
        
        switch (sync_mode_) {
            case SyncMode::MIDI:
                clock_hz = midi_clock_hz;
                break;
            case SyncMode::GATE2:
                clock_hz = gate2_clock_hz;
                break;
            default:
                break;
        }
        
        if (sync_mode_ != SyncMode::FREE && clock_hz > 0.0f) {
            // Clock sync mode: rate_ is 0-1, map to ratio index
            int ratio_idx = static_cast<int>(rate_ * (static_cast<float>(ClockRatio::NUM_RATIOS) - 1.0f) + 0.5f);
            if (ratio_idx < 0) ratio_idx = 0;
            if (ratio_idx >= static_cast<int>(ClockRatio::NUM_RATIOS)) {
                ratio_idx = static_cast<int>(ClockRatio::NUM_RATIOS) - 1;
            }
            // MIDI clock is 24 ppq, midi_clock_hz is clocks per second
            // Gate2 clock is assumed to be 1 ppq (quarter note)
            float quarter_note_hz = (sync_mode_ == SyncMode::MIDI) ? clock_hz / 24.0f : clock_hz;
            freq = quarter_note_hz / kClockRatioValues[ratio_idx];
        } else {
            // Free running mode: rate_ is 0-1 mapped to 0.1-20 Hz (log)
            freq = 0.1f * powf(200.0f, rate_);
        }
        
        // Advance phase
        float phase_inc = freq / block_rate_;
        lfo_phase_ += phase_inc;
        
        // Wrap phase
        bool phase_wrapped = false;
        while (lfo_phase_ >= 1.0f) {
            lfo_phase_ -= 1.0f;
            phase_wrapped = true;
        }
        
        // Apply phase offset
        float phase = lfo_phase_ + phase_offset_;
        while (phase >= 1.0f) phase -= 1.0f;
        
        // Generate waveform (output -1 to 1, then scale to 0-1)
        float wave = 0.0f;
        
        switch (lfo_shape_) {
            case LFOShape::SINE:
                wave = sinf(phase * 2.0f * 3.14159265f);
                break;
                
            case LFOShape::TRI:
                wave = phase < 0.5f 
                    ? 4.0f * phase - 1.0f 
                    : 3.0f - 4.0f * phase;
                break;
                
            case LFOShape::SAW:
                wave = 2.0f * phase - 1.0f;
                break;
                
            case LFOShape::SQUARE:
                wave = phase < 0.5f ? 1.0f : -1.0f;
                break;
                
            case LFOShape::SH:
                // Sample new value when phase wraps
                if (phase_wrapped) {
                    if (sh_source_ == SHSource::RANDOM) {
                        // Random source: -1 to 1
                        sh_value_ = 2.0f * (static_cast<float>(rand()) / RAND_MAX) - 1.0f;
                    } else if (cv_values != nullptr) {
                        // CV source: convert 0-1 to -1 to 1
                        int cv_idx = static_cast<int>(sh_source_) - 1;  // CV1=0, CV2=1, etc.
                        if (cv_idx >= 0 && cv_idx < 4) {
                            sh_value_ = 2.0f * cv_values[cv_idx] - 1.0f;
                        }
                    }
                }
                wave = sh_value_;
                break;
                
            case LFOShape::RAND_SMOOTH:
                // Pick new random target when phase wraps, slew to it
                if (phase_wrapped) {
                    random_target_ = 2.0f * (static_cast<float>(rand()) / RAND_MAX) - 1.0f;
                }
                // Configurable slew: slew_amount_ 0=fast (0.5 coeff), 1=slow (0.01 coeff)
                {
                    float slew_coeff = 0.5f * powf(0.02f, slew_amount_);  // 0.5 to 0.01
                    sh_value_ += slew_coeff * (random_target_ - sh_value_);
                }
                wave = sh_value_;
                break;
        }
        
        // Scale from -1..1 to 0..1
        return (wave + 1.0f) * 0.5f;
    }
    
    // Parameters
    CVOutMode mode_;
    float attack_;
    float release_;
    LFOShape lfo_shape_;
    SyncMode sync_mode_;
    float rate_;
    float amp_;
    float phase_offset_;
    float slew_amount_;
    SHSource sh_source_;
    bool retrig_;
    float follow_scale_3_;
    float follow_scale_4_;
    
    // State
    float env_value_;
    int env_stage_;
    float lfo_phase_;
    float output_;
    float slew_output_;
    float sh_value_;
    float random_target_;
    float slew_coeff_;
    
    // Follow mode state
    float audio_env_value_3_;
    float audio_env_value_4_;
    
    float sample_rate_;
    float block_rate_;
    
    // AD envelope coefficient cache (avoid expensive powf/expf every block)
    float cached_attack_coeff_ = 0.0f;
    float cached_release_coeff_ = 0.0f;
    float last_attack_ = -1.0f;
    float last_release_ = -1.0f;
};

// MIDI clock tracker
class MIDIClockTracker {
public:
    MIDIClockTracker() { Init(); }
    
    void Init() {
        last_clock_time_ = 0;
        clock_period_samples_ = 0;
        clock_hz_ = 0.0f;
        clock_count_ = 0;
    }
    
    // Call when MIDI clock message received
    void OnClock(uint32_t current_sample) {
        if (last_clock_time_ > 0 && current_sample > last_clock_time_) {
            uint32_t period = current_sample - last_clock_time_;
            // Simple averaging filter
            if (clock_period_samples_ == 0) {
                clock_period_samples_ = period;
            } else {
                clock_period_samples_ = (clock_period_samples_ * 7 + period) / 8;
            }
        }
        last_clock_time_ = current_sample;
        clock_count_++;
    }
    
    // Get estimated MIDI clock frequency in Hz (clocks per second)
    float GetClockHz(float sample_rate) const {
        if (clock_period_samples_ == 0) return 0.0f;
        return sample_rate / static_cast<float>(clock_period_samples_);
    }
    
    // Check if clock is active (received recently)
    bool IsActive(uint32_t current_sample, float sample_rate) const {
        // Consider inactive if no clock for > 2 seconds
        if (last_clock_time_ == 0) return false;
        uint32_t timeout = static_cast<uint32_t>(sample_rate * 2.0f);
        return (current_sample - last_clock_time_) < timeout;
    }
    
private:
    uint32_t last_clock_time_;
    uint32_t clock_period_samples_;
    float clock_hz_;
    uint32_t clock_count_;
};

// Gate clock tracker (tracks rising edges of gate input)
class GateClockTracker {
public:
    GateClockTracker() { Init(); }
    
    void Init() {
        last_clock_time_ = 0;
        clock_period_samples_ = 0;
        last_gate_state_ = false;
    }
    
    // Call each block with current gate state and sample position
    void Process(bool gate_state, uint32_t current_sample) {
        // Detect rising edge
        if (gate_state && !last_gate_state_) {
            if (last_clock_time_ > 0 && current_sample > last_clock_time_) {
                uint32_t period = current_sample - last_clock_time_;
                // Simple averaging filter
                if (clock_period_samples_ == 0) {
                    clock_period_samples_ = period;
                } else {
                    clock_period_samples_ = (clock_period_samples_ * 7 + period) / 8;
                }
            }
            last_clock_time_ = current_sample;
        }
        last_gate_state_ = gate_state;
    }
    
    // Get estimated clock frequency in Hz (rising edges per second)
    float GetClockHz(float sample_rate) const {
        if (clock_period_samples_ == 0) return 0.0f;
        return sample_rate / static_cast<float>(clock_period_samples_);
    }
    
    // Check if clock is active (received recently)
    bool IsActive(uint32_t current_sample, float sample_rate) const {
        // Consider inactive if no clock for > 2 seconds
        if (last_clock_time_ == 0) return false;
        uint32_t timeout = static_cast<uint32_t>(sample_rate * 2.0f);
        return (current_sample - last_clock_time_) < timeout;
    }
    
    // Get last gate state (for edge detection before Process)
    bool GetLastState() const { return last_gate_state_; }
    
private:
    uint32_t last_clock_time_;
    uint32_t clock_period_samples_;
    bool last_gate_state_;
};

} // namespace mutables_plaits
