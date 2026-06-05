/**
 * @file notator_dongle.h
 * @brief Atari Notator Dongle Logic - XC9536XL Emulation
 * @author Based on TPH reverse-engineering by Unnamed Villain
 * @license "YOU GOT IT FOR FREE then GIVE IT FOR FREE!"
 *
 * Implements the exact combinational and sequential logic from:
 * - notator_dongle.equ (ABEL equations)
 * - n_dongle.vf (Xilinx ISE netlist)
 * - notator_dongle_xilinx.jed (JEDEC bitstream)
 *
 * State machine:
 *   FEEDB1: D-FF clocked by ROM4, D = STER(address)
 *   D8-D15: 8 D-FFs clocked by feedb2, async reset on specific addresses
 *   feedb2 = (!FEEDB1 & UDS) | (FEEDB1 & !ROM3)
 *   STER   = A7 & A6 & A5 & !A4 & A3 & !A2 & A1 & !A8
 */

#ifndef NOTATOR_DONGLE_H
#define NOTATOR_DONGLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dongle state registers (emulating XC9536XL macrocells)
 * All flip-flops initialize to 0 (INIT=1'b0 in Verilog)
 */
typedef struct {
    uint8_t feedb1;   /**< Address match flip-flop (pin01, clocked by ROM4) */
    uint8_t d8;       /**< Data flip-flop D8  (bit 0 of data byte) */
    uint8_t d9;       /**< Data flip-flop D9  (bit 1) */
    uint8_t d10;      /**< Data flip-flop D10 (bit 2) */
    uint8_t d11;      /**< Data flip-flop D11 (bit 3) */
    uint8_t d12;      /**< Data flip-flop D12 (bit 4) */
    uint8_t d13;      /**< Data flip-flop D13 (bit 5) */
    uint8_t d14;      /**< Data flip-flop D14 (bit 6) */
    uint8_t d15;      /**< Data flip-flop D15 (bit 7) */
} notator_dongle_state_t;

/**
 * @brief Initialize dongle state (all FFs to 0)
 */
void notator_dongle_init(notator_dongle_state_t *dongle);

/**
 * @brief Compute STER (address match detector)
 * STER = A7 & A6 & A5 & !A4 & A3 & !A2 & A1 & !A8
 * @param addr A1-A8 address byte (A1 is bit 0, A8 is bit 7)
 * @return 1 if address matches trigger pattern, 0 otherwise
 */
static inline uint8_t notator_dongle_ster(uint8_t addr) {
    uint8_t a1  = (addr >> 0) & 1;
    uint8_t a2  = (addr >> 1) & 1;
    uint8_t a3  = (addr >> 2) & 1;
    uint8_t a4  = (addr >> 3) & 1;
    uint8_t a5  = (addr >> 4) & 1;
    uint8_t a6  = (addr >> 5) & 1;
    uint8_t a7  = (addr >> 6) & 1;
    uint8_t a8  = (addr >> 7) & 1;
    return (a7 && a6 && a5 && !a4 && a3 && !a2 && a1 && !a8) ? 1 : 0;
}

/**
 * @brief Compute feedb2 (data FF clock source)
 * feedb2 = (!FEEDB1 & UDS) | (FEEDB1 & !ROM3)
 */
static inline bool notator_dongle_feedb2(const notator_dongle_state_t *dongle,
                                          bool rom3_active, bool uds_active) {
    return ((!dongle->feedb1 && uds_active) || (dongle->feedb1 && rom3_active));
}

/**
 * @brief Clock FEEDB1 flip-flop (called on ROM4 activation)
 * FEEDB1 := STER(address)
 */
void notator_dongle_clock_feedb1(notator_dongle_state_t *dongle, uint8_t addr);

/**
 * @brief Compute next D8-D15 state and clock all data flip-flops
 * Called on feedb2 rising edge. Implements the exact /:= equations.
 * @param addr A1-A8 address byte
 * @param rom3_active ROM3 chip select state (for async reset check)
 */
void notator_dongle_clock_data(notator_dongle_state_t *dongle, uint8_t addr, bool rom3_active);

/**
 * @brief Apply asynchronous resets while ROM3 is active
 * D9.rst = FEEDB1 & A4 & !ROM3 & A2
 * D8.rst = FEEDB1 & A3 & !ROM3 & A1
 */
void notator_dongle_apply_async_resets(notator_dongle_state_t *dongle, uint8_t addr, bool rom3_active);

/**
 * @brief Get current data byte for D8-D15 bus drive
 * @return Byte with D8=bit0, D9=bit1, ..., D15=bit7
 */
static inline uint8_t notator_dongle_get_data(const notator_dongle_state_t *dongle) {
    return (dongle->d8  << 0) | (dongle->d9  << 1) | (dongle->d10 << 2) | (dongle->d11 << 3) |
           (dongle->d12 << 4) | (dongle->d13 << 5) | (dongle->d14 << 6) | (dongle->d15 << 7);
}

/**
 * @brief Full bus cycle handler
 * @param dongle Dongle state
 * @param addr A1-A8 address byte
 * @param rom3_active ROM3 chip select (active low -> true)
 * @param rom4_active ROM4 chip select (active low -> true)
 * @param uds_active UDS strobe (active low -> true)
 * @param prev_rom4 Previous ROM4 state (for edge detection)
 * @param prev_feedb2 Previous feedb2 state (for edge detection)
 * @return Current data byte to drive (only valid when ROM3 is active)
 */
uint8_t notator_dongle_bus_cycle(notator_dongle_state_t *dongle,
                                  uint8_t addr,
                                  bool rom3_active,
                                  bool rom4_active,
                                  bool uds_active,
                                  bool *prev_rom4,
                                  bool *prev_feedb2);

#ifdef __cplusplus
}
#endif

#endif /* NOTATOR_DONGLE_H */
