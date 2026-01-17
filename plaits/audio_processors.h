#pragma once

#include <cmath>
#include <cstdint>

namespace mutables_plaits {

// Audio-derived modulation modes
enum class AudioEnvMode {
    OFF = 0,
    ENV,        // Envelope follower
    TRIG        // Transient detector
};

//=============================================================================
// EnvelopeFollower
// Extracts amplitude envelope from audio signal for modulation
//=============================================================================
class EnvelopeFollower {
public:
    EnvelopeFollower() = default;
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        envelope_ = 0.0f;
        SetAttack(10.0f);   // Default 10ms attack
        SetRelease(100.0f); // Default 100ms release
    }
    
    // Process a single sample and return envelope value (0.0 - 1.0)
    float Process(float input) {
        // Rectify (absolute value)
        float rect = std::abs(input);
        
        // Attack/release follower
        if (rect > envelope_) {
            // Attack: fast rise
            envelope_ += attack_coeff_ * (rect - envelope_);
        } else {
            // Release: slower fall
            envelope_ += release_coeff_ * (rect - envelope_);
        }
        
        return envelope_;
    }
    
    // Process a block and return the final envelope value
    float ProcessBlock(const float* input, size_t size) {
        for (size_t i = 0; i < size; i++) {
            Process(input[i]);
        }
        return envelope_;
    }
    
    // Set attack time in milliseconds (0.5ms - 200ms recommended)
    void SetAttack(float attack_ms) {
        // Clamp to reasonable range
        attack_ms = std::max(0.5f, std::min(attack_ms, 500.0f));
        // Time constant: coeff = 1 - exp(-1 / (time_sec * sample_rate))
        float time_sec = attack_ms * 0.001f;
        attack_coeff_ = 1.0f - std::exp(-1.0f / (time_sec * sample_rate_));
    }
    
    // Set release time in milliseconds (5ms - 2000ms recommended)
    void SetRelease(float release_ms) {
        release_ms = std::max(1.0f, std::min(release_ms, 5000.0f));
        float time_sec = release_ms * 0.001f;
        release_coeff_ = 1.0f - std::exp(-1.0f / (time_sec * sample_rate_));
    }
    
    // Set attack from normalized 0-1 value (log scaled: 0.5ms - 200ms)
    void SetAttackNormalized(float value) {
        // Log scale: 0.5ms at 0, 200ms at 1
        float attack_ms = 0.5f * std::pow(400.0f, value);
        SetAttack(attack_ms);
    }
    
    // Set release from normalized 0-1 value (log scaled: 5ms - 2000ms)
    void SetReleaseNormalized(float value) {
        // Log scale: 5ms at 0, 2000ms at 1
        float release_ms = 5.0f * std::pow(400.0f, value);
        SetRelease(release_ms);
    }
    
    float GetEnvelope() const { return envelope_; }
    void Reset() { envelope_ = 0.0f; }
    
private:
    float sample_rate_ = 48000.0f;
    float envelope_ = 0.0f;
    float attack_coeff_ = 0.1f;
    float release_coeff_ = 0.01f;
};

//=============================================================================
// TransientDetector
// Detects transients in audio and outputs triggers
//=============================================================================
class TransientDetector {
public:
    TransientDetector() = default;
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        envelope_ = 0.0f;
        prev_envelope_ = 0.0f;
        holdoff_counter_ = 0;
        trigger_counter_ = 0;
        SetThreshold(0.3f);
        SetHoldoff(50.0f);  // 50ms default holdoff
    }
    
    // Process a single sample, returns true if trigger detected
    bool Process(float input) {
        // Fast envelope follower for transient detection
        float rect = std::abs(input);
        
        // Very fast attack, medium release for transient detection
        if (rect > envelope_) {
            envelope_ = rect;  // Instant attack
        } else {
            envelope_ *= 0.9995f;  // ~30ms release at 48kHz
        }
        
        bool trigger = false;
        
        // Decrement holdoff
        if (holdoff_counter_ > 0) {
            holdoff_counter_--;
        }
        
        // Detect rising edge crossing threshold
        if (holdoff_counter_ == 0) {
            if (prev_envelope_ < threshold_ && envelope_ >= threshold_) {
                trigger = true;
                holdoff_counter_ = holdoff_samples_;
                trigger_counter_ = trigger_samples_;  // Start trigger pulse
            }
        }
        
        prev_envelope_ = envelope_;
        return trigger;
    }
    
    // Process a block, returns true if any trigger occurred
    bool ProcessBlock(const float* input, size_t size) {
        bool any_trigger = false;
        for (size_t i = 0; i < size; i++) {
            if (Process(input[i])) {
                any_trigger = true;
            }
        }
        return any_trigger;
    }
    
    // Returns true while trigger pulse is active (for gate output)
    bool GetTriggerState() {
        if (trigger_counter_ > 0) {
            trigger_counter_--;
            return true;
        }
        return false;
    }
    
    // Decrement trigger counter by block size, for block-based processing
    void DecrementTrigger(size_t samples) {
        if (trigger_counter_ > samples) {
            trigger_counter_ -= samples;
        } else {
            trigger_counter_ = 0;
        }
    }
    
    bool IsTriggerActive() const { return trigger_counter_ > 0; }
    
    // Set threshold (0.0 - 1.0)
    void SetThreshold(float threshold) {
        threshold_ = std::max(0.01f, std::min(threshold, 1.0f));
    }
    
    // Set holdoff time in milliseconds (20ms - 500ms recommended)
    void SetHoldoff(float holdoff_ms) {
        holdoff_ms = std::max(10.0f, std::min(holdoff_ms, 500.0f));
        holdoff_samples_ = static_cast<uint32_t>(holdoff_ms * 0.001f * sample_rate_);
    }
    
    // Set holdoff from normalized value (20ms - 200ms range)
    void SetHoldoffNormalized(float value) {
        // Linear scale: 20ms at 0, 200ms at 1
        float holdoff_ms = 20.0f + value * 180.0f;
        SetHoldoff(holdoff_ms);
    }
    
    void Reset() {
        envelope_ = 0.0f;
        prev_envelope_ = 0.0f;
        holdoff_counter_ = 0;
        trigger_counter_ = 0;
    }
    
