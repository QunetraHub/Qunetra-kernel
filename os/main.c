// #define QUNETRA_STRING
// #define QUNETRA_MEMORY
// #define QUNETRA_MATH
// #define QUNETRA_CPU
// #define QUNETRA_LFB
// #define QUNETRA_FS
#define QUNETRA_STORAGE
#include "qunetra.h" // IWYU pragma: keep

#define FILE "hello.txt"
// Calls functions before the loop
void qunetraStart() {
	kprintf("Hello from " KERNEL_NAME " ver. " KERNEL_VERSION_STR " !\n\n");
	uint64 f = findFile(1, FILE);
	// if (f) deleteFile(1, FILE);
	// f = createFile(1, FILE);

	// kfwrite(f, "Hello, World!");

	const uint64 fs = fileSize(f);
	kprintf(FILE ":\n\tsector: %p\n\t  size: %uB\n\t  data:\n", f, fs);
	uint i = 0;
	uint8 v = 0, l = 0;
	while (freadb(f, i, &v)) {
		putChar(v);
		i++;
		l++;
		if (l >= 64) {
			putChar('\n');
			l = 0;
		}
	}
	putChar('\n');
}
// The system's main loop passes the currently pressed keyboard key
void qunetraLoop(const char keyPressed) {
	(void)keyPressed;
}
