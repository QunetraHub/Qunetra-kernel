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

#include "types.h"

#define HEAP_START 0x00400000
#define HEAP_INITIAL_SIZE (1024 * 1024)

typedef struct block_header {
	vsize size;
	state is_free;
	struct block_header *next;
} block_header_t;

static block_header_t *heap_head = 0;

void heap_init() {
	heap_head = (block_header_t *)HEAP_START;
	heap_head->size = HEAP_INITIAL_SIZE - sizeof(block_header_t);
	heap_head->is_free = 1;
	heap_head->next = 0;
}
void *kmalloc(vsize size) {
	size = (size + 3) & ~3;
	block_header_t *current = heap_head;
	while (current != 0) {
		if (current->is_free && current->size >= size) {
			if (current->size > size + sizeof(block_header_t)) {
				block_header_t *new_block = (block_header_t*)((uint8*)current + sizeof(block_header_t) + size);
				new_block->size = current->size - size - sizeof(block_header_t);
				new_block->is_free = true;
				new_block->next = current->next;
				current->size = size;
				current->next = new_block;
			}
			current->is_free = false;
			return (void *)((uint8 *)current + sizeof(block_header_t));
		}
		current = current->next;
	}
	return 0;
}
void kfree(void *ptr) {
	if (!ptr) return;
	block_header_t *header = (block_header_t*)((uint8*)ptr - sizeof(block_header_t));
	header->is_free = 1;
	block_header_t *current = heap_head;
	while (current != 0 && current->next != 0) {
		if (current->is_free && current->next->is_free) {
			current->size += sizeof(block_header_t) + current->next->size;
			current->next = current->next->next;
		} else current = current->next;
	}
}
void *krealloc(void *ptr, const vsize new_size) {
	if (!ptr) return kmalloc(new_size);
	if (new_size == 0) {
		kfree(ptr);
		return 0;
	}
	block_header_t *header = (block_header_t*)((uint8*)ptr - sizeof(block_header_t));
	if (header->size >= new_size) return ptr;
	void *new_ptr = kmalloc(new_size);
	if (!new_ptr) return 0;
	uint8 *src = (uint8*)ptr, *dst = (uint8*)new_ptr;
	const vsize copy_size = header->size < new_size ? header->size : new_size;
	for (vsize i = 0; i < copy_size; i++) dst[i] = src[i];
	kfree(ptr);
	return new_ptr;
}
uint memoryTest() {
	uint x = 0;
	char *p1 = kmalloc(32), *p2 = kmalloc(64);
	if (!p1 || !p2) return x;
	x++;
	kfree(p1);
	char *p3 = kmalloc(16);
	if (!p3) return x;
	x++;
	char *p2n = krealloc(p2, 128);
	if (!p2n) {
		kfree(p2);
		kfree(p3);
		return x;
	}
	p2 = p2n;
	x++;
	kfree(p2);
	kfree(p3);
	return x;
}
