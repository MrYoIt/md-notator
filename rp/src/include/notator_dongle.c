/**
 * @file notator_dongle.c
 * @brief Atari Notator Dongle - XC9536XL Logic Implementation
 * @author Based on TPH reverse-engineering by Unnamed Villain
 * @license "YOU GOT IT FOR FREE then GIVE IT FOR FREE!"
 *
 * Exact implementation of the equations from notator_dongle.equ:
 *
 * FEEDB1 := A7* A6* A5*/A4* A3*/A2* A1*/A8;  Clocked by ROM4 on pin01
 * FEED      = A7* A6* A5*/A4* A3*/A2* A1*/A8; = STER w ISE
 * INT_CON_13 = /FEEDB1* UDS + FEEDB1*/ROM3;    = feedb2
 *
 * D9.ena   = /ROM3;
 * D9.rst   = FEEDB1* A4*/ROM3* A2;
 * D9 /:=   A5*/D13 + A4*D15 + A1*/D8*/D15 + A1*/D10*/D15 + A1*/D12*/D15 + A1*/D15*/D14 + A1*D8*D10*D12*D15*D14 + STER;
 *
 * D8.ena   = /ROM3;
 * D8.rst   = FEEDB1* A3*/ROM3* A1;
 * D8 /:=   A5*/D14 + A1*/D9*/D14 + A4*/D9*D15 + A4*/D9*D13 + A4*/D9*D11 + A4*D9*/D11*/D13*/D15 + STER + A1*D9*D8*D11*D10*D13*D12*D15*D14;
 *
 * D11.ena  = /ROM3;
 * D11 /:=  A5*/D15 + A1*/D8*/D10 + A1*/D8*/D12 + A1*/D8*/D14 + A4*/D8*D15 + A1*D8*D10*D12*D14 + STER + A4*/D9*D8*/D11*/D10*/D13*/D12*/D15*/D14;
 *
 * D10.ena  = /ROM3;
 * D10 /:=  A5*/D9 + A1*/D11*/D12 + A1*/D11*/D14 + A4*/D11*D15 + A4*/D11*D13 + A4*D11*/D13*/D15 + STER + A1*D8*D11*D10*D13*D12*D15*D14;
 *
 * D13.ena  = /ROM3;
 * D13 /:=  A5*/D11 + A1*/D10*/D13 + A1*/D13*/D12 + A1*/D13*/D14 + A4*/D13*D15 + A4*D13*/D15 + A1*D8*D10*D13*D12*D15*D14 + STER;
 *
 * D12.ena  = /ROM3;
 * D12 /:=  A5*/D10 + A1*/D12*/D14 + A4*/D12*D15 + A4*D13*/D12 + A4*D11*/D12 + A1*D12*D14 + A4*/D9*/D11*/D13*D12*/D15*/D14 + STER;
 *
 * D15.ena  = /ROM3;
 * D15 /:=  A5*/D8 + A1*/D10*/D12 + A1*/D10*/D14 + A4*/D10*D15 + A4*/D10*D13 + A1*D10*D12*D14 + A4*/D9*/D11*D10*/D13*/D12*/D15*/D14 + STER;
 *
 * D14.ena  = /ROM3;
 * D14 /:=  A5*/D12 + A1*D14 + A4*D15*/D14 + A4*D13*/D14 + A4*D11*/D14 + A4*D9*/D14 + A4*/D9*/D11*/D13*/D15*D14 + STER;
 */

#include "notator_dongle.h"

void notator_dongle_init(notator_dongle_state_t *dongle) {
    dongle->feedb1 = 0;
    dongle->d8  = 0;
    dongle->d9  = 0;
    dongle->d10 = 0;
    dongle->d11 = 0;
    dongle->d12 = 0;
    dongle->d13 = 0;
    dongle->d14 = 0;
    dongle->d15 = 0;
}

void notator_dongle_clock_feedb1(notator_dongle_state_t *dongle, uint8_t addr) {
    dongle->feedb1 = notator_dongle_ster(addr);
}

