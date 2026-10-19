#pragma once

#include <stdbool.h>
#include <stdint.h>

// Start and receive calls run on the NimBLE host queue. Reset invalidates
// callbacks atomically; input_ready posts an event. Both may be called by tasks.
void switch2_battery_client_init(void);
void switch2_battery_client_reset(void);
void switch2_battery_client_start(uint16_t conn, uint16_t value, uint16_t cccd,
                                 void (*ready)(void));
void switch2_battery_client_input_ready(void);
void switch2_battery_client_primary(uint16_t conn, const uint8_t *data, uint16_t len);
void switch2_battery_client_fd2(uint16_t conn, const uint8_t *data, uint16_t len);
