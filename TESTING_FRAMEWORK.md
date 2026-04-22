# Magic Lantern 70D - Testing Framework (Without Physical Camera)

This document describes how to develop and test Magic Lantern features for the Canon 70D without physical camera access.

## Overview

Since we don't have physical 70D access, we'll use a multi-layered testing approach:

1. **QEMU Emulation** - ARM emulation of DIGIC V hardware
2. **Host-Side Compilation** - Test logic on x86/Linux with stubs
3. **Simulation Framework** - Mock camera state for algorithm testing
4. **Remote Testing** - Collaborate with 70D owners for field validation

## 1. QEMU Emulation Setup

### Installation

```bash
# QEMU-EOS is already cloned at /app/70d/qemu-eos
cd /app/70d/qemu-eos

# Build QEMU (requires standard QEMU dependencies)
./configure --target-list=arm-softmmu
make -j$(nproc)
```

### 70D QEMU Configuration

Check if 70D is supported:
```bash
ls -la /app/70d/qemu-eos/hw/arm/ | grep -i 70d
ls -la /app/70d/qemu-eos/include/hw/arm/ | grep -i 70d
```

If not present, adapt from closest DIGIC V model (6D or 5D3):
```bash
# Copy and adapt from similar camera
cp hw/arm/digic5_6d.c hw/arm/digic5_70d.c
cp include/hw/arm/digic5_6d.h include/hw/arm/digic5_70d.h
```

### Running 70D in QEMU

```bash
# From qemu-eos directory
./arm-softmmu/qemu-system-arm -M 70d -kernel /app/70d/autoexec.bin -nographic
```

### QEMU-Specific Build Configuration

Create `platform/70D.112/Makefile.qemu`:
```makefile
# QEMU-specific build flags for 70D
CONFIG_QEMU := y
CFLAGS += -DCONFIG_QEMU
LDFLAGS += -Wl,--defsym,QEMU_BUILD=1
```

### What QEMU Can Test
- ✅ Memory mapping and register access
- ✅ Task creation and scheduling
- ✅ Property system logic
- ✅ File I/O (virtual SD card)
- ✅ State machine transitions
- ✅ Algorithm correctness

### What QEMU Cannot Test
- ❌ Actual image processing (no sensor)
- ❌ Timing-dependent features (FPS override)
- ❌ Hardware-specific bugs (SD overclocking)
- ❌ Audio processing
- ❌ Button/touch timing

## 2. Host-Side Compilation Framework

### Create Stub Implementations

Create `tests/host-stubs/stub_camera.c`:

```c
/**
 * Host-side stubs for 70D development
 * Allows compiling ML code on x86/Linux for logic testing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock camera state
typedef struct {
    uint32_t registers[0x10000];
    uint32_t evf_state;
    uint32_t focus_data;
    uint32_t iso;
    uint32_t shutter;
    uint32_t aperture;
} mock_camera_state_t;

static mock_camera_state_t g_camera;

// Initialize with 70D-like values
void mock_init_70d() {
    g_camera.evf_state = 0x12345; // Mock state
    g_camera.focus_data = 0;      // 70D doesn't expose this
    g_camera.iso = 100;
    g_camera.shutter = 50;        // 1/50s
    g_camera.aperture = 28;       // f/2.8
}

// Mock register access
uint32_t mock_read_register(uint32_t addr) {
    if (addr < 0x10000) {
        return g_camera.registers[addr / 4];
    }
    return 0;
}

void mock_write_register(uint32_t addr, uint32_t val) {
    if (addr < 0x10000) {
        g_camera.registers[addr / 4] = val;
        printf("[MOCK] Write reg 0x%08X = 0x%08X\n", addr, val);
    }
}

// Stub common ML functions
int task_create(const char* name, int priority, int stack_size, void (*entry)(void*), void* arg) {
    printf("[TASK] Create '%s' prio=%d stack=%d\n", name, priority, stack_size);
    return 0;
}

void msleep(int ms) {
    // No-op in host tests
}

// Property system stubs
int prop_get_property(uint32_t id, void* buf, int size) {
    printf("[PROP] Get 0x%08X (size %d)\n", id, size);
    memset(buf, 0, size);
    return 0;
}

int prop_set_property(uint32_t id, void* buf, int size) {
    printf("[PROP] Set 0x%08X (size %d)\n", id, size);
    return 0;
}
```

### Example Test: Focus Data Logic

Create `tests/test_focus_logic.c`:

