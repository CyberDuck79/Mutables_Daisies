#pragma once

#include "../common/module_base.h"
#include "../common/parameter.h"
#include <array>

// Forward declarations for Plaits classes
namespace plaits {
class Voice;
struct Modulations;
struct Patch;
}

namespace stmlib {
class BufferAllocator;
}

namespace mutables_plaits {

class PlaitsPort : public mutables_ui::ModuleBase {
public:
    PlaitsPort();
    ~PlaitsPort() override;
    
    // ModuleBase interface
    const char* GetName() const override { return "Plaits"; }
    const char* GetShortName() const override { return "plaits"; }
    
    void Init(float sample_rate) override;
    void Process(float** in, float** out, size_t size) override;
    mutables_ui::Parameter* GetParameters() override;
    size_t GetParameterCount() const override;
    
    void ProcessGate(int gate_index, bool state) override;
    float GetCVOutput(int cv_index) override;
    
private:
    // Plaits engine
    plaits::Voice* voice_;
    plaits::Patch* patch_;
    plaits::Modulations* modulations_;
    stmlib::BufferAllocator* allocator_;
    
    // Buffers
    static constexpr size_t kBlockSize = 24;
    static constexpr size_t kBufferSize = 32768;  // Buffer for Plaits engines
    uint8_t buffer_[kBufferSize];
    
    // Parameters
    static constexpr int kNumParams = 9;  // Added Bank parameter
    std::array<mutables_ui::Parameter, kNumParams> params_;
    
    // Bank and engine system
    static const char* bank_names_[];
    static const char* synth_engine_names_[];
    static const char* drum_engine_names_[];
    static const char* new_engine_names_[];
    static constexpr int kNumBanks = 3;
    static constexpr int kNumSynthEngines = 8;
    static constexpr int kNumDrumEngines = 8;
    static constexpr int kNumNewEngines = 8;
    
    int current_bank_;
    
    // State
    bool gate_state_;
    float sample_rate_;
    
    void UpdatePatchFromParams();
    void SetupParameters();
    void UpdateEngineListForBank(int bank);
    int GetActualEngineIndex(int bank, int engine_in_bank);
};

} // namespace mutables_plaits