private:
    float sample_rate_ = 48000.0f;
    float envelope_ = 0.0f;
    float prev_envelope_ = 0.0f;
    float threshold_ = 0.3f;
    uint32_t holdoff_counter_ = 0;
    uint32_t holdoff_samples_ = 2400;  // 50ms at 48kHz
    uint32_t trigger_counter_ = 0;
    static constexpr uint32_t trigger_samples_ = 480;  // 10ms trigger pulse at 48kHz
};

//=============================================================================
// AudioEnvProcessor
// Combined envelope follower + transient detector with modulation routing
//=============================================================================
class AudioEnvProcessor {
public:
    AudioEnvProcessor() = default;
    
    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        env_follower_.Init(sample_rate);
        transient_detector_.Init(sample_rate);
        mode_ = AudioEnvMode::OFF;
        input_gain_ = 1.0f;
        timbre_amount_ = 0.0f;
        morph_amount_ = 0.0f;
        current_env_ = 0.0f;
        trigger_detected_ = false;
    }
    
    void SetMode(AudioEnvMode mode) { mode_ = mode; }
    AudioEnvMode GetMode() const { return mode_; }
    
    // Input gain (1.0 - 10.0 for line level signals)
    void SetGain(float gain) { input_gain_ = std::max(1.0f, std::min(gain, 10.0f)); }
    void SetGainNormalized(float value) { input_gain_ = 1.0f + value * 9.0f; }  // 0-1 -> 1x-10x
    
    // Modulation amounts: -1.0 to +1.0
    void SetTimbreAmount(float amount) { 
        timbre_amount_ = std::max(-1.0f, std::min(amount, 1.0f)); 
    }
    void SetMorphAmount(float amount) { 
        morph_amount_ = std::max(-1.0f, std::min(amount, 1.0f)); 
    }
    
    // Envelope follower params (normalized 0-1)
    void SetAttack(float attack) { env_follower_.SetAttackNormalized(attack); }
    void SetRelease(float release) { env_follower_.SetReleaseNormalized(release); }
    
    // Transient detector params
    void SetThreshold(float threshold) { transient_detector_.SetThreshold(threshold); }
    void SetHoldoff(float holdoff) { transient_detector_.SetHoldoffNormalized(holdoff); }
    
    // Process a block of audio, updates internal state
    void ProcessBlock(const float* input, size_t size) {
        if (mode_ == AudioEnvMode::OFF) {
            current_env_ = 0.0f;
            trigger_detected_ = false;
            return;
        }
        
        // Apply input gain for line-level signals
        if (mode_ == AudioEnvMode::ENV) {
            // Process each sample with gain applied
            for (size_t i = 0; i < size; i++) {
                float gained = input[i] * input_gain_;
                env_follower_.Process(gained);
            }
            // Scale envelope to useful modulation range
            // Raw envelope tracks amplitude (typically 0-0.5 for hot signals)
            // Scale by ~4x and clamp to get more usable 0-1 range
            float raw_env = env_follower_.GetEnvelope();
            current_env_ = std::min(1.0f, raw_env * 4.0f);
            trigger_detected_ = false;
        } else if (mode_ == AudioEnvMode::TRIG) {
            // Process each sample with gain applied
            trigger_detected_ = false;
            for (size_t i = 0; i < size; i++) {
                float gained = input[i] * input_gain_;
                if (transient_detector_.Process(gained)) {
                    trigger_detected_ = true;
                }
            }
            // Also track envelope for display/debugging
            current_env_ = transient_detector_.IsTriggerActive() ? 1.0f : 0.0f;
        }
    }
    
    // Get modulation values to add to timbre/morph
    float GetTimbreModulation() const {
        if (mode_ == AudioEnvMode::OFF) return 0.0f;
        return current_env_ * timbre_amount_;
    }
    
    float GetMorphModulation() const {
        if (mode_ == AudioEnvMode::OFF) return 0.0f;
        return current_env_ * morph_amount_;
    }
    
    // Get raw envelope value (0-1)
    float GetEnvelope() const { return current_env_; }
    
    // Returns true if a trigger was detected this block
    bool GetTrigger() const { return trigger_detected_; }
    
    // Returns true while trigger pulse is active (for gate output routing)
    bool IsTriggerActive() const { 
        return mode_ == AudioEnvMode::TRIG && transient_detector_.IsTriggerActive(); 
    }
    
    // Decrement trigger counter (call after checking IsTriggerActive)
    void DecrementTrigger(size_t samples) {
        transient_detector_.DecrementTrigger(samples);
    }
    
private:
    float sample_rate_ = 48000.0f;
    EnvelopeFollower env_follower_;
    TransientDetector transient_detector_;
    
    AudioEnvMode mode_ = AudioEnvMode::OFF;
    float input_gain_ = 1.0f;
    float timbre_amount_ = 0.0f;
    float morph_amount_ = 0.0f;
    float current_env_ = 0.0f;
    bool trigger_detected_ = false;
};

} // namespace mutables_plaits
