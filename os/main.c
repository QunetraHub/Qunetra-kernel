// #define QUNETRA_STRING
// #define QUNETRA_MEMORY
// #define QUNETRA_MATH
// #define QUNETRA_CPU
// #define QUNETRA_LFB
// #define QUNETRA_FS
#define QUNETRA_NET
#define QUNETRA_CLI
#include "qunetra.h" // IWYU pragma: keep

static void ping(const int argc, char** argv) {
	uint8 ip[4], ttl = 0;
	if (argc < 2 || !netParseIp(argv[1], ip)) { kprintf("usage: ping <a.b.c.d>\n"); return; }
	if (netPing(ip, &ttl)) kprintf("reply from %s, ttl=%d\n", argv[1], ttl);
	else kprintf("no reply\n");
}
static void dns(const int argc, char** argv) {
	uint8 ip[4];
	if (argc < 2) { kprintf("usage: dns <name>\n"); return; }
	if (!netDnsResolve(argv[1], ip)) { kprintf("not found\n"); return; }
	kprintf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
}
// Calls functions before the loop
void qunetraStart() {
	addQunetraBaseCommands();
	__aqc__(ping, "ping an IPv4 address");
	__aqc__(dns, "resolve a name");
	kprintf("Hello from " KERNEL_NAME " " KERNEL_VERSION_STR " !\n");
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
}
