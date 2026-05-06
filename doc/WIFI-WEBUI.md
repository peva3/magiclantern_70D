# WiFi Web UI: Camera Control via Browser

## Design, Constraints & Architecture for DIGIC V Cameras

**Last Updated:** 2026-05-06
**Target Platform:** Canon 70D (first implementation), then EOS M (tethering via USB/PTP tunnel)

---

## 1. The Core Idea

The 70D's built-in WiFi runs a complete TCP/IP stack initialized at boot. Socket APIs are loaded in RAM and validated on hardware. This means we can build a **web-based camera control interface** — no app install, no proprietary protocol, just a browser.

The camera runs an HTTP server. Users connect their phone/laptop to the camera's WiFi network and open a web page. Full camera control from any device.

**What sets this apart from Canon's stock WiFi:** Canon's EOS Utility requires a desktop app. Canon's Camera Connect app requires installation. Our approach works from any browser — phone, tablet, laptop, Chromebook, smart TV — with zero setup.

---

## 2. UX Flow (End to End)

### Setup
1. User navigates ML menu: **WiFi → Web UI → Enable**
2. Camera initializes WiFi as an access point (SSID: `EOS_70D_ML` or custom)
3. Status shows: *"WiFi active. Connect to 'EOS_70D_ML' then visit http://192.168.1.1"*
4. User connects phone/laptop to that network, opens browser, types the address

### Main Screen (Web App)
The web app is a single-page responsive design. Layout adapts to phone portrait or laptop landscape.

```
┌─────────────────────────────────────────┐
│ [⚡ 80%] [📶 2 bars] [💾 32GB free]     │  ← Status bar
├─────────────────────┬───────────────────┤
│                     │   Camera Controls  │
│   ╔═══════════════╗ │   ┌─────────────┐  │
│   ║   Live View   ║ │   │ ISO  800    │  │
│   ║   (3-5 fps)   ║ │   │ SS   1/125  │  │
│   ║               ║ │   │ F/2.8       │  │
│   ║   [Tap to AF] ║ │   │ WB  Daylight│  │
│   ╚═══════════════╝ │   └─────────────┘  │
│                     │                     │
│  ┌────────────────┐ │   ┌─────────────┐  │
│  │ Focus │═══●═══│ │   │  ╔═══════╗   │  │
│  │ Pull  │ Near  │ │   │  ║ SHUT ║   │  │
│  └────────────────┘ │   │  ╚═══════╝   │  │
│                     │   │  [Hold=AF]   │  │
├─────────────────────┴───────────────────┤
│ [📷 IMG_0001.JPG] [📷 IMG_0002.JPG] ...│  ← Gallery strip
│ [   Thumbnail   ] [   Thumbnail   ]     │
└─────────────────────────────────────────┘
```

### Key Interactions
- **Live View**: Updates every 200-300ms. Tap to set AF point. Pinch to zoom preview.
- **Shutter Button**: Tap to shoot. Half-press (touch hold) triggers AF, full release fires.
- **Focus Pull**: Slider for remote focus pulling during video. Uses AfCtrl remote functions.
- **Gallery**: Scrollable thumbnails. Tap to full view. Download button saves to device. Delete button removes from card.
- **Settings**: Tap any value (ISO, SS, aperture) to cycle or type a value.
- **Mode**: Photo / Video / Bulb / Timelapse selector.
- **Burst**: Hold shutter for continuous drive mode.

---

## 3. Technical Architecture

### Camera Side: HTTP Server Module

**Module name:** `webui` (max 8 chars for FAT 8.3)
**Location:** `modules/webui/webui.c`
**Size target:** ~15KB compiled

```
┌──────────────────────────────────────────────────┐
│                  webui module                    │
│                                                  │
│  WiFi Init ──> socket_create ──> socket_bind     │
│                    │              socket_listen   │
│                    │              socket_accept   │
│                    │              socket_recv     │
│                    │              socket_send     │
│                    ▼                              │
│  ┌──────────────────────────────────────────┐    │
│  │         HTTP Request Router              │    │
│  │  ┌────────┐  ┌────────┐  ┌───────────┐  │    │
│  │  │ Static │  │  API   │  │ WebSocket │  │    │
│  │  │ Files  │  │  JSON  │  │   (LV)    │  │    │
│  │  └────────┘  └────────┘  └───────────┘  │    │
│  └──────────────────────────────────────────┘    │
│                                                  │
│  Static files served from ML/WWW/                │
│  API: /api/shoot, /api/status, /api/af, ...       │
│  WS: LiveView frame push (~3-5 fps)              │
└──────────────────────────────────────────────────┘
```

