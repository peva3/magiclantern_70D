/**
 * WiFi Discovery Module for Canon 70D
 *
 * This module attempts to initialize WiFi on the 70D and discover
 * available function addresses. It uses:
 * 1. call() for functions resolved by name
 * 2. WEAK_FUNC stubs for socket functions (fallback if stubs missing)
 * 3. Diagnostic output to identify missing addresses
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

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

/* ============================================================
 * Weak stubs for socket functions (one stub per unique signature)
 * Each must match the exact type of the extern declaration for alias to work
 * ============================================================ */
static int stub_socket_create(int a, int b, int c) { (void)a; (void)b; (void)c; return -1; }
static int stub_socket_bind(int s, struct sockaddr_in *a, int l) { (void)s; (void)a; (void)l; return -1; }
static int stub_socket_connect(int s, struct sockaddr_in *a, int l) { (void)s; (void)a; (void)l; return -1; }
static int stub_socket_listen(int s, int b) { (void)s; (void)b; return -1; }
static int stub_socket_accept(int s, void *a, int l) { (void)s; (void)a; (void)l; return -1; }
static int stub_socket_recv(int s, void *b, int l, int f) { (void)s; (void)b; (void)l; (void)f; return -1; }
static int stub_socket_send(int s, void *b, int l, int f) { (void)s; (void)b; (void)l; (void)f; return -1; }
static int stub_socket_close(int s) { (void)s; return -1; }
static int stub_convertfd(int s) { (void)s; return -1; }
static int stub_wlan_connect(struct wlan_settings *s) { (void)s; return -1; }
static int stub_nif_setup(int i) { (void)i; return -1; }
static int stub_set_ip(int a, uint32_t b, uint32_t c, uint32_t d) { (void)a; (void)b; (void)c; (void)d; return -1; }

/* WEAK_FUNC - if real symbol exists in firmware, use it; otherwise use stub */
extern WEAK_FUNC(stub_socket_create) int socket_create(int domain, int type, int protocol);
extern WEAK_FUNC(stub_socket_bind) int socket_bind(int socket, struct sockaddr_in *addr, int addr_len);
extern WEAK_FUNC(stub_socket_connect) int socket_connect(int socket, struct sockaddr_in *addr, int addr_len);
extern WEAK_FUNC(stub_socket_listen) int socket_listen(int socket, int backlog);
extern WEAK_FUNC(stub_socket_accept) int socket_accept(int socket, void *addr, int addr_len);
extern WEAK_FUNC(stub_socket_recv) int socket_recv(int socket, void *buf, int len, int flags);
extern WEAK_FUNC(stub_socket_send) int socket_send(int socket, void *buf, int len, int flags);
extern WEAK_FUNC(stub_socket_close) int socket_close_caller(int socket);
extern WEAK_FUNC(stub_convertfd) int socket_convertfd(int socket);
extern WEAK_FUNC(stub_wlan_connect) int wlan_connect(struct wlan_settings *settings);
extern WEAK_FUNC(stub_nif_setup) int nif_setup(int interface);
extern WEAK_FUNC(stub_set_ip) int set_IP_address(int interface, uint32_t client_IP, uint32_t subnet_mask, uint32_t gateway_IP);

/* call() resolves functions by name at runtime - declared in dryos.h */

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

    int sock = socket_create(SOCK_FAMILY_IPv4, SOCK_STREAM, 0);
    printf("[WiFi] socket_create returned: %d\n", sock);

    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = SOCK_FAMILY_IPv4;
        addr.sin_port = htons_ml(5555);

        int ret = socket_bind(sock, &addr, sizeof(addr));
        printf("[WiFi] socket_bind returned: %d\n", ret);

        ret = socket_listen(sock, 1);
        printf("[WiFi] socket_listen returned: %d\n", ret);

        ret = socket_connect(sock, &addr, sizeof(addr));
        printf("[WiFi] socket_connect returned: %d\n", ret);
    }
}

static void wifi_discovery_task(void *unused)
{
    (void)unused;
    printf("\n========================================\n");
    printf("  WiFi Discovery Module - Canon 70D\n");
    printf("========================================\n\n");

    show_status("Starting", 0);

    printf("\n=== WiFi Init Sequence ===\n");
    int init_count = try_wifi_sequence();

    printf("\n=== WLAN Connect Test ===\n");
    int ret = wlan_connect(NULL);
    printf("[WiFi] wlan_connect(NULL) = %d\n", ret);

    ret = nif_setup(0);
    printf("[WiFi] nif_setup(0) = %d\n", ret);

    ret = set_IP_address(0, 0, 0, 0);
    printf("[WiFi] set_IP_address(0,0,0,0) = %d\n", ret);

    test_socket_api();

    printf("\n=== Summary ===\n");
    if (init_count > 0) {
        printf("WiFi: %d/6 init calls succeeded\n", init_count);
    } else {
        printf("WiFi: Init not available (stubs needed)\n");
    }
    printf("\nTo add stubs, find addresses in ROM1.BIN:\n");
    printf("  platform/70D.112/stubs.S:\n");
    printf("  NSTUB(0xFFFFFFFF, socket_create)\n");
    printf("  NSTUB(0xFFFFFFFF, socket_bind)\n");
    printf("  NSTUB(0xFFFFFFFF, wlan_connect)\n\n");

    show_status("Complete", init_count > 0);

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