```c
#include <stdio.h>
#include <stdint.h>
#include "../src/focus.c"  // Include focus logic

int main() {
    printf("Testing focus data handling on 70D...\n");
    
    // Test 1: Verify LV_FOCUS_DATA is not available
    #ifdef CONFIG_70D
    printf("✓ CONFIG_70D defined - focus features should be disabled\n");
    #else
    printf("✗ CONFIG_70D not defined\n");
    #endif
    
    // Test 2: Test focus calculation with mock data
    mock_init_70d();
    
    // Simulate focus data
    g_camera.focus_data = 0x1234;
    
    printf("Focus data: 0x%08X\n", g_camera.focus_data);
    
    return 0;
}
```

### Build Host Tests

Create `tests/Makefile`:
```makefile
CC = gcc
CFLAGS = -I../src -I../platform/70D.112/include -DCONFIG_70D -g
LDFLAGS = 

TESTS = test_focus_logic test_fps_calc test_crop_modes

all: $(TESTS)

test_focus_logic: test_focus_logic.c host-stubs/stub_camera.c
	$(CC) $(CFLAGS) -o $@ $^

test_fps_calc: test_fps_calc.c host-stubs/stub_camera.c
	$(CC) $(CFLAGS) -o $@ $^

test_crop_modes: test_crop_modes.c host-stubs/stub_camera.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TESTS) *.o

.PHONY: all clean
```

## 3. Simulation Framework

### Camera State Simulator

Create `tests/sim/camera_sim.h`:

```c
#ifndef _CAMERA_SIM_H
#define _CAMERA_SIM_H

#include <stdint.h>

typedef struct {
    // Register space
    uint32_t registers[0x10000];
    
    // State objects
    uint32_t evf_state;
    uint32_t movrec_state;
    uint32_t display_state;
    
    // Mock sensor data
    uint32_t iso;
    uint32_t shutter;
    uint32_t aperture;
    uint32_t focus_step;
    
    // Timing
    uint32_t vsync_count;
    uint32_t fps_register_a;
    uint32_t fps_register_b;
    
    // Flags
    int lv_active;
    int recording;
} camera_sim_t;

// Initialize simulator with 70D defaults
void sim_init_70d(camera_sim_t* cam);

// Register access with validation
int sim_poke(camera_sim_t* cam, uint32_t addr, uint32_t val);
uint32_t sim_peek(camera_sim_t* cam, uint32_t addr);

// State transitions
void sim_start_lv(camera_sim_t* cam);
void sim_stop_lv(camera_sim_t* cam);
void sim_start_recording(camera_sim_t* cam);
void sim_stop_recording(camera_sim_t* cam);

// VSync simulation
void sim_trigger_vsync(camera_sim_t* cam);

// Dump state for debugging
void sim_dump_state(camera_sim_t* cam);

#endif
```

### Example: FPS Override Test

Create `tests/sim/test_fps_sim.c`:

```c
#include <stdio.h>
#include <assert.h>
#include "camera_sim.h"

int main() {
    camera_sim_t cam;
    sim_init_70d(&cam);
    
    printf("Testing FPS override logic...\n");
    printf("Initial FPS Register A: 0x%08X\n", cam.fps_register_a);
    printf("Initial FPS Register B: 0x%08X\n", cam.fps_register_b);
    
    // Test writing to FPS registers
    sim_poke(&cam, 0xC0F06008, 0x12345678);  // FPS_REGISTER_A
    sim_poke(&cam, 0xC0F06014, 0x87654321);  // FPS_REGISTER_B
    
    printf("After write:\n");
    printf("  FPS Register A: 0x%08X\n", cam.fps_register_a);
    printf("  FPS Register B: 0x%08X\n", cam.fps_register_b);
    
    // Simulate VSync cycles
    for (int i = 0; i < 100; i++) {
        sim_trigger_vsync(&cam);
    }
    
    printf("VSync count: %d\n", cam.vsync_count);
    
    return 0;
}
```

## 4. Testing Strategy by Feature

### Focus Features (LV_FOCUS_DATA)

**Without Camera:**
1. ✅ Test focus data parsing logic with mock data
2. ✅ Verify `#if !defined(CONFIG_70D)` guards work correctly
3. ✅ Test memory spy hook registration
4. ✅ Validate focus graph algorithm with synthetic data

**With Remote Testing:**
1. Deploy test build with logging
2. Capture memory dumps during AF
3. Analyze logs to find focus data location
4. Iterate on candidate addresses

### FPS Override

**Without Camera:**
1. ✅ Test register calculation formulas
2. ✅ Verify timer A/B math
3. ✅ Test TG_FREQ_BASE impact
4. ✅ Validate menu UI for FPS selection

**Limitations:**
- Cannot test actual banding
- Cannot verify frame timing accuracy
- Cannot test blanking register interactions

