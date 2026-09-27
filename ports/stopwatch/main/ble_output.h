// SPDX-License-Identifier: MIT
#pragma once
#include "engine.h"
#include "host/ble_hs.h"

// Init/sync and identity lookup run on the NimBLE host task; output runs on input task.
void ble_output_init(void);
void ble_output_sync(uint8_t address_type);
bool ble_output_is_host(const ble_addr_t *address);
uint32_t ble_output_session(void); // Zero when encrypted keyboard notifications are unavailable.
bool ble_output_waiting(void);
void ble_output_send(const struct output *out, void *context);
void ble_output_forget(void); // Asynchronous; USB management only, preserves split bonds.
void ble_output_status(uint8_t status[8]);
