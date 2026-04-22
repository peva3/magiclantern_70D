/**
 * Host-side stubs for 70D development
 * Allows compiling ML code on x86/Linux for logic testing
 */

#ifndef _HOST_STUBS_H
#define _HOST_STUBS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_70D 1
#define CONFIG_70D_112 1

typedef int8_t sint8;
typedef uint8_t uint8;
typedef int16_t sint16;
typedef uint16_t uint16;
typedef int32_t sint32;
typedef uint32_t uint32;
typedef int64_t sint64;
typedef uint64_t uint64;

#define NULL ((void*)0)
#define IS_ERROR(x) ((uint32)(x) >= 0xF0000000)

typedef struct {
    uint32_t registers[0x10000];
    uint32_t evf_state;
    uint32_t focus_data;
    uint32_t iso;
    uint32_t shutter;
    uint32_t aperture;
    uint32_t vsync_count;
} mock_camera_state_t;

static mock_camera_state_t g_camera;

void mock_init_70d() {
    memset(&g_camera, 0, sizeof(g_camera));
    g_camera.evf_state = 0x12345;
    g_camera.focus_data = 0;
    g_camera.iso = 100;
    g_camera.shutter = 50;
    g_camera.aperture = 28;
    g_camera.vsync_count = 0;
}

uint32_t mock_read_register(uint32_t addr) {
    if (addr < 0x10000) {
        return g_camera.registers[addr / 4];
    }
    return 0;
}

void mock_write_register(uint32_t addr, uint32_t val) {
    if (addr < 0x10000) {
        g_camera.registers[addr / 4] = val;
    }
}

int task_create(const char* name, int priority, int stack_size, void (*entry)(void*), void* arg) {
    printf("[TASK] Create '%s' prio=%d stack=%d\n", name, priority, stack_size);
    return 0;
}

void msleep(int ms) {
    // No-op in host tests
}

#define printf(...) fprintf(stdout, __VA_ARGS__)

void* fio_malloc(size_t size) {
    return malloc(size);
}

void fio_free(void* ptr) {
    free(ptr);
}

#endif // _HOST_STUBS_H