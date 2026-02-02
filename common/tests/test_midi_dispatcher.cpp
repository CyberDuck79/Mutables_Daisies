// Test for MIDIDispatcher
// Note: We test the callback registration and logic flow.
// Hardware-dependent parts (actual MIDI processing) would require mocking DaisyPatch.

#include "test_framework.h"
#include <functional>

// ============================================================================
// Mock types for testing MIDIDispatcher logic without hardware
// ============================================================================

// We can't include the full midi_dispatcher.h without DaisyPatch headers,
// so we test the callback interface patterns here.

namespace test_midi_dispatcher {

// Simplified callback types matching MIDIDispatcher
using NoteCallback = std::function<void(uint8_t note, uint8_t velocity)>;
using ClockCallback = std::function<void()>;
using CCCallback = std::function<void(uint8_t cc_number, uint8_t value)>;

// Simplified dispatcher for testing callback logic
class TestMIDIDispatcher {
public:
    TestMIDIDispatcher() : trigger_sample_hold_(false) {}
    
    void SetNoteOnCallback(NoteCallback cb) { note_on_callback_ = cb; }
    void SetNoteOffCallback(NoteCallback cb) { note_off_callback_ = cb; }
    void SetClockCallback(ClockCallback cb) { clock_callback_ = cb; }
    void SetCCCallback(CCCallback cb) { cc_callback_ = cb; }
    
    bool CheckAndClearSampleHoldTrigger() {
        bool triggered = trigger_sample_hold_;
        trigger_sample_hold_ = false;
        return triggered;
    }
    
    // Simulate receiving events for testing
    void SimulateNoteOn(uint8_t note, uint8_t velocity) {
        trigger_sample_hold_ = true;
        if (note_on_callback_) {
            note_on_callback_(note, velocity);
        }
    }
    
    void SimulateNoteOff(uint8_t note, uint8_t velocity) {
        if (note_off_callback_) {
            note_off_callback_(note, velocity);
        }
    }
    
    void SimulateClock() {
        if (clock_callback_) {
            clock_callback_();
        }
    }
    
    void SimulateCC(uint8_t cc, uint8_t value) {
        if (cc_callback_) {
            cc_callback_(cc, value);
        }
    }
    
private:
    NoteCallback note_on_callback_;
    NoteCallback note_off_callback_;
    ClockCallback clock_callback_;
    CCCallback cc_callback_;
    bool trigger_sample_hold_;
};

} // namespace test_midi_dispatcher

using namespace test_midi_dispatcher;

// ============================================================================
// MIDIDispatcher Callback Tests
// ============================================================================

TEST(MIDIDispatcher, NoteOnCallbackInvoked) {
    TestMIDIDispatcher dispatcher;
    
    uint8_t received_note = 0;
    uint8_t received_velocity = 0;
    
    dispatcher.SetNoteOnCallback([&](uint8_t note, uint8_t vel) {
        received_note = note;
        received_velocity = vel;
    });
    
    dispatcher.SimulateNoteOn(60, 100);
    
    EXPECT_EQ(received_note, 60);
    EXPECT_EQ(received_velocity, 100);
}

TEST(MIDIDispatcher, NoteOffCallbackInvoked) {
    TestMIDIDispatcher dispatcher;
    
    uint8_t received_note = 0;
    uint8_t received_velocity = 0;
    
    dispatcher.SetNoteOffCallback([&](uint8_t note, uint8_t vel) {
        received_note = note;
        received_velocity = vel;
    });
    
    dispatcher.SimulateNoteOff(64, 0);
    
    EXPECT_EQ(received_note, 64);
    EXPECT_EQ(received_velocity, 0);
}

TEST(MIDIDispatcher, ClockCallbackInvoked) {
    TestMIDIDispatcher dispatcher;
    
    int clock_count = 0;
    
    dispatcher.SetClockCallback([&]() {
        clock_count++;
    });
    
    dispatcher.SimulateClock();
    dispatcher.SimulateClock();
    dispatcher.SimulateClock();
    
    EXPECT_EQ(clock_count, 3);
}

TEST(MIDIDispatcher, CCCallbackInvoked) {
    TestMIDIDispatcher dispatcher;
    
    uint8_t received_cc = 0;
    uint8_t received_value = 0;
    
    dispatcher.SetCCCallback([&](uint8_t cc, uint8_t val) {
        received_cc = cc;
        received_value = val;
    });
    
    dispatcher.SimulateCC(74, 127);
    
    EXPECT_EQ(received_cc, 74);
    EXPECT_EQ(received_value, 127);
}

TEST(MIDIDispatcher, SampleHoldTriggerOnNoteOn) {
    TestMIDIDispatcher dispatcher;
    
    // Initially false
    EXPECT_FALSE(dispatcher.CheckAndClearSampleHoldTrigger());
    
    // NoteOn should trigger S&H
    dispatcher.SimulateNoteOn(60, 100);
    EXPECT_TRUE(dispatcher.CheckAndClearSampleHoldTrigger());
    
    // Should be cleared after check
    EXPECT_FALSE(dispatcher.CheckAndClearSampleHoldTrigger());
}

TEST(MIDIDispatcher, SampleHoldNotTriggeredByNoteOff) {
    TestMIDIDispatcher dispatcher;
    
    dispatcher.SimulateNoteOff(60, 0);
    EXPECT_FALSE(dispatcher.CheckAndClearSampleHoldTrigger());
}

TEST(MIDIDispatcher, MultipleNoteOnsOnlyOneTrigger) {
    TestMIDIDispatcher dispatcher;
    
    dispatcher.SimulateNoteOn(60, 100);
    dispatcher.SimulateNoteOn(64, 100);
    dispatcher.SimulateNoteOn(67, 100);
    
    // First check clears
    EXPECT_TRUE(dispatcher.CheckAndClearSampleHoldTrigger());
    
    // Second check is false (cleared)
    EXPECT_FALSE(dispatcher.CheckAndClearSampleHoldTrigger());
}

TEST(MIDIDispatcher, NoCallbackNoError) {
    TestMIDIDispatcher dispatcher;
    
    // Should not crash without callbacks set
    dispatcher.SimulateNoteOn(60, 100);
    dispatcher.SimulateNoteOff(60, 0);
    dispatcher.SimulateClock();
    dispatcher.SimulateCC(7, 64);
    
    EXPECT_TRUE(true); // If we get here, no crash
}

TEST(MIDIDispatcher, CallbackCanModifyExternalState) {
    TestMIDIDispatcher dispatcher;
    
    struct ModuleState {
        bool gate = false;
        uint8_t note = 0;
        float velocity = 0.0f;
    } state;
    
    dispatcher.SetNoteOnCallback([&](uint8_t note, uint8_t vel) {
        state.gate = true;
        state.note = note;
        state.velocity = vel / 127.0f;
    });
    
    dispatcher.SetNoteOffCallback([&](uint8_t note, uint8_t) {
        if (state.note == note) {
            state.gate = false;
        }
    });
    
    EXPECT_FALSE(state.gate);
    
    dispatcher.SimulateNoteOn(60, 100);
    EXPECT_TRUE(state.gate);
    EXPECT_EQ(state.note, 60);
    EXPECT_FLOAT_EQ(state.velocity, 100.0f / 127.0f);
    
    dispatcher.SimulateNoteOff(60, 0);
    EXPECT_FALSE(state.gate);
}