**Thread model:** Single accept loop with non-blocking I/O. One connection at a time for simplicity. Keep-alive for LV streaming.

### API Endpoints

| Endpoint | Method | Response | Description |
|----------|--------|----------|-------------|
| `/` | GET | `index.html` | Main SPA |
| `/api/status` | GET | JSON | Camera state (ISO, SS, aperture, WB, battery, card, mode, recording) |
| `/api/shoot` | POST | JSON | Trigger shutter. Body: `{"af": true}` for AF+shoot, `{"af": false}` for shoot only |
| `/api/af` | POST | JSON | Trigger AF only. Body: `{"x": 0.5, "y": 0.5}` for AF point (normalized) |
| `/api/lv.jpg` | GET | JPEG | LiveView snapshot (current frame) |
| `/api/gallery` | GET | JSON | List of photos: `[{name, size, date, thumb_url}, ...]` |
| `/api/thumb?f=IMG_0001.JPG` | GET | JPEG | Thumbnail (320x240) |
| `/api/photo?f=IMG_0001.JPG` | GET | JPEG/CR2 | Full-res file download |
| `/api/set` | POST | JSON | Change setting. Body: `{"iso": 400, "ss": "1/125", "aperture": 2.8, "wb": "daylight"}` |
| `/api/focus` | POST | JSON | Focus pull. Body: `{"position": 0.5}` (0=near, 1=far) |
| `/api/mode` | POST | JSON | Change mode. Body: `{"mode": "photo"}` or `"video"`, `"bulb"`, `"timelapse"` |
| `/api/delete?f=IMG_0001.JPG` | DELETE | JSON | Delete file from card |
| `/api/format` | POST | JSON | Format card (with confirmation double-tap) |

### Web Frontend

**Stack:** Vanilla HTML + CSS + JavaScript. No frameworks.
**Location:** `ML/WWW/` on SD card.

**Files:**
```
ML/WWW/index.html       — Main app (single page)
ML/WWW/style.css         — Responsive styling
ML/WWW/app.js            — All JavaScript logic
ML/WWW/lv.jpg (dynamic)  — LiveView frame
```

**Key JS behaviors:**
- Polls `/api/status` every 1 second for state updates
- Fetches `/api/lv.jpg` every 200-300ms for LiveView
  - Uses `fetch()` with cache-busting timestamp
  - On mobile, lower rate to save battery (every 1s)
- Gallery fetched as JSON, thumbnails lazy-loaded
- AF point set by tapping LV canvas (sends normalized coords)
- Shutter via POST to `/api/shoot`
- Settings changes sent as JSON POST
- Graceful degradation: if WiFi drops, show "Reconnecting..." overlay

### LiveView Streaming Strategy

**Canvas-based approach:**
1. Camera captures LV YUV buffer (720x480, ~675KB)
2. Software JPEG encode down to ~30-50KB (takes ~100-200ms on ARM)
3. Serve via `/api/lv.jpg` with cache-control: no-cache
4. Client JS fetches new frame every 200-300ms using `fetch()` + `img.src = url + "?t=" + Date.now()`
5. On mobile: throttle to every 1-2 seconds to save battery

**Alternative if JPEG is too slow:** Serve raw YUV via WebSocket and render on canvas with JS. This skips JPEG encode but sends 675KB every frame. Only viable on local network (WiFi direct). JPEG is better for most cases.

**Maximum achievable framerate:**
- Capture LV buffer: <1ms (EDMAC already has it)
- JPEG encode 720x480: ~100-200ms on ARM
- WiFi send 50KB: ~5ms
- **Realistic: 3-5 fps** — adequate for composition and focus

---

## 4. Constraints & Limitations

### CPU
- DIGIC V ARM Cortex-A9 at ~500MHz
- JPEG encode is the bottleneck (~100-200ms per 720x480 frame)
- JSON API responses are microseconds
- File I/O (gallery listing) is milliseconds
- **Do not** attempt real-time H.264 transcoding — will overload the CPU

### RAM
- ML's usable heap: ~50MB (shared with all modules)
- HTTP connection buffer: 4-8KB per connection
- LV JPEG buffer: 50-100KB
- Gallery listing buffer: ~4KB
- **Plan for ~200KB total module footprint** — very manageable

### SD Card Bandwidth
- Sequential read: ~13MB/s baseline (no overclock)
- With sd_uhs at 160MHz: ~70MB/s
- Gallery thumbnail: 320x240 ~10KB — 1300+ thumbnails/second
- Full JPEG download: 5-10MB — <1 second per photo
- RAW download: 25-40MB — 2-3 seconds per RAW
- **Do not** serve large files while recording video (bus contention)

