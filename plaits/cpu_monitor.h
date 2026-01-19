#pragma once

#include "daisy_patch.h"
#include "util/CpuLoadMeter.h"

// CPU Usage Monitor for Plaits Port
//
// Features:
// - Wraps daisy::CpuLoadMeter for audio callback timing
// - Periodic logging (controlled by DEBUG_LOGGING flag)
// - UI alert threshold (default 95%)
// - Conditional compilation for release builds
//
// Usage:
//   In main.cpp:
//     CpuMonitor cpu_monitor;
//     cpu_monitor.Init(sample_rate, block_size);
//     
//   In AudioCallback:
//     cpu_monitor.OnBlockStart();
//     // ... audio processing ...
//     cpu_monitor.OnBlockEnd();
//     
//   In main loop:
//     cpu_monitor.Update();  // Logs periodically if DEBUG_LOGGING
//     if (cpu_monitor.IsOverloaded()) { show alert }
//
// Build flags:
//   DEBUG_LOGGING=1  - Enable CPU logging (default in debug builds)
//   RELEASE=1        - Disable all debug logging

namespace mutables {

// Default alert threshold (95% CPU usage)
constexpr float kCpuAlertThreshold = 0.95f;

// Logging interval in milliseconds
constexpr uint32_t kCpuLogIntervalMs = 2000;

// Smoothing filter cutoff for average calculation (Hz)
constexpr float kCpuSmoothingHz = 0.5f;

class CpuMonitor {
public:
    CpuMonitor() 
        : alert_threshold_(kCpuAlertThreshold)
        , overloaded_(false)
        , last_log_time_(0)
        , sample_rate_(0)
        , block_size_(0)
        , initialized_(false) {}

    /**
     * Initialize the CPU monitor
     * @param sample_rate Audio sample rate in Hz
     * @param block_size Audio block size in samples
     */
    void Init(float sample_rate, int block_size) {
        sample_rate_ = sample_rate;
        block_size_ = block_size;
        meter_.Init(sample_rate, block_size, kCpuSmoothingHz);
        last_log_time_ = 0;
        initialized_ = true;
    }

    /**
     * Call at the start of AudioCallback
     * Marks the beginning of audio processing
     */
    inline void OnBlockStart() {
        if (initialized_) {
            meter_.OnBlockStart();
        }
    }

    /**
     * Call at the end of AudioCallback
     * Measures processing time and updates statistics
     */
    inline void OnBlockEnd() {
        if (initialized_) {
            meter_.OnBlockEnd();
            // Check for overload condition
            float current = meter_.GetAvgCpuLoad();
            overloaded_ = (current >= alert_threshold_);
        }
    }

    /**
     * Get current average CPU load (0.0 to 1.0)
     */
    float GetAvgLoad() const { 
        return initialized_ ? meter_.GetAvgCpuLoad() : 0.0f; 
    }

    /**
     * Get maximum CPU load since last reset (0.0 to 1.0)
     */
    float GetMaxLoad() const { 
        return initialized_ ? meter_.GetMaxCpuLoad() : 0.0f; 
    }

    /**
     * Get minimum CPU load since last reset (0.0 to 1.0)
     */
    float GetMinLoad() const { 
        return initialized_ ? meter_.GetMinCpuLoad() : 0.0f; 
    }

    /**
     * Check if CPU usage is above alert threshold
     * @return true if current average load >= threshold
     */
    bool IsOverloaded() const { 
        return overloaded_; 
    }

    /**
     * Set alert threshold (0.0 to 1.0)
     * Default is 0.95 (95%)
     */
    void SetAlertThreshold(float threshold) {
        alert_threshold_ = std::clamp(threshold, 0.0f, 1.0f);
    }

    /**
     * Get current alert threshold
     */
    float GetAlertThreshold() const {
        return alert_threshold_;
    }

    /**
     * Reset min/max/avg statistics
     */
    void Reset() {
        if (initialized_) {
            meter_.Reset();
        }
    }

    /**
     * Update and optionally log CPU usage
     * Call this in the main loop (not in AudioCallback)
     * @param current_time_ms Current time in milliseconds
     * @param logger Pointer to logger (can be nullptr to skip logging)
     * @return true if logging occurred this call
     */
    template<typename LoggerType>
    bool Update(uint32_t current_time_ms, LoggerType* logger) {
        if (!initialized_) return false;

#ifndef RELEASE
#ifdef DEBUG_LOGGING
        // Only log periodically
        if (logger && (current_time_ms - last_log_time_ >= kCpuLogIntervalMs)) {
            last_log_time_ = current_time_ms;
            
            float avg = GetAvgLoad() * 100.0f;
            float max = GetMaxLoad() * 100.0f;
            float min = GetMinLoad() * 100.0f;
            
            logger->PrintLine("CPU: %.1f%% avg, %.1f%% max, %.1f%% min%s",
                avg, max, min,
                overloaded_ ? " [OVERLOAD]" : "");
            
            return true;
        }
#endif // DEBUG_LOGGING
#endif // RELEASE
        (void)current_time_ms;
        (void)logger;
        return false;
    }

    /**
     * Get formatted string for display (no memory allocation)
     * @param buf Buffer to write to (should be at least 12 chars)
     * @return Pointer to buf
     */
    char* GetDisplayString(char* buf) const {
        if (!initialized_) {
            buf[0] = '-';
            buf[1] = '-';
            buf[2] = '%';
            buf[3] = '\0';
            return buf;
        }
        
        int pct = static_cast<int>(GetAvgLoad() * 100.0f + 0.5f);
        if (pct > 99) pct = 99;
        if (pct < 0) pct = 0;
        
        // Format: "XX%"
        buf[0] = (pct / 10) + '0';
        buf[1] = (pct % 10) + '0';
        buf[2] = '%';
        buf[3] = '\0';
        
        return buf;
    }

private:
    daisy::CpuLoadMeter meter_;
    float alert_threshold_;
    bool overloaded_;
    uint32_t last_log_time_;
    float sample_rate_;
    int block_size_;
    bool initialized_;
};

} // namespace mutables
