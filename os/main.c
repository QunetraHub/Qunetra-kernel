//	#define QUNETRA_MEMORY
	#define QUNETRA_STRING
	#define QUNETRA_MOUSE
//	#define QUNETRA_TIMER
//	#define QUNETRA_BITS
//	#define QUNETRA_MATH
	#define QUNETRA_CLI
//	#define QUNETRA_CPU
//	#define QUNETRA_LFB
//	#define QUNETRA_NET
//	#define QUNETRA_FS
#include "qunetra.h" // IWYU pragma: keep

static state mouseLog = false;
static void cmdMouse(const int argc, char** argv) { // mouse | mouse log
	if (argc > 1 && strIs(argv[1], "log")) {
		mouseLog = !mouseLog;
		kprintf("mouse log %s\n", mouseLog ? "on" : "off");
		return;
	}
	int32 x, y;
	mouseGetPosition(&x, &y);
	kprintf("position %d,%d buttons %d wheel %s\n", x, y, mouseButtons(), mouseHasWheel() ? "yes" : "no");
}
// Calls functions before the loop
void qunetraStart() {
	addQunetraBaseCommands();
	addCmd("mouse", "", cmdMouse);
	kprintf("Hello, World!\n");
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
	mouseEvent e;
	while (mouseGet(&e)) if (mouseLog) kprintf("dx=%d dy=%d wheel=%d buttons=%d\n", e.dx, e.dy, e.wheel, e.buttons);
}
