/**
 * LED Controller for Plaits on Daisy Patch.Init()
 * 
 * The LED on Patch.Init() is connected to CV_OUT_2 (DAC output)
 * so we control brightness directly via analog voltage (0-5V).
 * 
 * Implements:
 * - 8-level gamma-corrected brightness for engine selection (0-7)
 * - Pulse pattern (3 speeds) for bank indication (0-2)
 */

#pragma once

#include <cstdint>

namespace led_controller {

// Number of engines per bank
constexpr int kEnginesPerBank = 8;
constexpr int kNumBanks = 3;
constexpr int kTotalEngines = 24;

// Brightness table as voltage (0-5V)
constexpr float kBrightnessTable[8] = {
    1.4f,   // Engine 0: Dim (but clearly visible)
    1.7f,   // Engine 1: Low
    1.8f,   // Engine 2: Low-medium
    2.1f,   // Engine 3: Medium-low
    2.3f,   // Engine 4: Medium
    2.6f,   // Engine 5: Medium-high
    3.1f,   // Engine 6: Bright
    5.0f    // Engine 7: Full brightness
};

// Pulse pattern timing (in milliseconds) for bank indication
// Bank 0: Fast pulse (250ms period)
// Bank 1: Medium pulse (500ms period)  
// Bank 2: Slow pulse (1000ms period)
constexpr uint32_t kBankPulsePeriodMs[3] = {
    250,    // Bank 0: 4 Hz
    500,    // Bank 1: 2 Hz
    1000    // Bank 2: 1 Hz
};

// Duration of the bank indication pulse sequence
constexpr uint32_t kBankIndicationDurationMs = 2000;

// Software PWM frequency target (~1kHz for smooth dimming)
constexpr uint32_t kPwmPeriodUs = 1000;  // 1ms period = 1kHz

/**
 * LED Controller class
 * Manages LED brightness for engine display and pulse patterns for bank indication
 */
class LedController {
public:
    LedController() 
        : engine_(0)
        , bank_(0)
        , bank_indication_active_(false)
        , bank_indication_start_ms_(0) {}

    /**
     * Set current engine (0-7 within current bank)
     */
    void SetEngine(int engine_in_bank) {
        engine_ = engine_in_bank % kEnginesPerBank;
    }

    /**
     * Set current bank (0-2)
     */
    void SetBank(int bank) {
        bank_ = bank % kNumBanks;
    }

    /**
     * Get the global engine index (0-23)
     */
    int GetGlobalEngine() const {
        return bank_ * kEnginesPerBank + engine_;
    }

    /**
     * Set engine directly from global index (0-23)
     */
    void SetGlobalEngine(int global_engine) {
        global_engine = global_engine % kTotalEngines;
        bank_ = global_engine / kEnginesPerBank;
        engine_ = global_engine % kEnginesPerBank;
    }

    /**
     * Move to next engine (wraps within bank)
     * @return true if wrapped to engine 0
     */
    bool NextEngine() {
        engine_++;
        if (engine_ >= kEnginesPerBank) {
            engine_ = 0;
            return true;  // Wrapped
        }
        return false;
    }

    /**
     * Move to next bank (wraps around)
     * Also triggers bank indication display
     */
    void NextBank() {
        bank_ = (bank_ + 1) % kNumBanks;
        StartBankIndication();
    }

    /**
     * Start the bank indication pulse sequence
     */
    void StartBankIndication() {
        bank_indication_active_ = true;
        bank_indication_start_ms_ = 0;  // Will be set on first update
    }

    /**
     * Update the LED state
     * Call this from main loop
     * 
     * @param current_time_ms Current system time in milliseconds
     * @return LED voltage (0-5V) for CV_OUT_2
     */
    float Update(uint32_t current_time_ms) {
        // Initialize bank indication start time if needed
        if (bank_indication_active_ && bank_indication_start_ms_ == 0) {
            bank_indication_start_ms_ = current_time_ms;
        }

        // Check if bank indication period has ended
        if (bank_indication_active_) {
            if (current_time_ms - bank_indication_start_ms_ >= kBankIndicationDurationMs) {
                bank_indication_active_ = false;
            }
        }

        // Determine target brightness voltage
        float target_voltage;
        
        if (bank_indication_active_) {
            // Bank indication mode: pulsing at bank-specific rate
            uint32_t pulse_period = kBankPulsePeriodMs[bank_];
            uint32_t elapsed = current_time_ms - bank_indication_start_ms_;
            uint32_t phase = elapsed % pulse_period;
            
            // Simple on/off pulse (first half of period = ON at full brightness)
            target_voltage = (phase < pulse_period / 2) ? 5.0f : 0.0f;
        } else {
            // Normal mode: show engine brightness
            target_voltage = kBrightnessTable[engine_];
        }

        return target_voltage;
    }

    // Getters
    int GetEngine() const { return engine_; }
    int GetBank() const { return bank_; }
    bool IsBankIndicationActive() const { return bank_indication_active_; }

private:
    int engine_;                      // Current engine within bank (0-7)
    int bank_;                        // Current bank (0-2)
    bool bank_indication_active_;     // True during bank pulse display
    uint32_t bank_indication_start_ms_;
};

} // namespace led_controller
