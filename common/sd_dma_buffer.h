#pragma once

#include "daisy_core.h"
#include <cstdint>
#include <cstddef>

/**
 * Shared DMA buffer for SD card operations
 * 
 * SD card operations via SDMMC1 use IDMA which REQUIRES the buffer to be in
 * AXI SRAM (0x24000000), NOT D2 SRAM (0x30000000). The IDMA cannot efficiently
 * access D2 SRAM, causing TX_UNDERRUN errors during writes.
 * 
 * By NOT using DMA_BUFFER_MEM_SECTION, this buffer goes to the default .bss
 * section which is placed in AXI SRAM by the linker script.
 * 
 * IMPORTANT: SD operations are NOT thread-safe and should never be concurrent.
 * This buffer can safely be shared because:
 * - Preset saves/loads happen one at a time
 * - User data loads happen one at a time
 * - These operations don't overlap
 * 
 * Size is 8KB to accommodate:
 * - Preset files: ~7KB (header + 128 params * 56 bytes)
 * - User data: 4KB
 */
namespace sd_utils {

// Buffer size - large enough for all SD operations
// PresetManager needs: sizeof(PresetHeader) + MAX_PARAMS * sizeof(ParameterData) ≈ 7200 bytes
// UserDataManager needs: 4096 bytes
static constexpr size_t DMA_BUFFER_SIZE = 8192;

// 32-byte aligned buffer for ARM cache operations
// NO section attribute - goes to default .bss in AXI SRAM (0x24000000)
struct alignas(32) SharedDmaBuffer {
    uint8_t data[DMA_BUFFER_SIZE];
};

/**
 * Get the shared DMA buffer
 * Buffer is in AXI SRAM (default .bss section) for SDMMC1 IDMA compatibility
 * 
 * IMPORTANT: Caller is responsible for ensuring no concurrent SD operations
 */
inline SharedDmaBuffer& GetSharedDmaBuffer() {
    // No DMA_BUFFER_MEM_SECTION - stays in AXI SRAM for SDMMC1 IDMA
    static SharedDmaBuffer buffer;
    return buffer;
}

}  // namespace sd_utils
