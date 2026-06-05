/**
 * @file romemul.h
 * @brief ROM emulation engine header for md-notator
 */

#ifndef ROMEMUL_H
#define ROMEMUL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ROM emulation engine (PIO/DMA bus monitor)
 */
void romemul_init(void);

/**
 * @brief Start ROM emulation
 */
void romemul_start(void);

/**
 * @brief Stop ROM emulation
 */
void romemul_stop(void);

/**
 * @brief Load ROM image (not used for dongle, kept for compatibility)
 */
bool load_rom(const char *filename, uint8_t *buffer, size_t size);

/**
 * @brief Eject ROM (not used for dongle, kept for compatibility)
 */
void eject_rom(void);

#ifdef __cplusplus
}
#endif

#endif /* ROMEMUL_H */
