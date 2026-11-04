// #define QUNETRA_STRING
// #define QUNETRA_MEMORY
// #define QUNETRA_MATH
// #define QUNETRA_CPU
// #define QUNETRA_LFB
// #define QUNETRA_FS
// #define QUNETRA_STORAGE
#include "qunetra.h" // IWYU pragma: keep

// Calls functions before the loop
void qunetraStart() {
	kprintf("Hello, World!\n");
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
}