void notator_dongle_apply_async_resets(notator_dongle_state_t *dongle, uint8_t addr, bool rom3_active) {
    uint8_t a1 = (addr >> 0) & 1;
    uint8_t a2 = (addr >> 1) & 1;
    uint8_t a3 = (addr >> 2) & 1;
    uint8_t a4 = (addr >> 3) & 1;

    if (rom3_active) {
        if (dongle->feedb1 && a4 && a2) {
            dongle->d9 = 0;
        }
        if (dongle->feedb1 && a3 && a1) {
            dongle->d8 = 0;
        }
    }
}

void notator_dongle_clock_data(notator_dongle_state_t *dongle, uint8_t addr, bool rom3_active) {
    uint8_t a1 = (addr >> 0) & 1;
    uint8_t a4 = (addr >> 3) & 1;
    uint8_t a5 = (addr >> 4) & 1;

    uint8_t d8  = dongle->d8;
    uint8_t d9  = dongle->d9;
    uint8_t d10 = dongle->d10;
    uint8_t d11 = dongle->d11;
    uint8_t d12 = dongle->d12;
    uint8_t d13 = dongle->d13;
    uint8_t d14 = dongle->d14;
    uint8_t d15 = dongle->d15;

    uint8_t ster_val = notator_dongle_ster(addr);

    /* D9 /:= A5*/D13 + A4*D15 + A1*/D8*/D15 + A1*/D10*/D15 + A1*/D12*/D15 + A1*/D15*/D14 + A1*D8*D10*D12*D15*D14 + STER */
    uint8_t d9_next = !( (a5 && !d13) ||
                         (a4 && d15) ||
                         (a1 && !d8  && !d15) ||
                         (a1 && !d10 && !d15) ||
                         (a1 && !d12 && !d15) ||
                         (a1 && !d15 && !d14) ||
                         (a1 && d8 && d10 && d12 && d15 && d14) ||
                         ster_val );

    /* D8 /:= A5*/D14 + A1*/D9*/D14 + A4*/D9*D15 + A4*/D9*D13 + A4*/D9*D11 + A4*D9*/D11*/D13*/D15 + STER + A1*D9*D8*D11*D10*D13*D12*D15*D14 */
    uint8_t d8_next = !( (a5 && !d14) ||
                         (a1 && !d9 && !d14) ||
                         (a4 && !d9 && d15) ||
                         (a4 && !d9 && d13) ||
                         (a4 && !d9 && d11) ||
                         (a4 && d9 && !d11 && !d13 && !d15) ||
                         ster_val ||
                         (a1 && d9 && d8 && d11 && d10 && d13 && d12 && d15 && d14) );

    /* D11 /:= A5*/D15 + A1*/D8*/D10 + A1*/D8*/D12 + A1*/D8*/D14 + A4*/D8*D15 + A1*D8*D10*D12*D14 + STER + A4*/D9*D8*/D11*/D10*/D13*/D12*/D15*/D14 */
    uint8_t d11_next = !( (a5 && !d15) ||
                          (a1 && !d8 && !d10) ||
                          (a1 && !d8 && !d12) ||
                          (a1 && !d8 && !d14) ||
                          (a4 && !d8 && d15) ||
                          (a1 && d8 && d10 && d12 && d14) ||
                          ster_val ||
                          (a4 && !d9 && d8 && !d11 && !d10 && !d13 && !d12 && !d15 && !d14) );

    /* D10 /:= A5*/D9 + A1*/D11*/D12 + A1*/D11*/D14 + A4*/D11*D15 + A4*/D11*D13 + A4*D11*/D13*/D15 + STER + A1*D8*D11*D10*D13*D12*D15*D14 */
    uint8_t d10_next = !( (a5 && !d9) ||
                          (a1 && !d11 && !d12) ||
                          (a1 && !d11 && !d14) ||
                          (a4 && !d11 && d15) ||
                          (a4 && !d11 && d13) ||
                          (a4 && d11 && !d13 && !d15) ||
                          ster_val ||
                          (a1 && d8 && d11 && d10 && d13 && d12 && d15 && d14) );

    /* D13 /:= A5*/D11 + A1*/D10*/D13 + A1*/D13*/D12 + A1*/D13*/D14 + A4*/D13*D15 + A4*D13*/D15 + A1*D8*D10*D13*D12*D15*D14 + STER */
    uint8_t d13_next = !( (a5 && !d11) ||
                          (a1 && !d10 && !d13) ||
                          (a1 && !d13 && !d12) ||
                          (a1 && !d13 && !d14) ||
                          (a4 && !d13 && d15) ||
                          (a4 && d13 && !d15) ||
                          (a1 && d8 && d10 && d13 && d12 && d15 && d14) ||
                          ster_val );

    /* D12 /:= A5*/D10 + A1*/D12*/D14 + A4*/D12*D15 + A4*D13*/D12 + A4*D11*/D12 + A1*D12*D14 + A4*/D9*/D11*/D13*D12*/D15*/D14 + STER */
    uint8_t d12_next = !( (a5 && !d10) ||
                          (a1 && !d12 && !d14) ||
                          (a4 && !d12 && d15) ||
                          (a4 && d13 && !d12) ||
                          (a4 && d11 && !d12) ||
                          (a1 && d12 && d14) ||
                          (a4 && !d9 && !d11 && !d13 && d12 && !d15 && !d14) ||
                          ster_val );

    /* D15 /:= A5*/D8 + A1*/D10*/D12 + A1*/D10*/D14 + A4*/D10*D15 + A4*/D10*D13 + A1*D10*D12*D14 + A4*/D9*/D11*D10*/D13*/D12*/D15*/D14 + STER */
    uint8_t d15_next = !( (a5 && !d8) ||
                          (a1 && !d10 && !d12) ||
                          (a1 && !d10 && !d14) ||
                          (a4 && !d10 && d15) ||
                          (a4 && !d10 && d13) ||
                          (a1 && d10 && d12 && d14) ||
                          (a4 && !d9 && !d11 && d10 && !d13 && !d12 && !d15 && !d14) ||
                          ster_val );

    /* D14 /:= A5*/D12 + A1*D14 + A4*D15*/D14 + A4*D13*/D14 + A4*D11*/D14 + A4*D9*/D14 + A4*/D9*/D11*/D13*/D15*D14 + STER */
    uint8_t d14_next = !( (a5 && !d12) ||
                          (a1 && d14) ||
                          (a4 && d15 && !d14) ||
                          (a4 && d13 && !d14) ||
                          (a4 && d11 && !d14) ||
                          (a4 && d9 && !d14) ||
                          (a4 && !d9 && !d11 && !d13 && !d15 && d14) ||
                          ster_val );

    dongle->d8  = d8_next;
    dongle->d9  = d9_next;
    dongle->d10 = d10_next;
    dongle->d11 = d11_next;
    dongle->d12 = d12_next;
    dongle->d13 = d13_next;
    dongle->d14 = d14_next;
    dongle->d15 = d15_next;

    notator_dongle_apply_async_resets(dongle, addr, rom3_active);
}

uint8_t notator_dongle_bus_cycle(notator_dongle_state_t *dongle,
                                  uint8_t addr,
                                  bool rom3_active,
                                  bool rom4_active,
                                  bool uds_active,
                                  bool *prev_rom4,
                                  bool *prev_feedb2) {

    bool rom4_rising = !*prev_rom4 && rom4_active;
    if (rom4_rising) {
        notator_dongle_clock_feedb1(dongle, addr);
    }
    *prev_rom4 = rom4_active;

    bool feedb2 = notator_dongle_feedb2(dongle, rom3_active, uds_active);

    bool feedb2_rising = feedb2 && !*prev_feedb2;
    if (feedb2_rising) {
        notator_dongle_clock_data(dongle, addr, rom3_active);
    }
    *prev_feedb2 = feedb2;

    if (rom3_active) {
        return notator_dongle_get_data(dongle);
    }

    return 0x00;
}
