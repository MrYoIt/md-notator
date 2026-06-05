/**
 * @file romemul.c
 * @brief Simplified ROM emulation engine for md-notator
 * @author Based on SidecarTridge template, modified for dongle emulation
 *
 * Monitors the 68000 bus via PIO/DMA and intercepts ROM3/ROM4 reads
 * in the cartridge space ($FA0000-$FBFFFF). For dongle accesses,
 * calls the notator_dongle_intercept() function instead of reading
 * from ROM_IN_RAM.
 *
 * This is a minimal implementation - the full template has more features
 * (command protocol, GEMDRIVE, etc.) that we don't need for a dongle emulator.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "romemul.h"
#include "notator_dongle.h"

/* === PIO Programs === */
// romemul.pio - 68000 bus monitor (simplified for dongle only)
// This would normally be compiled from .pio file, but we include inline

#include "romemul.pio.h"

/* === Constants === */
#define ROM_IN_RAM_SIZE         65536     // 64KB shared region
#define ROM_IN_RAM_ADDRESS      0x20030000 // RP2040 RAM address
#define CART_START_ADDRESS      0xFA0000   // Atari cartridge space
#define CART_END_ADDRESS        0xFBFFFF

/* === Globals === */
static uint8_t *rom_in_ram = (uint8_t *)ROM_IN_RAM_ADDRESS;
static PIO pio = pio0;
static uint sm = 0;
static uint dma_chan = 0;

/* === External dongle intercept function (defined in main.c) === */
extern uint16_t notator_dongle_intercept(uint32_t addr, bool rom3, bool rom4);

/* === Bus cycle handler === */
static void __not_in_flash_func(handle_bus_cycle)(uint32_t addr_full, bool rom3, bool rom4) {
    // Check if in cartridge space
    if ((addr_full & 0xFF8000) != CART_START_ADDRESS) {
        return; // Not our space
    }

    // Try dongle intercept first
    uint16_t dongle_data = notator_dongle_intercept(addr_full, rom3, rom4);

    if (dongle_data != 0xFFFF) {
        // Dongle handled it - write data to shared RAM for 68000 to read
        // In real hardware, this would drive the data bus directly
        // For now, we write to the shared region
        uint32_t offset = (addr_full - CART_START_ADDRESS) & (ROM_IN_RAM_SIZE - 1);
        rom_in_ram[offset] = (uint8_t)(dongle_data & 0xFF);     // Lower byte
        rom_in_ram[offset + 1] = (uint8_t)((dongle_data >> 8) & 0xFF); // Upper byte
    }
    // else: pass through to normal ROM read (if we had a ROM image)
}

/* === PIO IRQ handler === */
static void __not_in_flash_func(romemul_pio_irq_handler)(void) {
    // Read PIO FIFO for bus cycle info
    // Format: [31:8] = address, [7:4] = control, [3:0] = data (if write)
    while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        uint32_t word = pio_sm_get(pio, sm);
        uint32_t addr = (word >> 8) & 0xFFFFFF;
        bool rom3 = (word & 0x10) != 0;
        bool rom4 = (word & 0x20) != 0;

        handle_bus_cycle(addr, rom3, rom4);
    }

    // Clear IRQ
    pio_interrupt_clear(pio, 0);
}

/* === Initialization === */
void romemul_init(void) {
    // Load PIO program
    uint offset = pio_add_program(pio, &romemul_program);
    pio_sm_config cfg = romemul_program_get_default_config(offset);

    // Configure PIO state machine
    sm_config_set_in_pins(&cfg, 0);  // GPIO 0-23 for address/control
    sm_config_set_out_pins(&cfg, 0, 16); // GPIO 0-15 for data
    sm_config_set_sideset_pins(&cfg, 0);
    sm_config_set_clkdiv(&cfg, 1.0f); // Full speed

    pio_sm_init(pio, sm, offset, &cfg);

    // Setup DMA for bulk transfers (optional, for ROM loading)
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_cfg, true);
    channel_config_set_write_increment(&dma_cfg, true);

    // Enable PIO IRQ
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, romemul_pio_irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
}

void romemul_start(void) {
    pio_sm_set_enabled(pio, sm, true);
}

void romemul_stop(void) {
    pio_sm_set_enabled(pio, sm, false);
}

/* === ROM loading (not used for dongle, but kept for compatibility) === */
bool load_rom(const char *filename, uint8_t *buffer, size_t size) {
    // Not needed for dongle emulator
    return true;
}

void eject_rom(void) {
    // Not needed for dongle emulator
}
