/**
 * @file main.c
 * @brief md-notator - SidecarTridge Notator Dongle Microfirmware
 * @author Based on TPH reverse-engineering by Unnamed Villain
 * @license "YOU GOT IT FOR FREE then GIVE IT FOR FREE!"
 *
 * Minimal main for dongle-only operation. No GEMDRIVE, no floppy, no RTC.
 * Just the ROM emulation engine with dongle logic intercept.
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
#include "pico/util/queue.h"
#include "pico/unique_id.h"
#include "notator_dongle.h"
#include "romemul.h"
#include "config.h"

/* === Configuration === */
#define PIN_LED         25
#define PIN_SELECT      5

/* === Dongle State === */
static notator_dongle_state_t g_dongle;
static bool g_prev_rom4 = false;
static bool g_prev_feedb2 = false;
static volatile bool g_dongle_enabled = true;

/* === LED Helpers === */
static inline void led_on(void) { gpio_put(PIN_LED, 1); }
static inline void led_off(void) { gpio_put(PIN_LED, 0); }
static inline void led_toggle(void) { gpio_put(PIN_LED, !gpio_get(PIN_LED)); }

static void led_blink(int count, int ms) {
    for (int i = 0; i < count; i++) {
        led_on(); sleep_ms(ms);
        led_off(); sleep_ms(ms);
    }
}

/* === Dongle Logic Intercept === */
/**
 * @brief Called by romemul.c on every ROM4 read cycle.
 * Intercepts cartridge space ($FA0000-$FBFFFF) and applies dongle logic.
 *
 * @param addr_full Full 24-bit address (A0-A23)
 * @param rom3_active ROM3 chip select (active low, true=asserted)
 * @param rom4_active ROM4 chip select (active low, true=asserted)
 * @return 16-bit data to return, or 0xFFFF to pass through to normal ROM read
 */
uint16_t notator_dongle_intercept(uint32_t addr_full, bool rom3_active, bool rom4_active) {
    // Check if in cartridge space: $FA0000-$FBFFFF
    // A23=1, A22=0, A21=1, A20=1, A19=1, A18=1, A17=1, A16=1, A15=0
    if ((addr_full & 0xFF8000) != 0xFA0000) {
        return 0xFFFF; // Pass through
    }

    // Extract A1-A8 for dongle logic (A1 is bit 0 of addr_full)
    uint8_t addr_byte = (uint8_t)((addr_full >> 1) & 0xFF);

    // UDS is active for upper byte access (we assume always active for ROM3/ROM4)
    bool uds_active = true;

    // Run dongle state machine
    uint8_t data = notator_dongle_bus_cycle(&g_dongle, addr_byte,
                                             rom3_active, rom4_active,
                                             uds_active,
                                             &g_prev_rom4, &g_prev_feedb2);

    // Return data in upper byte (D8-D15), lower byte is 0x00
    // The dongle only drives D8-D15
    return ((uint16_t)data << 8) | 0x00;
}

/* === Core 1: Real-time Bus Monitor === */
static void core1_main(void) {
    // Initialize dongle state
    notator_dongle_init(&g_dongle);
    g_prev_rom4 = false;
    g_prev_feedb2 = false;

    // Start ROM emulation engine (PIO/DMA bus monitor)
    // This will call notator_dongle_intercept() for each read cycle
    romemul_init();
    romemul_start();

    // Core 1 runs the bus monitor forever
    while (1) {
        tight_loop_contents();
    }
}

/* === Core 0: UI and Debug === */
static void handle_button(void) {
    static absolute_time_t press_start;
    static bool was_pressed = false;

    bool pressed = !gpio_get(PIN_SELECT); // Active low

    if (pressed && !was_pressed) {
        press_start = get_absolute_time();
        was_pressed = true;
    } else if (!pressed && was_pressed) {
        int64_t duration = absolute_time_diff_us(press_start, get_absolute_time());
        was_pressed = false;

        if (duration > 1000000) {
            // Long press: toggle dongle
            g_dongle_enabled = !g_dongle_enabled;
            if (g_dongle_enabled) {
                notator_dongle_init(&g_dongle);
                g_prev_rom4 = false;
                g_prev_feedb2 = false;
            }
            led_blink(g_dongle_enabled ? 2 : 1, 200);
        } else {
            // Short press: status blink
            led_blink(1, 50);
        }
    }
}

/* === Main Entry Point === */
int main(void) {
    stdio_init_all();

    // Initialize GPIO
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    led_off();

    gpio_init(PIN_SELECT);
    gpio_set_dir(PIN_SELECT, GPIO_IN);
    gpio_pull_down(PIN_SELECT);

    // Startup blink
    led_blink(3, 100);

    printf("\n");
    printf("========================================\n");
    printf("  md-notator v1.0\n");
    printf("  SidecarTridge Notator Dongle Emulator\n");
    printf("  XC9536XL logic on RP2040\n");
    printf("========================================\n");
    printf("\n");
    printf("Dongle state initialized.\n");
    printf("ROM3/ROM4 cartridge space: $FA0000-$FBFFFF\n");
    printf("Trigger address: $0074 (A1-A8 = 01110101)\n");
    printf("\n");
    printf("Long press SELECT (>1s) to toggle dongle.\n");
    printf("\n");

    // Launch dongle emulation on Core 1
    multicore_launch_core1(core1_main);

    // Core 0: Handle button, USB CDC debug, status LED
    while (true) {
        handle_button();

        // Heartbeat LED when dongle is active
        if (g_dongle_enabled) {
            static absolute_time_t last_blink;
            if (absolute_time_diff_us(last_blink, get_absolute_time()) > 500000) {
                led_toggle();
                last_blink = get_absolute_time();
            }
        }

        // Debug output every 5 seconds
        static absolute_time_t last_debug;
        if (absolute_time_diff_us(last_debug, get_absolute_time()) > 5000000) {
            printf("Status: FEEDB1=%d DATA=$%02X ENABLED=%d\n",
                   g_dongle.feedb1,
                   notator_dongle_get_data(&g_dongle),
                   g_dongle_enabled);
            last_debug = get_absolute_time();
        }

        tight_loop_contents();
    }

    return 0;
}
