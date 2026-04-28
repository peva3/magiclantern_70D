/**
 * WiFi Discovery Module for Canon 70D
 *
 * This module discovers and tests WiFi/socket functions on the 70D.
 * Architecture: 70D (DIGIC V) loads socket library into RAM at 0x0005xxxx
 * (NOT in ROM1 like 200D). Only socket_close (0xFF14F74C) is in ROM1.
 *
 * Approach:
 * 1. call() for eventproc-resolved functions (NwLimeInit, etc.)
 * 2. Direct function pointers for RAM-loaded socket functions
 * 3. socket_close_caller via NSTUB (in ROM1)
 * 4. Runtime address verification before calling RAM functions
 */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <ml_socket.h>

/* Forward declaration needed before MODULE_INFO uses it */
static unsigned int wifi_discovery_init(void);

MODULE_INFO_START()
MODULE_INIT(wifi_discovery_init)
MODULE_INFO_END()

/* Network constants */
#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

/* 70D RAM-loaded socket function addresses (loaded from firmware module space) */
/* These are called by firmware BL instructions at runtime - validated by capstone */
#define SOCKET_CREATE_ADDR   0x00059AF8
#define SOCKET_BIND_ADDR     0x00059E94
#define SOCKET_CONNECT_ADDR  0x00059DDC
#define SOCKET_LISTEN_ADDR   0x0005A9D0
#define SOCKET_SETSOCKOPT    0x0005A810
#define SOCKET_RECV_ADDR     0x00059CE8
#define SOCKET_SEND_ADDR     0x0005A09C

/* Function pointer types for RAM-loaded socket library */
typedef int (*socket_create_fn)(int domain, int type, int protocol);
typedef int (*socket_bind_fn)(int fd, struct sockaddr_in *addr, int addrlen);
typedef int (*socket_connect_fn)(int fd, struct sockaddr_in *addr, int addrlen);
typedef int (*socket_listen_fn)(int fd, int backlog);
typedef int (*socket_setsockopt_fn)(int fd, int level, int optname, const void *optval, int optlen);
typedef int (*socket_recv_fn)(int fd, void *buf, int len, int flags);
typedef int (*socket_send_fn)(int fd, const void *buf, int len, int flags);

/* Runtime function pointers (initialized to NULL, set after verify) */
static socket_create_fn     p_socket_create     = NULL;
static socket_bind_fn       p_socket_bind       = NULL;
static socket_connect_fn    p_socket_connect    = NULL;
static socket_listen_fn     p_socket_listen     = NULL;
static socket_setsockopt_fn p_socket_setsockopt = NULL;
static socket_recv_fn       p_socket_recv       = NULL;
static socket_send_fn       p_socket_send       = NULL;
/* socket_close_caller is available via NSTUB (in ROM1 at 0xFF14F74C) */
extern int socket_close_caller(int socket);

/* WiFi management - NW command interface (discovered but not yet functional) */
/* wlan_connect, nif_setup, set_IP_address are NOT available as NSTUB on 70D */

/* call() resolves eventproc names at runtime - declared in dryos.h */

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

/* Verify an address contains valid ARM code (PUSH prologue) */
static int verify_code_addr(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    volatile uint32_t *p = (uint32_t *)addr;
    uint32_t word = *p;
    if ((word & 0x0FFF0000) == 0x092D0000) return 1;
    return 0;
}