### WiFi Hardware
- 802.11b/g/n, 2.4GHz only
- Real-world throughput: ~3-5 MB/s (25-40 Mbps)
- Latency: ~1-5ms (direct connection, no router)
- Range: ~10-30 meters
- Camera acts as access point: devices connect directly
- Maximum recommended concurrent clients: 2-3

### Network
- Hardcoded IP: `192.168.1.1` (default gateway)
- No DHCP client — camera is the DHCP server for connected devices
- No internet access when connected to camera WiFi (phone maintains cellular)
- Camera has `wlan_settings` struct for SSID/password configuration
- Module reads `ML/SETTINGS/WIFI.CFG` for custom settings

### What WON'T Work
- Real-time H.264 streaming (IVA encoder is locked to recording path)
- Serving RAW files while recording 1080p video (SD bus contention)
- Multiple simultaneous heavy operations (single CPU, single SD card)
- Background LV during playback mode (no LV buffer)
- Internet access through camera WiFi (camera is not a gateway)

---

## 5. Comparison: yolo Module (200D) vs Our Approach

The existing `yolo` module (modules/yolo/) was a proof-of-concept for the 200D. Key differences:

| Aspect | yolo (200D) | Your webui (70D) |
|--------|-------------|-------------------|
| WiFi init | `call("NwLimeInit")`, `wlan_connect()` | Same pattern, validated on 70D |
| Connection mode | Infrastructure (connects to router) | Access point (devices connect to camera) |
| Target | External YOLO server processes images | Built-in camera control web app |
| Protocol | Custom binary protocol over TCP | Standard HTTP + REST API |
| Frontend | None | Single-page web app |
| LV stream | Raw YUV frames (require external decode) | JPEG frames (browser-decodable) |
| Configuration | `ML/SETTINGS/WIFI.CFG` (complex) | ML menu for SSID + channel |
| Port | Configurable | 80 (HTTP) |
| File serving | No | Full gallery browser + download |
| AF control | No | Via `/api/af` and `/api/focus` |

Our approach is more user-friendly (no config file editing, works from any browser) but needs more camera-side code for JPEG encoding and HTTP routing.

---

## 6. Phased Build Plan

### Phase 1: HTTP Server Foundation (Week 1)
- Create `modules/webui/` with Makefile
- Implement socket server (listen on port 80, accept connections)
- Parse HTTP GET requests (method, path, headers)
- Serve static files from `ML/WWW/`
- Serve `index.html` on `/`
- Return 404 for unknown paths
- **Test:** Connect to camera WiFi, browse to IP, see "Hello from ML" page

### Phase 2: Camera API (Week 1-2)
- `GET /api/status` — dump all camera state as JSON
- `POST /api/shoot` — trigger shutter, return success/fail
- `POST /api/af` — trigger autofocus
- `GET /api/gallery` — list files on card (JSON array)
- `GET /api/photo?f=...` — serve file download
- `GET /api/thumb?f=...` — serve EXIF thumbnail from CR2/JPEG
- `POST /api/set` — change ISO/aperture/shutter/WB
- `POST /api/mode` — switch photo/video/bulb
- **Test:** `curl` from laptop to verify each endpoint

