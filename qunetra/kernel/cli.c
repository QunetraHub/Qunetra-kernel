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

#include "../utility/string.h"
#include "../utility/memory.h"
#include "../utility/types.h"
#include "console.h"

#ifndef QUNETRA_CMD_MAX_LEN
#define QUNETRA_CMD_MAX_LEN 128
#endif
#ifndef QUNETRA_CMDL_STEP
#define QUNETRA_CMDL_STEP 64
#endif

static uint16 inputLength = 0;
static char input[QUNETRA_CMD_MAX_LEN + 1];
static state CLIDisabled = false;

void clearCLI() {
	input[0] = '\0';
	while (inputLength > 0) {
		putChar('\b');
		inputLength--;
	}
}
void setCLIState(const state _state) {
	CLIDisabled = !_state;
	if (CLIDisabled && inputLength > 0) clearCLI();
}
typedef struct {
	char* cmd;
	void (*action)();
} qCmd;
static qCmd* commands = 0;
static uint cmdCount = 0, maxCount = QUNETRA_CMDL_STEP;
state initCLI() {
	commands = kmalloc(QUNETRA_CMDL_STEP * sizeof(qCmd));
	return commands != 0;
}
void addCmd(const char* cmd, const void (*action)()) {
	if (cmdCount >= maxCount) {
		qCmd* newCmds = krealloc(commands, (maxCount + QUNETRA_CMDL_STEP) * sizeof(qCmd));
		commands = newCmds;
	}
	// commands[cmdCount].cmd = cmd;
	// commands[cmdCount].action = action;
	cmdCount++;
}
void runCmd(const char* cmd, uint maxLen) {
	if (inputLength == 0) return;
	putChar('\n');
	int cmdLen = strFind(cmd, ' ');
	if (cmdLen < 0) cmdLen = maxLen;
	char tmp[cmdLen + 1];
	strCopyN(tmp, cmd, cmdLen);
	uint cmdID = 0;
	state cmdFound = false;
	for (uint i = 0; i < cmdCount; i++) {
		if (strIs(tmp, commands[i].cmd)) {
			cmdID = i;
			cmdFound = true;
			break;
		}
	}
	if (!cmdFound) {
		kprintf("%qUnkown command! ['%s']%q\n", 20, cmd, 13);
		inputLength = 0;
		input[0] = '\0';
		return;
	}
	
	inputLength = 0;
	input[0] = '\0';
}
void setCharCLI(const char c) {
	if (CLIDisabled || c == '\0') return;
	if (c == '\n') {
		runCmd(input, inputLength);
		return;
	}
	if (c == '\b') {
		putChar('\b');
		if (inputLength > 0) inputLength--;
		input[inputLength] = '\0';
		return;
	}
	if (inputLength >= QUNETRA_CMD_MAX_LEN) return;
	putChar(c);
	input[inputLength] = c;
	inputLength++;
	input[inputLength] = '\0';
}
