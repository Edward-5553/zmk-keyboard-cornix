// SPDX-License-Identifier: MIT
#pragma once
#include "engine.h"
enum output_route { OUTPUT_OFF, OUTPUT_USB, OUTPUT_BLE };
struct output_router {
    enum output_route route;
    uint32_t session;
    output_fn usb, ble;
};
void output_router_send(const struct output *out, void *context);
// Caller cancels engine through OLD route before changing, and NEW route afterwards.
enum output_route output_route_select(bool usb_mounted, bool usb_suspended, uint32_t ble_session);
bool output_router_update(struct output_router *router, bool mounted, bool suspended,
                          uint32_t ble_session, bool overflow);
// HOGP notifications omit the report ID; the Report Reference descriptor supplies it.
unsigned output_hid_report(const struct output *out, uint8_t bytes[8], unsigned *index);
