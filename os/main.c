//	#define QUNETRA_MEMORY
//	#define QUNETRA_STRING
//	#define QUNETRA_TIMER
//	#define QUNETRA_BITS
//	#define QUNETRA_MATH
	#define QUNETRA_CLI
//	#define QUNETRA_CPU
//	#define QUNETRA_LFB
//	#define QUNETRA_NET
	#define QUNETRA_FS
#include "qunetra.h" // IWYU pragma: keep

// Calls functions before the loop
void qunetraStart() {
	addQunetraBaseCommands();
	kprintf("Hello, World!\n");
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
}