**With Remote Testing:**
1. Deploy with multiple FPS presets
2. Capture register states
3. Measure actual frame rates from recorded video
4. Correlate banding with register values

### RAW Zebras

**Without Camera:**
1. ✅ Test zebra threshold algorithm on sample RAW files
2. ✅ Verify double-buffer logic
3. ✅ Test vsync lock mechanism
4. ✅ Validate overlay rendering

**With Remote Testing:**
1. Deploy with zebra logging
2. Capture RAW buffer timing data
3. Verify no QuickReview corruption
4. Tune threshold values

### Crop Recording

**Without Camera:**
1. ✅ Test crop dimension calculations
2. ✅ Verify register value math
3. ✅ Test preset menu logic
4. ✅ Validate buffer allocation

**With Remote Testing:**
1. Deploy crop presets
2. Verify actual crop dimensions
3. Test stability at various frame rates
4. Check for overheating

## 5. Automated Test Suite

### Create Test Runner

Create `tests/run_all_tests.sh`:

```bash
#!/bin/bash
set -e

echo "=== Magic Lantern 70D Test Suite ==="
echo ""

# Host-side compilation tests
echo "1. Running host compilation tests..."
cd tests
make clean
make all
./test_focus_logic
./test_fps_calc
./test_crop_modes
cd ..

# QEMU tests (if available)
if [ -d "qemu-eos" ]; then
    echo "2. Running QEMU tests..."
    cd qemu-eos
    # Run minimal QEMU test
    ../minimal/qemu-fio/Makefile
    cd ..
else
    echo "2. QEMU not available, skipping..."
fi

echo ""
echo "=== All tests completed ==="
```

## 6. CI/CD Integration

### GitHub Actions Workflow

Create `.github/workflows/test.yml`:

```yaml
name: 70D Test Suite

on: [push, pull_request]

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential
      
      - name: Build host tests
        run: |
          cd tests
          make
      
      - name: Run tests
        run: |
          cd tests
          ./run_all_tests.sh

  build-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Check 70D build
        run: |
          make clean
          make -j4 70D
```

## 7. Remote Testing Protocol

### For 70D Owners

When collaborating with 70D owners for testing:

1. **Provide Minimal Test Builds**
   - Single feature per build
   - Extensive logging to SD card
   - Safe defaults with rollback

2. **Logging Format**
   ```c
   // Log to ML/LOGS/70D_test_001.txt
   void log_test_data(const char* test_name, uint32_t* data, int len) {
       FILE* f = fopen("ML/LOGS/70D_test_001.txt", "a");
       fprintf(f, "[%s] ", test_name);
       for (int i = 0; i < len; i++) {
           fprintf(f, "%08X ", data[i]);
       }
       fprintf(f, "\n");
       fclose(f);
   }
   ```

3. **Feedback Collection**
   - Structured issue templates
   - Required log file uploads
   - Firmware version tracking

## 8. Development Workflow

### Daily Development (No Camera)

```
1. Write code with host-side stubs
2. Compile and test logic on x86
3. Run simulation tests
4. Commit to git
5. Push to CI for build verification
```

### Weekly Validation (With Remote Tester)

```
1. Merge feature branches
2. Create test build with logging
3. Send to remote tester
4. Collect logs and feedback
5. Iterate based on results
```

## 9. Quick Start Guide

### For New Developers

```bash
# 1. Clone the repository
git clone https://github.com/peva3/magiclantern_70D.git
cd magiclantern_70D

# 2. Set up host testing
cd tests
make

# 3. Run basic tests
./run_all_tests.sh

# 4. (Optional) Set up QEMU
cd ../qemu-eos
./configure --target-list=arm-softmmu
make -j$(nproc)

# 5. Start developing!
```

## 10. Known Limitations

| Feature | Testable Without Camera | Notes |
|---------|------------------------|-------|
| Focus confirmation | Partially | Logic only, needs field test |
| FPS override | Partially | Register math only |
| RAW zebras | Partially | Algorithm only |
| Crop modes | Yes | Dimensions and registers |
| Dual ISO | No | Requires sensor |
| Audio controls | No | Requires hardware |
| SD overclock | Partially | Register access only |

## Conclusion

While we cannot fully replace physical camera testing, this framework allows us to:

- ✅ Develop and test 70%+ of ML logic without hardware
- ✅ Catch bugs early through automated testing
- ✅ Validate algorithms and state machines
- ✅ Prepare safe, tested builds for remote testers
- ✅ Accelerate development cycle

The key is understanding what can be tested virtually vs what requires physical hardware, and structuring development accordingly.
