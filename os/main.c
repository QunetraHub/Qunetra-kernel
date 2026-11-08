// #define QUNETRA_STRING
// #define QUNETRA_MEMORY
// #define QUNETRA_MATH
// #define QUNETRA_CPU
// #define QUNETRA_LFB
// #define QUNETRA_FS
// #define QUNETRA_STORAGE
#define QUNETRA_CLI
#include "qunetra.h" // IWYU pragma: keep

// Calls functions before the loop
void qunetraStart() {
	addQunetraBaseCommands();
	kprintf("Hello from " KERNEL_NAME " " KERNEL_VERSION_STR " !\n");
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
}