### Phase 3: LiveView (Week 2-3)
- Capture LV YUV buffer from known address (0x5F227800)
- Software JPEG encode (port a tiny JPEG encoder or use Canon's)
- `GET /api/lv.jpg` — return current frame as JPEG
- Cache-busting via timestamp
- **Test:** Hit `/api/lv.jpg` repeatedly in browser, see view update

### Phase 4: Focus Pull (Week 2-3)
- `POST /api/focus` — use AfCtrl remote functions
- Map slider value (0.0-1.0) to focus distance
- Requires calling Canon's AF remote control functions
- **Test:** Place object at known distance, verify focus changes

### Phase 5: Web App UI (Week 2-3)
- Single-page HTML app with all controls
- Responsive layout (phone + tablet + laptop)
- LiveView canvas with tap-to-focus
- Shutter button with half-press AF
- Gallery with thumbnails and download
- Settings panel with all controls
- Error handling and reconnection
- **Test:** Full workflow on real camera

### Phase 6: Polish (Week 3-4)
- HTTPS? (Probably not worth it for direct WiFi)
- WiFi connection status indicator in ML menu
- Simultaneous connection handling
- Error recovery (WiFi drop, SD card full, etc.)
- Custom SSID via ML menu
- **Test:** Extended usage, edge cases

---

## 7. Feasibility Summary

| Component | Feasibility | Risk | Notes |
|-----------|-------------|------|-------|
| HTTP server | HIGH | Low | Basic socket API, 300 lines of C |
| Static file serving | HIGH | Low | Read file from SD, send over socket |
| JSON API | HIGH | Low | Standard JSON formatting |
| LV capture | HIGH | Low | Known buffer address + size |
| JPEG encode | MEDIUM | Medium | ~100-200ms per frame, need encoder code |
| Focus pull | MEDIUM | Medium | AfCtrl remote functions exist but exact API unknown |
| Shutter control | HIGH | Low | `schedule_remote_shot` confirmed at 0x0047db24 |
| Gallery browse | HIGH | Low | Standard FIO_* file operations |
| File download | HIGH | Low | Read file, send chunked over socket |
| Web UI | HIGH | Low | Standard HTML/JS, no camera-specific code |
| WiFi AP mode | MEDIUM | Medium | Need to get AP mode working (currently in infrastructure mode) |

**Overall feasibility: HIGH.** The foundational pieces (socket API, LV buffer, shutter control, file I/O) are all validated on hardware. The risk is in Phase 3 (JPEG encode performance) and Phase 4 (AfCtrl API details). Everything else is standard C programming.

---

## 8. Key Technical Decisions

### HTTP vs WebSocket for LiveView
**Decision: HTTP polling (fetch + img tag).** WebSocket adds complexity and requires JS to handle binary frame data. HTTP polling with cache-busting is simpler, works in any browser, and the 3-5fps rate doesn't need persistent connection overhead.

### Access Point vs Infrastructure Mode
**Decision: Start with Access Point.** Infrastructure mode (camera connects to existing WiFi) is more complex (requires SSID/password config, DHCP). AP mode is simpler: camera creates its own network, devices connect directly. Users switch WiFi networks on their phone to connect. Future: add infrastructure mode as an option.

### JPEG Encoder Choice
**Options:**
- Canon's internal JPEG encoder (call through eventproc) — fastest but may conflict with recording
- Tiny JPEG encoder (portable C, ~2KB) — slower but fully under our control
- Drop-in NanoJPEG or similar — medium complexity

**Recommendation:** Start with a minimal JPEG encoder (write a simple DCT+quantization+Huffman from first principles, or port a tiny one). This is pure software, no hardware dependencies. 100-200ms at 720x480 is acceptable for preview. If too slow, explore Canon's encoder.

### Embedded vs SD Card Web Files
**Decision: SD Card.** Embedding HTML/CSS/JS in the module binary would make it impossible for users to customize. Files on `ML/WWW/` can be edited freely. The module simply reads and serves them. Default files created on first module init if the directory doesn't exist.

---

## 9. Security Model

Since this is a direct WiFi connection with no internet:
- **No encryption needed** — physical proximity required to connect
- **No authentication** — anyone on the WiFi can control the camera
- **Add later:** simple password in ML menu → HTTP Basic Auth
- **No HTTPS** — self-signed certs add complexity for zero benefit on isolated network

Risk: If camera is in a public space, anyone within WiFi range could connect and operate it. Mitigation: disable WiFi by default, enable only when needed. Add password option in Phase 6.

---

## 10. What This Enables Long Term

Once the HTTP server and API are running, adding new features is just new endpoints:

- **Time-lapse controller** — set interval, duration, exposure ramping from browser
- **Astrophotography assistant** — live composite preview, star trail progress, bulb timer
- **Dual ISO verification** — upload test frames, see ISO map overlay
- **Focus stacking controller** — set step count, range, trigger automated sequence
- **Remote firmware update** — upload new autoexec.bin through browser
- **Log viewer** — browse ML/LOGS/ from browser
- **GPS relay** — phone sends GPS coordinates to camera via browser API
- **Multi-camera sync** — one camera acts as master, others as slaves via WiFi

Every feature we've documented in FUTURE-FEATURES.md that involves WiFi, remote control, or data transfer benefits from this foundation.

---

## 11. Files to Create/Modify

| File | Purpose |
|------|---------|
| `modules/webui/Makefile` | Build definition |
| `modules/webui/webui.c` | HTTP server + API handlers |
| `modules/webui/www/index.html` | Main web app |
| `modules/webui/www/style.css` | Responsive styling |
| `modules/webui/www/app.js` | Client-side logic |
| `platform/70D.112/modules.included` | Add `webui` (1 line) |
| `doc/WIFI-WEBUI.md` | This document |
