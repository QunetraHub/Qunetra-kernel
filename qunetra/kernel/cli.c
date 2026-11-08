/*
 * Copyright (c) 2026 Qunerum
 *
 * This file is part of Qunetra.
 *
 * Qunetra is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Qunetra is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Qunetra. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../drivers/storage/storage.h"
#include "../utility/string.h"
#include "../utility/types.h"
#include "keyboard.h"
#include "console.h"
#include "cli.h"

#ifndef QUNETRA_CMD_MAX_LEN
#define QUNETRA_CMD_MAX_LEN 128
#endif
#ifndef QUNETRA_CLI_MAX_ARGS
#define QUNETRA_CLI_MAX_ARGS 32
#endif
#ifndef QUNETRA_CLI_MAX_CMDS
#define QUNETRA_CLI_MAX_CMDS 32
#endif
#ifndef QUNETRA_CLI_HISTORY
#define QUNETRA_CLI_HISTORY 8
#endif
#ifndef QUNETRA_CLI_PROMPT
#define QUNETRA_CLI_PROMPT "> "
#endif
#ifndef QUNETRA_CLI_PROMPT_COLOR
#define QUNETRA_CLI_PROMPT_COLOR 150
#endif

typedef struct {
	const char* name, *help;
	cliAction action;
} qCmd;

static uint16 inputLength = 0;
static char input[QUNETRA_CMD_MAX_LEN + 1], history[QUNETRA_CLI_HISTORY][QUNETRA_CMD_MAX_LEN + 1], draft[QUNETRA_CMD_MAX_LEN + 1];
static state CLIDisabled = false, needPrompt = false;
static qCmd commands[QUNETRA_CLI_MAX_CMDS];
static uint cmdCount = 0, histCount = 0, histHead = 0, histPos = 0;

static void printPrompt() { kprintf("%q%s%q", QUNETRA_CLI_PROMPT_COLOR, QUNETRA_CLI_PROMPT, 13); }
static void eraseLine() {
	while (inputLength > 0) {
		putChar('\b');
		inputLength--;
	}
	input[0] = '\0';
}
static void setLine(const char* s) {
	eraseLine();
	for (uint i = 0; s[i] != '\0' && inputLength < QUNETRA_CMD_MAX_LEN; i++) {
		putChar(s[i]);
		input[inputLength++] = s[i];
	}
	input[inputLength] = '\0';
}
void clearCLI() { eraseLine(); }
void setCLIState(const state _state) {
	const state wasDisabled = CLIDisabled;
	CLIDisabled = !_state;
	if (CLIDisabled) eraseLine();
	else if (wasDisabled) needPrompt = true;
}
state addCmd(const char* name, const char* help, cliAction action) {
	if (!name || name[0] == '\0' || !action || cmdCount >= QUNETRA_CLI_MAX_CMDS) return false;
	for (const char* p = name; *p != '\0'; p++) if (*p <= ' ' || *p == '"' || *p == '\\') return false;
	for (uint i = 0; i < cmdCount; i++) if (strIs(commands[i].name, name)) return false;
	commands[cmdCount].name = name;
	commands[cmdCount].help = help;
	commands[cmdCount].action = action;
	cmdCount++;
	return true;
}
static int tokenize(char* s, char** argv, const int maxArgs) {
	int argc = 0;
	char* w = s;
	while (true) {
		while (*s == ' ' || *s == '\t') s++;
		if (*s == '\0') break;
		if (argc >= maxArgs) return -1;
		argv[argc++] = w;
		state quoted = false;
		while (*s != '\0' && (quoted || (*s != ' ' && *s != '\t'))) {
			if (*s == '"') {
				quoted = !quoted;
				s++;
				continue;
			}
			if (*s == '\\' && s[1] != '\0') s++;
			*w++ = *s++;
		}
		if (quoted) return -2;
		const state more = *s != '\0';
		*w++ = '\0';
		if (more) s++;
	}
	argv[argc] = 0;
	return argc;
}
state runCmd(const char* line) {
	if (!line) return false;
	char work[QUNETRA_CMD_MAX_LEN + 1], *argv[QUNETRA_CLI_MAX_ARGS + 1];
	strCopyN(work, line, QUNETRA_CMD_MAX_LEN);
	const int argc = tokenize(work, argv, QUNETRA_CLI_MAX_ARGS);
	if (argc == -1) {
		kprintf("%qToo many arguments (max %d)%q\n", 20, QUNETRA_CLI_MAX_ARGS, 13);
		return false;
	}
	if (argc == -2) {
		kprintf("%qMissing closing quote%q\n", 20, 13);
		return false;
	}
	if (argc == 0) return false;
	for (uint i = 0; i < cmdCount; i++) {
		if (strIs(argv[0], commands[i].name)) {
			commands[i].action(argc, argv);
			return true;
		}
	}
	kprintf("%qUnknown command: '%s' (try 'help')%q\n", 20, argv[0], 13);
	return false;
}

__nqc__(help) {
	if (argc > 1) {
		for (uint i = 0; i < cmdCount; i++) {
			if (strIs(commands[i].name, argv[1])) {
				kprintf("%s - %s\n", commands[i].name, commands[i].help ? commands[i].help : "");
				return;
			}
		}
		kprintf("%qNo such command: '%s'%q\n", 20, argv[1], 13);
		return;
	}
	uint width = 0;
	for (uint i = 0; i < cmdCount; i++) if (strLen(commands[i].name) > width) width = strLen(commands[i].name);
	for (uint i = 0; i < cmdCount; i++) {
		kprintf("%s", commands[i].name);
		for (uint k = strLen(commands[i].name); k < width + 2; k++) putChar(' ');
		kprintf("%q- %s\n", 8, commands[i].help ? commands[i].help : "");
	}
}
__nqc__(echo) {
	for (int i = 1; i < argc; i++) {
		if (i > 1) putChar(' ');
		kprintf("%s", argv[i]);
	}
	putChar('\n');
}
static uint64 curDir = 1;

state initCLI() {
	cmdCount = 0;
	histCount = 0;
	histHead = 0;
	histPos = 0;
	eraseLine();
	needPrompt = true;
	return __aqc__(help, "list commands, 'help <command>' shows one") && __aqc__(echo, "print the arguments");
}
static const char* historyEntry(const uint k) { return history[(histHead + QUNETRA_CLI_HISTORY - k) % QUNETRA_CLI_HISTORY]; }
static void addHistory() {
	if (histCount > 0 && strIs(historyEntry(1), input)) return;
	strCopyN(history[histHead], input, QUNETRA_CMD_MAX_LEN);
	histHead = (histHead + 1) % QUNETRA_CLI_HISTORY;
	if (histCount < QUNETRA_CLI_HISTORY) histCount++;
}
static void historyUp() {
	if (histPos >= histCount) return;
	if (histPos == 0) strCopyN(draft, input, QUNETRA_CMD_MAX_LEN);
	histPos++;
	setLine(historyEntry(histPos));
}
static void historyDown() {
	if (histPos == 0) return;
	histPos--;
	setLine(histPos == 0 ? draft : historyEntry(histPos));
}
static void complete() {
	for (uint i = 0; i < inputLength; i++) if (input[i] == ' ') return;
	uint matches = 0, first = 0;
	for (uint i = 0; i < cmdCount; i++) {
		if (!strStartWith(commands[i].name, input)) continue;
		if (matches == 0) first = i;
		matches++;
	}
	if (matches == 0) return;
	histPos = 0;
	if (matches == 1) {
		setLine(commands[first].name);
		if (inputLength < QUNETRA_CMD_MAX_LEN) {
			putChar(' ');
			input[inputLength++] = ' ';
			input[inputLength] = '\0';
		}
		return;
	}
	uint common = strLen(commands[first].name);
	for (uint i = first + 1; i < cmdCount; i++) {
		if (!strStartWith(commands[i].name, input)) continue;
		uint k = 0;
		while (k < common && commands[i].name[k] == commands[first].name[k]) k++;
		common = k;
	}
	if (common > inputLength) {
		char part[QUNETRA_CMD_MAX_LEN + 1];
		strCopyN(part, commands[first].name, common < QUNETRA_CMD_MAX_LEN ? common : QUNETRA_CMD_MAX_LEN);
		setLine(part);
		return;
	}
	putChar('\n');
	for (uint i = 0; i < cmdCount; i++) if (strStartWith(commands[i].name, input)) kprintf("%s  ", commands[i].name);
	putChar('\n');
	printPrompt();
	for (uint i = 0; i < inputLength; i++) putChar(input[i]);
}
void setCharCLI(const char c) {
	if (CLIDisabled) return;
	if (needPrompt) {
		printPrompt();
		needPrompt = false;
	}
	if (c == '\0') return;
	switch (c) {
		case '\n':
			putChar('\n');
			if (inputLength > 0) {
				addHistory();
				runCmd(input);
			}
			inputLength = 0;
			input[0] = '\0';
			histPos = 0;
			needPrompt = true;
			return;
		case '\b':
			if (inputLength > 0) {
				putChar('\b');
				inputLength--;
				input[inputLength] = '\0';
				histPos = 0;
			}
			return;
		case '\t': complete(); return;
		case KEY_ESC:
			eraseLine();
			histPos = 0;
			return;
		case KEY_UP: historyUp(); return;
		case KEY_DOWN: historyDown(); return;
		default: break;
	}
	if (c < 32 || c > 126 || inputLength >= QUNETRA_CMD_MAX_LEN) return;
	putChar(c);
	input[inputLength++] = c;
	input[inputLength] = '\0';
	histPos = 0;
}
#define __getFile_TF__(fi) const uint64 fs = findFile(curDir, argv[fi]); if (!fs) { kprintf("%qFile '%s' was not found\n", 20, argv[1]); return; }
#define __getFile_TC__(fi) const uint64 fs = findFile(curDir, argv[fi]); if (fs) { kprintf("%qFile '%s' already exists!\n", 20, argv[1]); return; }

__nqc__(ls) { __qcdb__ listFiles(curDir); }
__nqc__(cat) { __qccu__(1, "<filename>")
	__getFile_TF__(1)
	if (fileSize(fs)) {
		uint64 i = 0;
		uint8 v = 0;
		while (freadb(fs, i++, &v)) putChar(v);
	} else kprintf("%qThe file is empty", 6);
	putChar('\n');
}
__nqc__(touch) { __qccu__(1, "<filename>")
	__getFile_TC__(1)
	createFile(curDir, argv[1]);
}
__nqc__(rm) { __qccu__(1, "<filename>")
	__getFile_TF__(1)
	deleteFile(curDir, argv[1]);
}
__nqc__(stat) { __qccu__(1, "<filename>")
	__getFile_TF__(1)
}

void addQunetraBaseCommands() {
	__aqc__(ls, "List files in the current directory");
	__aqc__(cat, "Display the content of a file");
	__aqc__(touch, "Create a new empty file");
	__aqc__(rm, "Delete a specified file");
	__aqc__(stat, "Show file metadata (size & ID)");
}
