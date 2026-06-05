/**
 * @file emul.c
 * @brief md-notator emulation critical path
 * @author Based on TPH reverse-engineering by Unnamed Villain
 * @license "YOU GOT IT FOR FREE then GIVE IT FOR FREE!"
 *
 * This is the critical path of the Notator dongle emulation.
 * 
 * Architecture:
 * - ROM4 DMA engine: Disabled for dongle space ($FA0000-$FBFFFF)
 * - ROM3 DMA ring:  Used for commands (terminal, config)
 * - Core 1:         Real-time dongle bus polling loop
 * - Core 0:         Terminal, config, UI
 *
 * The dongle is a stateful challenge-response device (XC9536XL CPLD).
 * It CANNOT be emulated with a static ROM image because:
 *   - It has internal state (8-bit data register + FEEDB1 flip-flop)
 *   - Data outputs feed back into inputs
 *   - Same address returns different data on repeated reads
 *   - Requires edge-triggered clocking on ROM3/ROM4/UDS
 *
 * Therefore, we intercept ROM4 accesses in software on Core 1,
 * compute the dongle response dynamically, and drive the data bus.
 */

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"

#include "emul.h"
#include "chandler.h"
#include "tprotocol.h"
#include "display_term.h"
#include "select.h"
#include "gconfig.h"
#include "sdcard.h"
#include "network.h"
#include "download.h"
#include "reset.h"
#include "romemul.h"
#include "commemul.h"
#include "notator_dongle.h"

/* === Configuration === */

// GPIO mapping for SidecarTridge Multi-device v2
#define PIN_BUS_START   6
#define PIN_BUS_END     21
#define PIN_BUS_MASK    0x0003FFC0

#define PIN_ROM3        22
#define PIN_ROM4        26
#define PIN_READ        27
#define PIN_WRITE       28
#define PIN_SELECT      5
#define PIN_LED         25

// Cartridge space detection (A15-A8 pattern for $FA/$FB)
#define CART_A15_A8_FA  0x7A  // 01111010: A15=0,A14=1,A13=1,A12=1,A11=1,A10=1,A9=0,A8=1
#define CART_A15_A8_FB  0x7B  // 01111011

/* === Global State === */
static notator_dongle_state_t g_dongle;
static volatile bool g_dongle_enabled = true;
static volatile bool g_bus_active = false;
static bool g_prev_rom4 = false;
static bool g_prev_feedb2 = false;

/* === Fast GPIO Macros === */
static inline uint32_t read_bus_raw(void) {
    return (sio_hw->gpio_in >> PIN_BUS_START) & 0xFFFF;
}

static inline bool read_rom3(void) {
    return !(gpio_get(PIN_ROM3));
}

static inline bool read_rom4(void) {
    return !(gpio_get(PIN_ROM4));
}

static inline void set_bus_input(void) {
    gpio_put(PIN_READ, 0);
    gpio_put(PIN_WRITE, 1);
    for (int i = PIN_BUS_START; i <= PIN_BUS_END; i++) {
        gpio_set_dir(i, GPIO_IN);
    }
}

static inline void set_bus_output(void) {
    gpio_put(PIN_READ, 1);
    gpio_put(PIN_WRITE, 0);
    for (int i = PIN_BUS_START; i <= PIN_BUS_END; i++) {
        gpio_set_dir(i, GPIO_OUT);
    }
}

static inline void drive_data_upper(uint8_t data) {
    uint32_t bus_val = ((uint32_t)data) << 8;
    uint32_t gpio_val = bus_val << PIN_BUS_START;
    gpio_put_masked(PIN_BUS_MASK, gpio_val);
}

static inline bool is_cartridge_space(uint16_t addr) {
    uint8_t high_byte = (addr >> 8) & 0xFF;
    return (high_byte == CART_A15_A8_FA) || (high_byte == CART_A15_A8_FB);
}

/* === GPIO Initialization === */
static void dongle_gpio_init(void) {
    // Shared bus
    for (int i = PIN_BUS_START; i <= PIN_BUS_END; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_down(i);
    }
    // ROM3
    gpio_init(PIN_ROM3);
    gpio_set_dir(PIN_ROM3, GPIO_IN);
    gpio_pull_up(PIN_ROM3);
    // ROM4
    gpio_init(PIN_ROM4);
    gpio_set_dir(PIN_ROM4, GPIO_IN);
    gpio_pull_up(PIN_ROM4);
    // !READ
    gpio_init(PIN_READ);
    gpio_set_dir(PIN_READ, GPIO_OUT);
    gpio_put(PIN_READ, 1);
    // !WRITE
    gpio_init(PIN_WRITE);
    gpio_set_dir(PIN_WRITE, GPIO_OUT);
    gpio_put(PIN_WRITE, 1);
    // SELECT
    gpio_init(PIN_SELECT);
    gpio_set_dir(PIN_SELECT, GPIO_IN);
    gpio_pull_down(PIN_SELECT);
    // LED
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, 0);
}

