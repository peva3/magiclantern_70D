# Canon 70D ROM Dump Guide

This guide covers how to dump ROM files from a Canon 70D (firmware 1.1.2) for use with QEMU emulation and firmware analysis.

## Why You Need ROM Dumps

The QEMU 70D emulation model requires three binary files from a real camera:

| File | Address | Size | Purpose |
|------|---------|------|---------|
| `ROM0.BIN` | `0xF0000000` | 16 MB | Main firmware ROM |
| `ROM1.BIN` | `0xF8000000` | 16 MB | Secondary firmware ROM |
| `SFDATA.BIN` | Serial flash | 8 MB | Serial flash data (EFI, settings) |

Place them in `/app/70d/roms/70D/` after dumping.

**You cannot extract ROM data from the FIR firmware update file** — it is encrypted with Canon-proprietary encryption. The only way to get ROM dumps is from a running camera.

---

## Method 1: Auto-Backup on First Boot (Easiest)

ML automatically dumps ROM on first boot when `CONFIG_AUTOBACKUP_ROM` is enabled (it is by default).

### Steps

1. Format an SD card (FAT32) in-camera
2. Install Magic Lantern on the 70D (see README.md for build instructions)
3. Insert the SD card and power on the camera
4. ML boots and runs `ml_backup` task in the background
5. Wait ~30 seconds for the background task to finish
6. Power off the camera
7. Remove the SD card and check `ML/LOGS/` on your computer

### Files Produced

| File | Size |
|------|------|
| `ML/LOGS/ROM1.BIN` | 16 MB |
| `ML/LOGS/ROM0.BIN` | 16 MB |

### Notes

- The backup is **one-shot** — if the files already exist with the correct size, ML skips the dump
- ROM1 is dumped before ROM0 (order in source code)
- Data is copied to RAM in 1 MB chunks before writing (ROM is slow and may interfere with LiveView)
- To re-dump, delete the existing files from the SD card first
- This method does **not** produce SFDATA.BIN — use Method 3 for that

---

## Method 2: Debug Menu "Dump ROM and RAM"

Use this if auto-backup didn't work or you want to also dump RAM.

### Steps

1. Boot the camera with ML enabled
2. Press the ML menu button (typically TRASH/DELETE)
3. Navigate to **Debug** menu
4. Select **"Dump ROM and RAM"**
5. Press SET to confirm
6. Wait for the dump to complete (screen shows progress: "Saving XXXXXXXX...")
7. Power off, remove SD card

### Files Produced

| File | Address | Size |
|------|---------|------|
| `ML/LOGS/ROM0.BIN` | `0xF0000000` | 16 MB |
| `ML/LOGS/ROM1.BIN` | `0xF8000000` | 16 MB |
| `ML/LOGS/RAM4.BIN` | `0x40000000`+ | 256 MB |

### Notes

- This method **always overwrites** existing files (unlike auto-backup which skips)
- RAM4.BIN is 256 MB — make sure your SD card has enough free space (~300 MB total)
- Writes directly from ROM (no RAM copy step) — may be slower than auto-backup
- 200 ms delay between ROM0 and ROM1 writes

---

## Method 3: Serial Flash Dump (sf_dump module)

Required to get `SFDATA.BIN` for QEMU. Neither Method 1 nor Method 2 produces this file.

### Steps

1. Boot the camera with ML enabled
2. Press the ML menu button
3. Navigate to **Debug** menu
4. Select **"Dump serial flash"**
5. Press SET to confirm
6. The console will show progress (percentage)
7. Wait for completion (~30 seconds)
8. Power off, remove SD card

### File Produced

| File | Size |
|------|------|
| `ML/LOGS/SFDATA.BIN` | ~16 MB (default; actual flash may be smaller, excess repeats) |

### Notes

- The 70D does not override `SF_flash_size`, so the default is 16 MB. The actual serial flash content may be only 8 MB — the remainder will be repeated/copy data
- Uses firmware's own `SF_readSerialFlash()` API at address `0xFF144CD4`
- Dumps in 64 KB chunks
- The module must be loaded (it is included in the default build for 70D)

---

## Method 4: ML Installer Backup

The ML installer also backs up ROM before performing installation.

### Steps

1. Build the ML installer: `make -C installer/70D.112`
2. Copy `ML-SETUP.FIR` to the root of your SD card
3. In the camera's firmware update menu, select the FIR file
4. The installer runs, backs up ROM, then installs ML
5. After restart, the screen says: "After restart, please copy ML/LOGS/ROM\*.BIN to PC, in a safe place."

### Files Produced

Same as Method 1: `ML/LOGS/ROM0.BIN` (16 MB) and `ML/LOGS/ROM1.BIN` (16 MB).

### Notes

- Also uses skip-if-exists logic
- Uses symbolic constants (same addresses as other methods)

---

## Method 5: LED Blink Dumper (Last Resort)

Use this only if you cannot write to the SD card (e.g., card slot is broken). This method transmits ROM data via the camera's LED, captured by a PC sound card.

### Prerequisites

- Audio cable connecting camera LED area to PC sound card line-in
- Audio recording software on PC
- Decoder tools: `/app/70d/tools/led_blink_dumper/`

### Setup

1. Edit `/app/70d/src/reboot-dumper.c` — uncomment the `led_dump()` call at line ~490 and set the correct 70D addresses:

```c
led_dump(0xF0000000, 0x01000000);  // ROM0 (16 MB)
led_dump(0xF8000000, 0x01000000);  // ROM1 (16 MB)
```

2. Build the dumper binary (minimal build)
3. Copy to SD card as the bootable autoexec
4. Connect audio cable to PC sound card
5. Start audio recording on PC (44.1 kHz or higher, 16-bit)
6. Power on camera — LED begins blinking ROM data
7. Record the entire dump (~30+ minutes for 32 MB total)
8. Stop recording, save as WAV

### Decoding

```bash
# Step 1: Convert audio to raw bitstream
cd /app/70d/tools/led_blink_dumper
gcc -o adc adc.c crc16.c -lm
gcc -o dec dec.c crc16.c

./adc recording.wav raw_dump

# Step 2: Parse blocks, validate CRC, write binary
./dec raw_dump ROM0.BIN
```

### Protocol Details

- **Sync header:** `0x0A 0x55 0xAA 0x50`
- **Block size:** 512 bytes
- **CRC-16:** polynomial `0x8005`
- **Bit encoding:** LSB first, LED on/off timing (bit 0 = short pulse, bit 1 = long pulse)
- **All-zero blocks:** encoded as header-only (type=1), no data transmitted
- **All-0xFF blocks:** skipped entirely (empty ROM)

### 70D LED Constants

| Constant | Value |
|----------|-------|
| `CARD_LED_ADDRESS` | `0xC022C06C` |
| `LEDON` | `0x138800` |
| `LEDOFF` | `0x838C00` |

### Warnings

- Very slow — expect 30+ minutes per 16 MB region
- Sensitive to audio quality — use a good cable and quiet environment
- The default `led_dump()` address in source (`0xFC000000`, 64 MB) does not match the 70D ROM layout — must edit before building
- The `guess_led()` function in reboot-dumper.c has no DIGIC V support block — do not use it on 70D

---

## Verifying ROM Dumps

After copying files to your PC, verify they are valid:

### File Sizes

| File | Expected Size | QEMU Expected Size |
|------|---------------|--------------------|
| `ROM0.BIN` | 16 MB (16,777,216 bytes) | 8 MB (8,388,608 bytes) |
| `ROM1.BIN` | 16 MB (16,777,216 bytes) | 16 MB (16,777,216 bytes) |
| `SFDATA.BIN` | 8-16 MB | 8 MB (8,388,608 bytes) |

**Note:** The camera dumps 16 MB for ROM0, but QEMU only needs the first 8 MB (`0x00800000`). The QEMU model will truncate ROM0 to 8 MB automatically. ROM1 matches at 16 MB.

### Content Checks

```bash
# ROM should NOT be all zeros (that's a placeholder)
xxd ROM0.BIN | head -5

# Check for ARM code signature (should see ARM instructions, not 00 00 00 00)
xxd ROM1.BIN | head -5

# Check entropy (should be high for compressed firmware, not ~0)
python3 -c "
import sys
data = open(sys.argv[1], 'rb').read(65536)
entropy = -sum((data.count(b)/len(data)) * (lambda p: p and __import__('math').log2(p))(data.count(b)/len(data)) for b in range(256))
print(f'Entropy: {entropy:.2f} bits/byte (expect 5-8 for real ROM, ~0 for placeholder)')
" ROM0.BIN
```

### Known Firmware Signature

The 70D firmware 1.1.2 has signature `0xd8698f05` (`SIG_70D_112` in `property.h`).

---

## Placing ROM Dumps for QEMU

```bash
# Copy files to the QEMU ROM directory
cp ML/LOGS/ROM0.BIN /app/70d/roms/70D/ROM0.BIN
cp ML/LOGS/ROM1.BIN /app/70d/roms/70D/ROM1.BIN
cp ML/LOGS/SFDATA.BIN /app/70d/roms/70D/SFDATA.BIN
```

Then test with QEMU:

```bash
cd /app/70d/qemu-eos
python3 magiclantern/scripts/run_qemu.py 70D -r /app/70d/roms
```

If QEMU loads without the "placeholder ROM" warning, your dumps are valid.

---

## Recommended Dump Procedure

For a complete set of ROM files, follow this order:

1. **First boot with ML** — auto-backup creates `ROM0.BIN` + `ROM1.BIN`
2. **Debug menu > Dump serial flash** — creates `SFDATA.BIN`
3. **Copy all three files** from `ML/LOGS/` to your PC immediately
4. **Verify** file sizes and content (not all zeros)
5. **Back up** to multiple locations — these files are essential for development
6. **Place in QEMU directory** for emulation testing

The debug menu "Dump ROM and RAM" can be used as a backup method if auto-backup fails, and also produces the optional `RAM4.BIN` (256 MB) for advanced firmware analysis.

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Auto-backup didn't create files | Check that `ML/LOGS/` directory exists on SD card; try Method 2 instead |
| ROM0.BIN is all zeros | You may have a placeholder file — delete it and re-dump |
| SFDATA.BIN is 16 MB instead of 8 MB | Normal — 70D doesn't set `SF_flash_size`, defaults to 16 MB. QEMU will use 8 MB. |
| QEMU still shows "placeholder ROM" warning | Verify the files in `/app/70d/roms/70D/` are the real dumps, not the zero-filled placeholders |
| SD card write errors | Try a different SD card; format in-camera before dumping |
| Camera freezes during dump | Wait — ROM access is slow. If truly hung, remove battery and retry with Method 2 (smaller scope) |
