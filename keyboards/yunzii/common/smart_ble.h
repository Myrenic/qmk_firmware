#pragma once
#include <stdint.h>
#include "quantum.h"

void sc_ble_battary(uint8_t batt_level);
void wireless_start(uint32_t mode);
void wireless_pair(uint32_t mode);
void wireless_stop(void);
bool is_wireless_connected(void);
void smart_ble_task(void);
