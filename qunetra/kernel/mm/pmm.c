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

#include "../../utility/types.h"

#define BLOCK_SIZE 4096

static uint8 pmm_bitmap[2048];
static uint32 pmm_max_blocks = 16384;

static void bitmap_set(uint32 bit) { pmm_bitmap[bit / 8] |= (1 << (bit % 8)); }
static void bitmap_clear(uint32 bit) { pmm_bitmap[bit / 8] &= ~(1 << (bit % 8)); }
static int bitmap_test(uint32 bit) { return pmm_bitmap[bit / 8] & (1 << (bit % 8)); }
void pmm_init(uint32 mem_upper_kb) {
	uint32 total_ram_blocks = (mem_upper_kb * 1024) / BLOCK_SIZE;
	if (total_ram_blocks < pmm_max_blocks) pmm_max_blocks = total_ram_blocks;
	for (uint32 i = 0; i < sizeof(pmm_bitmap); i++) pmm_bitmap[i] = 0;
	for (uint32 i = 0; i < (1024 * 1024) / BLOCK_SIZE; i++) bitmap_set(i);
}
void *pmm_alloc_block(void) {
	for (uint32 i = 0; i < pmm_max_blocks; i++) {
		if (!bitmap_test(i)) {
			bitmap_set(i);
			return (void*)(uiptr)(i * BLOCK_SIZE);
		}
	}
	return 0;
}
void pmm_free_block(void *addr) {
	uint32 frame = (uint32)(uiptr)addr / BLOCK_SIZE;
	bitmap_clear(frame);
}