/* Initialize function pointers with runtime address verification */
static int init_socket_ptrs(void)
{
    int ok = 0;
    p_socket_create = (socket_create_fn)SOCKET_CREATE_ADDR;
    if (verify_code_addr(SOCKET_CREATE_ADDR)) { ok++; }
    else { p_socket_create = NULL; printf("[WiFi] socket_create INVALID\n"); }

    p_socket_bind = (socket_bind_fn)SOCKET_BIND_ADDR;
    if (verify_code_addr(SOCKET_BIND_ADDR)) { ok++; }
    else { p_socket_bind = NULL; }

    p_socket_connect = (socket_connect_fn)SOCKET_CONNECT_ADDR;
    if (verify_code_addr(SOCKET_CONNECT_ADDR)) { ok++; }
    else { p_socket_connect = NULL; }

    p_socket_listen = (socket_listen_fn)SOCKET_LISTEN_ADDR;
    if (verify_code_addr(SOCKET_LISTEN_ADDR)) { ok++; }
    else { p_socket_listen = NULL; }

    p_socket_setsockopt = (socket_setsockopt_fn)SOCKET_SETSOCKOPT;
    if (verify_code_addr(SOCKET_SETSOCKOPT)) { ok++; }
    else { p_socket_setsockopt = NULL; }

    p_socket_recv = (socket_recv_fn)SOCKET_RECV_ADDR;
    if (verify_code_addr(SOCKET_RECV_ADDR)) { ok++; }
    else { p_socket_recv = NULL; }

    p_socket_send = (socket_send_fn)SOCKET_SEND_ADDR;
    if (verify_code_addr(SOCKET_SEND_ADDR)) { ok++; }
    else { p_socket_send = NULL; }

    return ok;
}

static void show_status(const char *msg, int ok)
{
    bmp_printf(FONT_MED, 50, 50, "WiFi: %s [%s]", msg, ok ? "OK" : "FAIL");
}

static int call_wifi_init(const char *func_name)
{
    printf("[WiFi] Calling '%s'... ", func_name);
    int result = call(func_name);
    printf("result=%d\n", result);
    return result;
}

static int try_wifi_sequence(void)
{
    const char *init_funcs[] = {
        "NwLimeInit",
        "NwLimeOn",
        "wlanpoweron",
        "wlanup",
        "wlanchk",
        "wlanipset",
        NULL
    };
    int success = 0;
    for (int i = 0; init_funcs[i]; i++) {
        int r = call_wifi_init(init_funcs[i]);
        if (r >= 0) success++;
    }
    return success;
}

static void test_socket_api(void)
{
    printf("\n=== Socket API Test ===\n");

    if (!p_socket_create) {
        printf("[WiFi] socket_create not available\n");
        return;
    }

    int sock = p_socket_create(1, 1, 0);
    printf("[WiFi] socket_create(1,1,0) returned: %d\n", sock);

    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = 1;
        addr.sin_port = htons_ml(5555);

        if (p_socket_bind) {
            int ret = p_socket_bind(sock, &addr, sizeof(addr));
            printf("[WiFi] socket_bind returned: %d\n", ret);
        }
        if (p_socket_listen) {
            int ret = p_socket_listen(sock, 1);
            printf("[WiFi] socket_listen returned: %d\n", ret);
        }
        if (p_socket_connect) {
            int ret = p_socket_connect(sock, &addr, sizeof(addr));
            printf("[WiFi] socket_connect returned: %d\n", ret);
        }
        int close_ret = socket_close_caller(sock);
        printf("[WiFi] socket_close(%d) = %d\n", sock, close_ret);
    }
}

static void wifi_discovery_task(void *unused)
{
    (void)unused;
    printf("\n========================================\n");
    printf("  WiFi Discovery Module - Canon 70D\n");
    printf("========================================\n\n");

    show_status("Starting", 0);

    printf("\n=== Address Verification ===\n");
    int verified = init_socket_ptrs();
    printf("[WiFi] %d/7 socket function addresses verified\n", verified);

    printf("\n=== WiFi Init Sequence (call() by name) ===\n");
    int init_count = try_wifi_sequence();

    test_socket_api();

    printf("\n=== Summary ===\n");
    printf("[WiFi] call() init: %d/6 names resolved\n", init_count);
    printf("[WiFi] Socket ptrs: %d/7 verified\n", verified);
    printf("[WiFi] socket_close_caller: NSTUB(0xFF14F74C) in ROM1\n");

    show_status("Complete", verified > 0 || init_count > 0);

    printf("========================================\n");
    printf("  WiFi Discovery Complete\n");
    printf("========================================\n");
}

static unsigned int wifi_discovery_init(void)
{
    printf("\n*** WiFi Discovery Module Loading ***\n");
    task_create("wifi_disc", 0x1e, 0x1000, wifi_discovery_task, 0);
    return 0;
}
