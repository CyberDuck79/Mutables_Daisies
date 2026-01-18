// format_utils.h - Parameter value formatting utilities
// Part of Mutables Daisies UI library
//
// Extracted from plaits_port.cpp to deduplicate formatting code.
// These formatters handle common display patterns for audio parameters.

#pragma once

#include <cstdio>
#include <cstdint>
#include <cmath>
#include "../constants.h"

namespace mutables_ui {
namespace format {

// ============================================================================
// Time Formatting (Attack/Release/Holdoff)
// ============================================================================

// Format logarithmic time value to ms/s string
// Example: FormatLogTime(buffer, 16, value, 0.5f, 400.0f) 
//          produces "0.5ms" to "200ms" or "0.2s" to "2.0s"
inline void FormatLogTime(char* buffer, size_t buffer_size, 
                         float value, float min_ms, float scale) {
    float ms = min_ms * std::pow(scale, value);
    
    if (ms < 10.0f) {
        // Show one decimal place: X.Xms
        int ms_int = static_cast<int>(ms * 10.0f + 0.5f);
        std::snprintf(buffer, buffer_size, "%d.%dms", ms_int / 10, ms_int % 10);
    } else if (ms < 1000.0f) {
        // Show integer ms
        std::snprintf(buffer, buffer_size, "%dms", static_cast<int>(ms + 0.5f));
    } else {
        // Show as seconds with one decimal: X.Xs
        int s_int = static_cast<int>(ms / 100.0f + 0.5f);  // tenths of seconds
        std::snprintf(buffer, buffer_size, "%d.%ds", s_int / 10, s_int % 10);
    }
}

// Common time ranges used in audio
// Attack time: 0.5ms to 200ms (min=0.5, scale=400)
inline void FormatAttackTime(char* buffer, size_t buffer_size, float value) {
    FormatLogTime(buffer, buffer_size, value, 0.5f, 400.0f);
}

// Release time: 5ms to 2000ms (min=5, scale=400)
inline void FormatReleaseTime(char* buffer, size_t buffer_size, float value) {
    FormatLogTime(buffer, buffer_size, value, 5.0f, 400.0f);
}

// ============================================================================
// Gain/Level Formatting
// ============================================================================

// Format gain as dB (input range 0-1 maps to 1x-10x gain, 0dB to +20dB)
inline void FormatGainDB(char* buffer, size_t buffer_size, float value) {
    float gain = 1.0f + value * 9.0f;  // 1x to 10x
    // dB = 20 * log10(gain), using log10(x) = ln(x) / ln(10)
    float db = 20.0f * std::log(gain) / mutables::kLn10;
    int db_int = static_cast<int>(db + 0.5f);
    std::snprintf(buffer, buffer_size, "+%ddB", db_int);
}

// Format generic dB value (already in dB, can be negative)
inline void FormatDB(char* buffer, size_t buffer_size, float db) {
    int db_int = static_cast<int>(db + (db >= 0 ? 0.5f : -0.5f));
    if (db_int >= 0) {
        std::snprintf(buffer, buffer_size, "+%ddB", db_int);
    } else {
        std::snprintf(buffer, buffer_size, "%ddB", db_int);
    }
}

// ============================================================================
// Percentage Formatting
// ============================================================================

// Format value as percentage (0.0-1.0 -> 0%-100%)
inline void FormatPercent(char* buffer, size_t buffer_size, float value) {
    std::snprintf(buffer, buffer_size, "%d%%", static_cast<int>(value * 100.0f + 0.5f));
}

// Format bipolar value as percentage (-1.0 to 1.0 -> -100% to +100%)
inline void FormatBipolarPercent(char* buffer, size_t buffer_size, float value) {
    int percent = static_cast<int>(value * 100.0f);
    if (percent >= 0) {
        std::snprintf(buffer, buffer_size, "+%d%%", percent);
    } else {
        std::snprintf(buffer, buffer_size, "%d%%", percent);
    }
}

// ============================================================================
// Frequency Formatting
// ============================================================================

// Format logarithmic frequency value to Hz string
// Example: FormatLogFrequency(buffer, 16, value, 0.1f, 200.0f)
//          produces "0.10Hz" to "20Hz"
inline void FormatLogFrequency(char* buffer, size_t buffer_size,
                              float value, float min_hz, float scale) {
    float hz = min_hz * std::pow(scale, value);
    int hz_int = static_cast<int>(hz * 100.0f + 0.5f);  // hundredths of Hz
    
    if (hz < 1.0f) {
        // Show two decimals: 0.XXHz
        std::snprintf(buffer, buffer_size, "0.%02dHz", hz_int);
    } else if (hz < 10.0f) {
        // Show one decimal: X.XHz
        int hz_tenths = static_cast<int>(hz * 10.0f + 0.5f);
        std::snprintf(buffer, buffer_size, "%d.%dHz", hz_tenths / 10, hz_tenths % 10);
    } else if (hz < 1000.0f) {
        // Show integer Hz
        std::snprintf(buffer, buffer_size, "%dHz", static_cast<int>(hz + 0.5f));
    } else {
        // Show as kHz with one decimal
        int khz_tenths = static_cast<int>(hz / 100.0f + 0.5f);
        std::snprintf(buffer, buffer_size, "%d.%dkHz", khz_tenths / 10, khz_tenths % 10);
    }
}

// LFO rate: 0.1Hz to 20Hz (min=0.1, scale=200)
inline void FormatLFORate(char* buffer, size_t buffer_size, float value) {
    FormatLogFrequency(buffer, buffer_size, value, 0.1f, 200.0f);
}

// Filter cutoff: 20Hz to 20kHz (min=20, scale=1000)
inline void FormatCutoffFrequency(char* buffer, size_t buffer_size, float value) {
    FormatLogFrequency(buffer, buffer_size, value, 20.0f, 1000.0f);
}

// ============================================================================
// Miscellaneous Formatting
// ============================================================================

// Format value as degrees (0.0-1.0 -> 0°-360°)
inline void FormatDegrees(char* buffer, size_t buffer_size, float value) {
    std::snprintf(buffer, buffer_size, "%ddeg", static_cast<int>(value * 360.0f + 0.5f));
}

// Format multiplier (0.0-1.0 -> 0.0x-2.0x)
inline void FormatMultiplier(char* buffer, size_t buffer_size, float value, float max_mult = 2.0f) {
    float mult = value * max_mult;
    int mult_int = static_cast<int>(mult * 10.0f + 0.5f);
    std::snprintf(buffer, buffer_size, "%d.%dx", mult_int / 10, mult_int % 10);
}

// Format generic decimal (0.0-1.0 -> 0.00-1.00)
inline void FormatDecimal(char* buffer, size_t buffer_size, float value) {
    int val_int = static_cast<int>(value * 100.0f);
    int whole = val_int / 100;
    int frac = val_int % 100;
    std::snprintf(buffer, buffer_size, "%d.%02d", whole, frac);
}

// Format integer with suffix
inline void FormatIntWithSuffix(char* buffer, size_t buffer_size, 
                                int value, const char* suffix) {
    std::snprintf(buffer, buffer_size, "%d%s", value, suffix);
}

}  // namespace format
}  // namespace mutables_ui
