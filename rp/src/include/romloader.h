/**
 * @file romloader.h
 * @brief ROM loader header (stub for md-notator)
 */

#ifndef ROMLOADER_H
#define ROMLOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool romloader_init(void);
void romloader_start(void);
void romloader_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* ROMLOADER_H */
