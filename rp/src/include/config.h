/**
 * @file config.h
 * @brief Minimal configuration for md-notator
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi credentials (not used for dongle, but required by template)
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// IP configuration (not used for dongle)
#define IP_ADDRESS "0.0.0.0"
#define IP_GATEWAY "0.0.0.0"
#define IP_NETMASK "255.255.255.0"
#define IP_DNS "8.8.8.8"

// ROM emulation settings
#define ROM_SIZE 65536
#define ROM_START_ADDRESS 0xFA0000

// Feature flags (all disabled for dongle-only)
#define ENABLE_GEMDRIVE 0
#define ENABLE_FLOPPY 0
#define ENABLE_RTC 0
#define ENABLE_NETWORK 0
#define ENABLE_WIFI 0

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
