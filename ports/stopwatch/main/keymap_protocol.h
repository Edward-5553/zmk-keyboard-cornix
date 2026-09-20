// SPDX-License-Identifier: MIT
#pragma once
#include "engine.h"
#define KM_REPORT_ID 5
#define KM_REPORT_SIZE 63
#define KM_CHUNK 48
enum km_command {KM_INFO=1,KM_READ,KM_BEGIN,KM_WRITE,KM_COMMIT,KM_ABORT};
enum km_result {KM_OK,KM_BAD_REQUEST,KM_SCHEMA,KM_CONFLICT,KM_BUSY,KM_ORDER,KM_INVALID,KM_STORAGE,KM_EXPIRED};
struct km_protocol {
    uint8_t staging[KEYMAP_BYTES];
    uint32_t revision,token,touched;
    unsigned received;
    bool staging_active,saved;
};
typedef bool (*km_save_fn)(const uint8_t *,size_t);
uint32_t km_hash(const uint8_t *data,size_t size);
void km_protocol_init(struct km_protocol *p,bool saved);
void km_protocol_disconnect(struct km_protocol *p);
bool km_protocol_request(struct km_protocol *p,const uint8_t request[KM_REPORT_SIZE],
                         uint8_t response[KM_REPORT_SIZE],uint32_t now,km_save_fn save);
