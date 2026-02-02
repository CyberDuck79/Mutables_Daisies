#pragma once

#include "daisy_patch.h"
#include "midi_processor.h"
#include <functional>

namespace mutables_ui {

/**
 * MIDIDispatcher - Standard MIDI event handling with callbacks
 * 
 * Encapsulates common MIDI processing patterns:
 * - MIDI Thru (forwarding all events)
 * - Channel filtering via MIDIProcessor
 * - Clock handling
 * - Note On/Off dispatching
 * - CC handling
 * 
 * Usage:
 *   MIDIDispatcher midi_dispatcher;
 *   midi_dispatcher.SetNoteOnCallback([&](uint8_t note, uint8_t vel) {
 *       module.NoteOn(note, vel);
 *   });
 *   // In main loop:
 *   midi_dispatcher.Process(hw, midi_processor);
 */
class MIDIDispatcher {
public:
    // Callback types
    using NoteCallback = std::function<void(uint8_t note, uint8_t velocity)>;
    using ClockCallback = std::function<void()>;
    using CCCallback = std::function<void(uint8_t cc_number, uint8_t value)>;
    using ChannelCallback = std::function<int()>;  // Returns current MIDI channel
    
    MIDIDispatcher() 
        : midi_thru_enabled_(true)
        , trigger_sample_hold_(false) {}
    
    //=========================================================================
    // Configuration
    //=========================================================================
    
    void SetNoteOnCallback(NoteCallback cb) { note_on_callback_ = cb; }
    void SetNoteOffCallback(NoteCallback cb) { note_off_callback_ = cb; }
    void SetClockCallback(ClockCallback cb) { clock_callback_ = cb; }
    void SetCCCallback(CCCallback cb) { cc_callback_ = cb; }
    
    /**
     * Set callback to get current MIDI channel from module.
     * Called before processing each event batch to update channel filter.
     */
    void SetChannelCallback(ChannelCallback cb) { channel_callback_ = cb; }
    
    void SetMidiThruEnabled(bool enabled) { midi_thru_enabled_ = enabled; }
    
    /**
     * Check and clear the sample-hold trigger flag.
     * This is set on NoteOn events to trigger S&H for CV-mapped parameters.
     * @return true if a NoteOn occurred since last check
     */
    bool CheckAndClearSampleHoldTrigger() {
        bool triggered = trigger_sample_hold_;
        trigger_sample_hold_ = false;
        return triggered;
    }
    
    //=========================================================================
    // Processing
    //=========================================================================
    
    /**
     * Process all pending MIDI events.
     * Call this in your main loop after hw.midi.Listen().
     * 
     * @param hw Reference to DaisyPatch hardware
     * @param midi_processor Reference to MIDIProcessor for channel filtering and CC storage
     */
    void Process(daisy::DaisyPatch& hw, MIDIProcessor& midi_processor) {
        using namespace daisy;
        
        // Update channel from module if callback is set
        if (channel_callback_) {
            midi_processor.SetChannel(channel_callback_());
        }
        
        while (hw.midi.HasEvents()) {
            MidiEvent event = hw.midi.PopEvent();
            
            // Handle MIDI clock (system realtime, not channel-dependent)
            if (event.type == SystemRealTime) {
                if (event.srt_type == TimingClock && clock_callback_) {
                    clock_callback_();
                }
                continue;  // System realtime messages don't need further processing
            }
            
            // MIDI Thru: Forward channel messages regardless of our channel
            if (midi_thru_enabled_) {
                ForwardMidiThru(hw, event);
            }
            
            // Channel filter
            if (!midi_processor.ShouldProcess(event.channel)) {
                continue;
            }
            
            // Dispatch by event type
            switch (event.type) {
                case NoteOn: {
                    NoteOnEvent note = event.AsNoteOn();
                    if (note.velocity > 0) {
                        trigger_sample_hold_ = true;
                        if (note_on_callback_) {
                            note_on_callback_(note.note, note.velocity);
                        }
                    } else {
                        // Note On with velocity 0 = Note Off
                        if (note_off_callback_) {
                            note_off_callback_(note.note, 0);
                        }
                    }
                    break;
                }
                
                case NoteOff: {
                    NoteOffEvent note = event.AsNoteOff();
                    if (note_off_callback_) {
                        note_off_callback_(note.note, note.velocity);
                    }
                    break;
                }
                
                case ControlChange: {
                    ControlChangeEvent cc = event.AsControlChange();
                    midi_processor.SetCC(cc.control_number, cc.value);
                    if (cc_callback_) {
                        cc_callback_(cc.control_number, cc.value);
                    }
                    break;
                }
                
                default:
                    break;
            }
        }
    }
    
private:
    void ForwardMidiThru(daisy::DaisyPatch& hw, const daisy::MidiEvent& event) {
        using namespace daisy;
        
        if (event.type != NoteOn && event.type != NoteOff && event.type != ControlChange) {
            return;
        }
        
        uint8_t out_bytes[3];
        uint8_t status_type = 0;
        
        if (event.type == NoteOn) status_type = 0x90;
        else if (event.type == NoteOff) status_type = 0x80;
        else if (event.type == ControlChange) status_type = 0xB0;
        
        size_t size = MIDIProcessor::BuildThruMessage(
            status_type, event.channel, event.data[0], event.data[1], out_bytes);
        
        if (size > 0) {
            hw.midi.SendMessage(out_bytes, size);
        }
    }
    
    NoteCallback note_on_callback_;
    NoteCallback note_off_callback_;
    ClockCallback clock_callback_;
    CCCallback cc_callback_;
    ChannelCallback channel_callback_;
    
    bool midi_thru_enabled_;
    bool trigger_sample_hold_;
};

} // namespace mutables_ui