/* === Core 1: Real-time Dongle Emulation Loop === */
static void __not_in_flash_func(dongle_emulation_loop)(void) {
    bool last_rom3 = false;
    bool last_rom4 = false;
    bool last_feedb2 = false;

    set_bus_input();

    while (true) {
        bool rom3 = read_rom3();
        bool rom4 = read_rom4();

        bool rom3_falling = !rom3 && last_rom3;
        bool rom4_falling = !rom4 && last_rom4;
        bool any_rom_active = rom3 || rom4;

        if (any_rom_active) {
            uint16_t addr_raw = read_bus_raw();
            uint8_t addr_byte = (uint8_t)(addr_raw & 0xFF);

            if (is_cartridge_space(addr_raw)) {
                g_bus_active = true;

                if (rom4_falling) {
                    notator_dongle_clock_feedb1(&g_dongle, addr_byte);
                }

                bool uds_active = true;
                bool feedb2 = notator_dongle_feedb2(&g_dongle, rom3, uds_active);

                bool feedb2_rising = feedb2 && !last_feedb2;
                if (feedb2_rising) {
                    notator_dongle_clock_data(&g_dongle, addr_byte, rom3);
                }

                if (rom3) {
                    uint8_t data = notator_dongle_get_data(&g_dongle);
                    set_bus_output();
                    drive_data_upper(data);

                    while (read_rom3()) {
                        tight_loop_contents();
                    }

                    set_bus_input();
                }

                last_feedb2 = feedb2;
            }
        } else {
            g_bus_active = false;
            set_bus_input();
        }

        last_rom3 = rom3;
        last_rom4 = rom4;
    }
}

static void core1_main(void) {
    dongle_emulation_loop();
}

/* === Terminal Command Handler === */
static void notator_term_command_cb(TransmissionProtocol *protocol, uint16_t *payloadPtr) {
    uint16_t command = protocol->command;
    switch (command) {
        case DISPLAY_COMMAND_DONGLE_STATUS:
            // Return current dongle state
            break;
        default:
            break;
    }
}

/* === Emulation Start === */
void emul_start(void) {
    DPRINTF("md-notator v1.0\n");
    DPRINTF("Notator Dongle Emulator for SidecarTridge Multi-device\n");
    DPRINTF("Based on TPH reverse-engineering by Unnamed Villain\n\n");

    // Initialize dongle state
    notator_dongle_init(&g_dongle);
    g_prev_rom4 = false;
    g_prev_feedb2 = false;
    g_dongle_enabled = true;

    // Initialize dongle GPIO
    dongle_gpio_init();

    // Startup LED blink
    for (int i = 0; i < 3; i++) {
        gpio_put(PIN_LED, 1);
        sleep_ms(100);
        gpio_put(PIN_LED, 0);
        sleep_ms(100);
    }

    // Check if we should enter config mode
    if (select_detectPush()) {
        DPRINTF("SELECT pressed - entering config mode\n");
        // Config mode: init ROM4 engine for terminal, ROM3 for commands
        init_romemul(false);
        commemul_init();
        chandler_init();
        chandler_addCB(term_command_cb);
        chandler_addCB(notator_term_command_cb);

        // Main config loop
        while (!select_detectPush()) {
            chandler_loop();
            term_loop();
            sleep_ms(SLEEP_LOOP_MS);
        }
        select_waitPush();

        DPRINTF("Exiting config mode, starting emulation\n");
    }

    // Start dongle emulation on Core 1
    DPRINTF("Starting dongle emulation on Core 1\n");
    multicore_launch_core1(core1_main);

    // Core 0: handle SELECT button and status
    DPRINTF("Dongle active. Press SELECT for config.\n");

    while (getKeepActive()) {
        // Check for SELECT press (enter config)
        if (select_detectPush()) {
            DPRINTF("SELECT pressed - entering config\n");
            // Signal Core 1 to stop? For now, just enter config
            // In a full implementation, we'd need a shared flag
            break;
        }

        // Heartbeat LED
        if (g_dongle_enabled) {
            static absolute_time_t last_blink;
            if (absolute_time_diff_us(last_blink, get_absolute_time()) > 500000) {
                gpio_put(PIN_LED, !gpio_get(PIN_LED));
                last_blink = get_absolute_time();
            }
        }

        sleep_ms(SLEEP_LOOP_MS);
    }

    DPRINTF("Emulation stopped\n");
}

/* === Poll Tick (for network callbacks) === */
void emul_pollTick(void) {
    chandler_loop();
    term_loop();
}
