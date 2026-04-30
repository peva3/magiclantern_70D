#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <stdio.h>
#include <string.h>
#include <config.h>
#include <ml_socket.h>

static unsigned int wifi_discovery_init(void);

MODULE_INFO_START()
MODULE_INIT(wifi_discovery_init)
MODULE_INFO_END()

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

/* 70D RAM-loaded socket function addresses (loaded from firmware module space at boot) */
/* Called by firmware BL instructions at runtime - verified by capstone disassembly     */
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

/* Runtime function pointers (RAM-loaded, verified at runtime) */
static socket_create_fn     p_socket_create     = NULL;
static socket_bind_fn       p_socket_bind       = NULL;
static socket_connect_fn    p_socket_connect    = NULL;
static socket_listen_fn     p_socket_listen     = NULL;
static socket_setsockopt_fn p_socket_setsockopt = NULL;
static socket_recv_fn       p_socket_recv       = NULL;
static socket_send_fn       p_socket_send       = NULL;

/* ROM1 NSTUB stubs (always available) */
extern int socket_close_caller(int socket);
extern int socket_close_if_valid(int socket);

/* PTPIP ROM1-safe wrapper functions */
extern int ptpip_sock_create(void);
extern int ptpip_open_server(void);
extern int ptpip_create_client(void);
extern int ptpip_listen_close(int fd);
extern int ptpip_close_server(int fd);
extern int ptpip_set_keepalive(int fd);
extern int ptpip_bind_param(void);
extern int ptpip_sock_accept(int server_fd);

static unsigned short htons_ml(unsigned short port)
{
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

static int verify_code_addr(uint32_t addr)
{
    if (addr < 0x1000 || addr >= 0xFFFF0000) return 0;
    volatile uint32_t *p = (uint32_t *)addr;
    uint32_t word = *p;
    if ((word & 0x0FFF0000) == 0x092D0000) return 1;
    return 0;
}

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

/* --- Section 1: call() based WiFi init --- */

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

/* --- Section 2: RAM-loaded socket API test --- */

static void test_socket_api(void)
{
    printf("\n=== Socket API Test (RAM-loaded 0x0005xxxx) ===\n");

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

/* --- Section 3: PTPIP ROM1-safe wrappers --- */

static void test_ptpip_wrappers(void)
{
    printf("\n=== PTPIP Wrapper Test (ROM1-safe 0xFF7Axxxx) ===\n");

    printf("[PTPIP] ptpip_sock_create()... ");
    int result = ptpip_sock_create();
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_open_server()... ");
    result = ptpip_open_server();
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_create_client()... ");
    result = ptpip_create_client();
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_set_keepalive(1)... ");
    result = ptpip_set_keepalive(1);
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_listen_close(-1)... ");
    result = ptpip_listen_close(-1);
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_close_server(-1)... ");
    result = ptpip_close_server(-1);
    printf("returned: %d\n", result);

    printf("[PTPIP] ptpip_sock_accept(-1)... ");
    result = ptpip_sock_accept(-1);
    printf("returned: %d\n", result);

    printf("[PTPIP] socket_close_if_valid(-1) (should noop)... ");
    result = socket_close_if_valid(-1);
    printf("returned: %d\n", result);

    printf("[PTPIP] socket_close_if_valid(999) (invalid fd)... ");
    result = socket_close_if_valid(999);
    printf("returned: %d\n", result);
}

/* --- Section 4: NW command interface test --- */

static void test_nw_commands(void)
{
    printf("\n=== NW Command Interface Test ===\n");
    const char *nw_funcs[] = {
        "nif_up",
        "nif_start",
        "dhcpc_setup",
        "dnsc_setup",
        "ipset",
        NULL
    };
    int success = 0;
    for (int i = 0; nw_funcs[i]; i++) {
        int r = call_wifi_init(nw_funcs[i]);
        if (r >= 0) success++;
    }
    printf("[NW] %d/%d commands resolved\n", success, 4);
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
    test_ptpip_wrappers();
    test_nw_commands();

    printf("\n=== Summary ===\n");
    printf("[WiFi] call() init:        %d/6 names resolved\n", init_count);
    printf("[WiFi] Socket ptrs:        %d/7 verified\n", verified);
    printf("[WiFi] socket_close:       NSTUB(0xFF14F74C) in ROM1\n");
    printf("[WiFi] socket_close_valid: NSTUB(0xFF7AF380) in ROM1\n");
    printf("[PTPIP] Wrappers:          8 NSTUBs at 0xFF7AEE00-0xFF7AF500\n");

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
