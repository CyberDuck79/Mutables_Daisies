/**
 * Knob Catcher - Handles "catch-up" behavior for knobs
 * 
 * Based on Plaits pot_controller.h by Emilie Gillet
 * 
 * When switching parameter pages, the physical knob position may not match
 * the stored parameter value. This class handles:
 * 1. Not changing parameters until the knob is moved
 * 2. Optionally: smoothly transitioning using "skew ratio" algorithm (for Play mode)
 * 3. Or: immediately tracking once moved (for Parameters mode)
 */

#ifndef KNOB_CATCHER_H_
#define KNOB_CATCHER_H_

#include "stmlib/dsp/dsp.h"

namespace plaits_daisy {

enum KnobState {
    KNOB_TRACKING,    // Normal: parameter follows knob
    KNOB_WAITING,     // Waiting for user to move knob
    KNOB_CATCHING_UP  // Smoothly transitioning to match knob position
};

class KnobCatcher {
public:
    KnobCatcher() {}
    ~KnobCatcher() {}
    
    /**
     * Initialize with LP filter coefficient
     * @param lp_coeff Low-pass filter coefficient (smaller = smoother)
     * @param use_catchup If true, use skew/catch-up. If false, go directly to tracking.
     */
    void Init(float lp_coeff = 0.01f, bool use_catchup = true) {
        lp_coefficient_ = lp_coeff;
        use_catchup_ = use_catchup;
        Reset();
    }
    
    /**
     * Reset to tracking state (call on startup)
     */
    void Reset() {
        state_ = KNOB_TRACKING;
        filtered_value_ = 0.0f;
        stored_value_ = 0.0f;
        previous_value_ = 0.0f;
    }
    
    /**
     * Call when switching to a new parameter page
     * Stores the current parameter value and enters waiting state
     * @param current_param_value The current value of the parameter (0-1)
     */
    void OnPageChange(float current_param_value) {
        stored_value_ = current_param_value;
        state_ = KNOB_WAITING;
    }
    
    /**
     * Force back to tracking state (e.g., after long press reset)
     */
    void ForceTracking() {
        state_ = KNOB_TRACKING;
    }
    
    /**
     * Process a new ADC reading and return the parameter value
     * @param adc_value Raw ADC value (0-1)
     * @return The parameter value to use (0-1)
     */
    float Process(float adc_value) {
        // Low-pass filter the input
        ONE_POLE(filtered_value_, adc_value, lp_coefficient_);
        
        float output_value;
        
        switch (state_) {
            case KNOB_TRACKING:
                // Normal operation: output follows filtered input
                output_value = filtered_value_;
                previous_value_ = filtered_value_;
                break;
                
            case KNOB_WAITING:
                // Wait for knob to move before doing anything
                if (fabsf(filtered_value_ - previous_value_) > kMovementThreshold) {
                    // User moved the knob
                    if (use_catchup_) {
                        // Use catch-up mode (for Play mode)
                        state_ = KNOB_CATCHING_UP;
                    } else {
                        // Immediately start tracking (for Parameters mode)
                        state_ = KNOB_TRACKING;
                        output_value = filtered_value_;
                        previous_value_ = filtered_value_;
                        return output_value;
                    }
                    previous_value_ = filtered_value_;
                }
                // Output the stored value (don't change parameter)
                output_value = stored_value_;
                break;
                
            case KNOB_CATCHING_UP:
                {
                    // Check if knob is still moving
                    if (fabsf(filtered_value_ - previous_value_) > kMinDelta) {
                        float delta = filtered_value_ - previous_value_;
                        
                        // Calculate skew ratio for smooth catch-up
                        // This makes the parameter move faster/slower depending on
                        // how far it is from the knob position
                        float skew_ratio;
                        if (delta > 0.0f) {
                            skew_ratio = (1.001f - stored_value_) / (1.001f - previous_value_);
                        } else {
                            skew_ratio = (0.001f + stored_value_) / (0.001f + previous_value_);
                        }
                        
                        // Clamp skew ratio to reasonable bounds
                        if (skew_ratio < 0.1f) skew_ratio = 0.1f;
                        if (skew_ratio > 10.0f) skew_ratio = 10.0f;
                        
                        // Apply scaled delta to stored value
                        stored_value_ += skew_ratio * delta;
                        
                        // Clamp to valid range
                        if (stored_value_ < 0.0f) stored_value_ = 0.0f;
                        if (stored_value_ > 1.0f) stored_value_ = 1.0f;
                        
                        // Check if we've caught up (knob and parameter match)
                        if (fabsf(stored_value_ - filtered_value_) < kCatchUpThreshold) {
                            state_ = KNOB_TRACKING;
                        }
                        
                        previous_value_ = filtered_value_;
                    }
                    output_value = stored_value_;
                }
                break;
                
            default:
                output_value = filtered_value_;
                break;
        }
        
        return output_value;
    }
    
    /**
     * Check if this knob is currently catching up
     */
    bool IsCatchingUp() const {
        return state_ == KNOB_CATCHING_UP;
    }
    
    /**
     * Check if this knob is waiting for movement
     */
    bool IsWaiting() const {
        return state_ == KNOB_WAITING;
    }
    
    /**
     * Get current state
     */
    KnobState GetState() const {
        return state_;
    }

private:
    // Thresholds (same as original Plaits)
    static constexpr float kMovementThreshold = 0.03f;  // Movement needed to exit waiting
    static constexpr float kMinDelta = 0.005f;          // Minimum delta to process
    static constexpr float kCatchUpThreshold = 0.005f;  // When to consider "caught up"
    
    KnobState state_;
    float lp_coefficient_;
    bool use_catchup_;
    float filtered_value_;
    float stored_value_;
    float previous_value_;
};

/**
 * Manager for multiple knobs with catch-up behavior
 */
template<int N>
class KnobCatcherBank {
public:
    KnobCatcherBank() {}
    
    /**
     * @param lp_coeff LP filter coefficient
     * @param use_catchup If true, use skew catch-up. If false, direct tracking after move.
     */
    void Init(float lp_coeff = 0.01f, bool use_catchup = true) {
        for (int i = 0; i < N; i++) {
            catchers_[i].Init(lp_coeff, use_catchup);
        }
    }
    
    /**
     * Call when page changes - puts all knobs into waiting state
     * @param current_values Array of current parameter values for the new page
     */
    void OnPageChange(const float* current_values) {
        for (int i = 0; i < N; i++) {
            catchers_[i].OnPageChange(current_values[i]);
        }
    }
    
    /**
     * Process a single knob
     */
    float Process(int index, float adc_value) {
        return catchers_[index].Process(adc_value);
    }
    
    /**
     * Force all knobs to tracking state
     */
    void ForceAllTracking() {
        for (int i = 0; i < N; i++) {
            catchers_[i].ForceTracking();
        }
    }
    
    /**
     * Reset all knobs
     */
    void Reset() {
        for (int i = 0; i < N; i++) {
            catchers_[i].Reset();
        }
    }
    
    /**
     * Get state of a specific knob
     */
    KnobState GetState(int index) const {
        return catchers_[index].GetState();
    }
    
    KnobCatcher& operator[](int index) {
        return catchers_[index];
    }
    
    const KnobCatcher& operator[](int index) const {
        return catchers_[index];
    }

private:
    KnobCatcher catchers_[N];
};

}  // namespace plaits_daisy

#endif  // KNOB_CATCHER_H_
