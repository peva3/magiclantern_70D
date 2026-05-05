# 70D WiFi Initialization Sequence (RE Analysis)

## Key Functions and Addresses

### Eventproc-dispatchable (callable by name)

| Function | Runtime Address | Table |
|----------|----------------|-------|
| `InitializePTPFrameworkController` | `0xFF29CE68` | PTP (0xDDF93C) |
| `TerminatePTPFrameworkController` | `0xFF29CF2C` | PTP (0xDDF93C) |
| `OpenSession` | `0xFF29CD88` | PTP (0xDDF93C) |
| `CloseSession` | `0xFF29CDA0` | PTP (0xDDF93C) |
| `LughConnect` | `0xFF2ADD40` | Lugh (0xBD5CEC) |
| `LughDisconnect` | `0xFF2AE53C` | Lugh (0xBD5CEC) |
| `LughPowerOn` | `0xFF2AE518` | Lugh (0xBD5CEC) |
| `DisablePowerSave` | `0xFF14455C` | Power (0xDC70F8) |
| `EnablePowerSave` | `0xFF1445C8` | Power (0xDC70F8) |
| `PrepareCommunication` | `0xFF317A2C` | WiFi/Protocol (0xBD8C30) |
| `SetWiFiEnable` | `0xFF317A58` | WiFi/Protocol (0xBD8C30) |
| `InitializeResponderTransactionUSB` | `0xFF2A12A4` | PTP/USB transport (0xBD599C) |
| `InitializeResponderTransactionSDIO` | `0xFF2A2808` | PTP/SDIO transport (0xBD5A64) |

### RAM-loaded DryOS modules (0x000xxxxx range)

| Address | Context |
|---------|---------|
| `0x00006904` | Power/clock setup for WiFi (called by FUN_0x299924) |
| `0x00059AF8` | `socket_create` |
| `0x00059E94` | `socket_bind` |
| `0x00059DDC` | `socket_connect` |
| `0x0005A9D0` | `socket_listen` |
| `0x00059CE8` | `socket_recv` |
| `0x0005A09C` | `socket_send` |
| `0x0005A810` | `socket_setsockopt` |

## WiFi Connection Sequence (FUN at ROM1+0x299924)

```
1. BL -> 0x00006904 (RAM DryOS module: power/clock enable with params 0x33, 0x01)
2. BL -> FUN_0x29E4B4 (PTP framework sub-setup)
3. BL -> FUN_0x477B44 (SDIO driver init)
4. BL -> DisablePowerSave (0xFF14455C)
5. BL -> LughConnect (0xFF2ADD40)
6. (conditional branch for error handling)
7. BL -> 0x00006904 (RAM DryOS module: power/clock setup again)
8. BL -> FUN_0x293DA8 (post-connection setup)
9. BL -> DisablePowerSave (0xFF14455C)  -- disable power save again
10. BL -> FUN_0x2ADDEC (post-Lugh function)
11. BL -> EnablePowerSave (0xFF1445C8)  -- restore power save
```

## PTP Framework Init Sequence (InitializePTPFrameworkController at ROM1+0x29CE68)

Uses state variable at 0x00094420 (single-shot init):
```
1. LDR r0, =0x00094420  -- state variable
2. If state == 0, proceed with init
3. Set state = 1
4. BL -> FUN_0x29F5E4 (PTP internal init)
5. BL -> FUN_0x29E574 (PTP setup)
6. BL -> FUN_0x29DA58 (PTP setup)
7. BL -> FUN_0x275958 (SRM/memory setup)
8. BL -> FUN_0x29DD4C (conditional: event handler registration?)
9. BL -> FUN_0x29F6F4 (conditional: callback/text init)
10. BL -> FUN_0x478678 (PTP responder init)
11. BL -> FUN_0x478714 (PTP responder setup, r0=0xFF29CDD4 function ptr)
12. BL -> FUN_0x29A038 (registration with callback 0xFF29CDD4)
13. BL -> FUN_0x29A540 (registration with callback 0xFF29CDD0: BX lr)
14. BL -> FUN_0x274DAC
15. LDR r0, =0xFF29CDD0 (callback)
16. BL -> FUN_0x2A148C (USB responder transaction setup)
17. BL -> FUN_0x2A147C (USB responder transaction continuation)
```

## ML WiFi Enable Sequence (proposed)

Based on analysis, this should work from ML:

```c
// 1. Disable power save (enable WiFi power)
call("DisablePowerSave");

// 2. Initialize PTP framework (single-shot, safe to call multiple times)
call("InitializePTPFrameworkController");

// 3. Power on Lugh (WiFi management layer)
call("LughPowerOn");

// 4. Connect Lugh (actually establishes WiFi connection)
call("LughConnect");

// 5. Open PTP session over WiFi
call("OpenSession");

// 6. Re-enable power save
call("EnablePowerSave");
```

Note: `WLANSDIODRV_InitializeSDIODriver` is NOT a name-based eventproc. It's likely called automatically 
by InitializePTPFrameworkController or initialized at boot time. If not, it may need to be called 
via direct function pointer or it's referenced differently.

## Key Finding: RAM-loaded Modules

The FUN_0x299924 call to 0x00006904 wraps to a DryOS RAM module (32-bit address overflow from 
0xFF29994C + 0xD6CFB8 = 0x00006904). This confirms the WiFi connection code uses a RAM-loaded 
module, consistent with the socket API functions also being RAM-loaded (0x00059xxx range). 
The 0x000xxxxx range is DryOS module space.
