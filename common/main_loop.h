#pragma once

#include "daisy_patch.h"
#include "cpu_monitor.h"
#include "display.h"
#include "midi_dispatcher.h"
#include "midi_processor.h"
#include <functional>

namespace mutables_ui {

/**
 * MainLoop - Encapsulates the common main loop pattern for Daisy Patch modules.
 * 
 * Handles:
 * - MIDI listening and processing
 * - Control/encoder updates
 * - CPU monitoring
 * - Display updates at 60fps
 * - Non-blocking message display
 * 
 * Usage:
 *   MainLoop loop;
 *   loop.Init(&hw, &display, &cpu_monitor, &midi_dispatcher, &midi_processor);
 *   loop.SetEncoderCallback([&]() { UpdateEncoder(); });
 *   loop.SetDisplayCallback([&]() { UpdateDisplay(); });
 *   loop.Run();  // Never returns
 */
class MainLoop {
public:
    using VoidCallback = std::function<void()>;
    using LoggerType = void*;  // Opaque logger type
    
    MainLoop() 
        : hw_(nullptr)
        , display_(nullptr)
        , cpu_monitor_(nullptr)
        , midi_dispatcher_(nullptr)
        , midi_processor_(nullptr)
        , logger_(nullptr)
        , display_interval_ms_(16)  // ~60fps
        , loop_delay_ms_(1) {}
    
    /**
     * Initialize the main loop with required components.
     */
    void Init(daisy::DaisyPatch* hw, 
              Display* display,
              CpuMonitor* cpu_monitor,
              MIDIDispatcher* midi_dispatcher,
              MIDIProcessor* midi_processor) {
        hw_ = hw;
        display_ = display;
        cpu_monitor_ = cpu_monitor;
        midi_dispatcher_ = midi_dispatcher;
        midi_processor_ = midi_processor;
    }
    
    /**
     * Set optional logger for CPU monitoring output.
     */
    template<typename Logger>
    void SetLogger(Logger* logger) {
        logger_ = static_cast<void*>(logger);
    }
    
    /**
     * Set callback for encoder processing.
     * Called every iteration of the main loop after ProcessAllControls().
     */
    void SetEncoderCallback(VoidCallback cb) { encoder_callback_ = cb; }
    
    /**
     * Set callback for display rendering.
     * Called at display_interval_ms_ rate (default 16ms = ~60fps).
     * Should render the current UI state to display.
     */
    void SetDisplayCallback(VoidCallback cb) { display_callback_ = cb; }
    
    /**
     * Set callback for additional per-iteration processing.
     * Called every iteration before display update check.
     */
    void SetUpdateCallback(VoidCallback cb) { update_callback_ = cb; }
    
    /**
     * Set display update interval in milliseconds.
     * Default is 16ms (~60fps).
     */
    void SetDisplayInterval(uint32_t ms) { display_interval_ms_ = ms; }
    
    /**
     * Set main loop delay in milliseconds.
     * Default is 1ms. Set to 0 for maximum responsiveness at cost of CPU.
     */
    void SetLoopDelay(uint32_t ms) { loop_delay_ms_ = ms; }
    
    /**
     * Run the main loop. This function never returns.
     * 
     * Before calling Run(), ensure:
     * - hw->StartAdc() has been called
     * - hw->StartAudio(callback) has been called
     * - hw->midi.StartReceive() has been called
     */
    [[noreturn]] void Run() {
        uint32_t last_display_update = 0;
        
        while (true) {
            // MIDI
            hw_->midi.Listen();
            if (midi_dispatcher_ && midi_processor_) {
                midi_dispatcher_->Process(*hw_, *midi_processor_);
            }
            
            // Controls
            hw_->ProcessAllControls();
            if (encoder_callback_) {
                encoder_callback_();
            }
            
            uint32_t now = daisy::System::GetNow();
            
            // CPU monitoring
            if (cpu_monitor_) {
                // Note: CpuMonitor::Update takes a logger, we pass nullptr here
                // unless a proper logger interface is established
                cpu_monitor_->Update(now, nullptr);
            }
            
            // Custom update logic
            if (update_callback_) {
                update_callback_();
            }
            
            // Display update (with message queue support)
            if (now - last_display_update >= display_interval_ms_) {
                last_display_update = now;
                
                // Check for pending messages first
                if (display_ && display_->HasPendingMessage()) {
                    display_->UpdatePendingMessages();
                } else if (display_callback_) {
                    display_callback_();
                }
            }
            
            // Small delay to prevent busy-waiting
            if (loop_delay_ms_ > 0) {
                daisy::System::Delay(loop_delay_ms_);
            }
        }
    }
    
    /**
     * Run a single iteration of the main loop.
     * Useful for testing or custom loop implementations.
     * 
     * @param now Current time in milliseconds
     * @param last_display_update Reference to last display update time (updated by this function)
     */
    void RunOnce(uint32_t now, uint32_t& last_display_update) {
        // MIDI
        hw_->midi.Listen();
        if (midi_dispatcher_ && midi_processor_) {
            midi_dispatcher_->Process(*hw_, *midi_processor_);
        }
        
        // Controls
        hw_->ProcessAllControls();
        if (encoder_callback_) {
            encoder_callback_();
        }
        
        // CPU monitoring
        if (cpu_monitor_) {
            cpu_monitor_->Update(now, nullptr);
        }
        
        // Custom update logic
        if (update_callback_) {
            update_callback_();
        }
        
        // Display update
        if (now - last_display_update >= display_interval_ms_) {
            last_display_update = now;
            
            if (display_ && display_->HasPendingMessage()) {
                display_->UpdatePendingMessages();
            } else if (display_callback_) {
                display_callback_();
            }
        }
    }
    
private:
    daisy::DaisyPatch* hw_;
    Display* display_;
    CpuMonitor* cpu_monitor_;
    MIDIDispatcher* midi_dispatcher_;
    MIDIProcessor* midi_processor_;
    void* logger_;
    
    VoidCallback encoder_callback_;
    VoidCallback display_callback_;
    VoidCallback update_callback_;
    
    uint32_t display_interval_ms_;
    uint32_t loop_delay_ms_;
};

} // namespace mutables_ui
