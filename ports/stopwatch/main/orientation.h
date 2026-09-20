// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
struct orientation {
    float x,y,z,candidate,angle;
    uint32_t since;
    bool filtered,pending,valid;
};
// Screen axes: x right, y down; acceleration points upwards at rest. Values in g.
bool orientation_update(struct orientation *s,float x,float y,float z,uint32_t now);
float orientation_cardinal(float angle);
// Exact cardinal rotation for square RGB565 images; 0 degrees uses memcpy.
void orientation_rows(const uint16_t *src,uint16_t *dst,int width,int height,
                      int row,int rows,float angle);
