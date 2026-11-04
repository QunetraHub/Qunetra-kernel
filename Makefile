CC  = gcc
ASM = nasm

BIN_DIR = .bin
OBJ_DIR = .obj
ISO_DIR = iso
ISO     = qunetra.iso
KERNEL  = $(BIN_DIR)/kernel.bin
IMG     = qunetra.img
IMG_SIZE = 64 # MiB

CFLAGS   = -std=gnu99 -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -I. -Iinclude
ASMFLAGS = -f elf32
LDFLAGS  = -m32 -nostdlib -no-pie -T qunetra/boot/linker.ld

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

C_SRCS   = $(call rwildcard,.,*.c)
ASM_SRCS = $(call rwildcard,.,*.asm)

OBJ_C   = $(patsubst ./%.c, $(OBJ_DIR)/%.o, $(C_SRCS))
OBJ_ASM = $(patsubst ./%.asm, $(OBJ_DIR)/%_asm.o, $(ASM_SRCS))
OBJECTS = $(OBJ_ASM) $(OBJ_C)

K = \033[1;38;5;240m
T = \033[1;38;5;246m
L = \033[1;38;5;250m
G = \033[1;38;5;34m

C_C   = \033[1;38;5;39m
ASM_C = \033[1;38;5;99m

define CHECK_TOOL
	@if ! command -v $(1) >/dev/null 2>&1; then \
		echo "\033[1;31m[ERROR] Missing required tool: '$(1)'\033[0m"; \
		echo "Install it on your system (e.g., Ubuntu/Debian: sudo apt install $(2))"; \
		exit 1; \
	fi
endef

all: check-deps $(BIN_DIR)/$(ISO)

check-deps:
	$(call CHECK_TOOL,gcc,build-essential)
	$(call CHECK_TOOL,nasm,nasm)
	$(call CHECK_TOOL,qemu-system-i386,qemu-system-x86)
	$(call CHECK_TOOL,grub-mkrescue,grub-pc-bin xorriso)
	$(call CHECK_TOOL,xorriso,xorriso)
	$(call CHECK_TOOL,dd,coreutils)

$(IMG):
	@dd if=/dev/zero of=$@ bs=1M count=$(IMG_SIZE) >/dev/null 2>&1
	@printf "$(T)New image file: $@\n"

run: all $(IMG)
	@printf "$(T)Running '$(L)$(ISO)$(T)' on $(L)QEMU$(T)...$(T)\n"
	@qemu-system-i386 -cdrom $(BIN_DIR)/$(ISO) -boot d \
		-device VGA,xres=1280,yres=720 -display gtk,zoom-to-fit=off \
		-drive file=$(IMG),format=raw,if=ide,index=0,media=disk \
		-d int,cpu_reset -D qemu.log

$(BIN_DIR)/$(ISO): $(KERNEL) grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	@cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $@ $(ISO_DIR) >/dev/null 2>&1
	@rm -rf $(ISO_DIR)
	@printf "$(G)ISO is ready: $@$(T)\n"

$(KERNEL): $(OBJECTS) | $(BIN_DIR)
	@printf "$(L)Linking Kernel...$(T)\n"
	@$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@printf "$(K)[$(C_C) C $(K)]$(T) $<\n"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%_asm.o: %.asm | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@printf "$(K)[$(ASM_C)ASM$(K)]$(T) $<\n"
	@$(ASM) $(ASMFLAGS) $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR) $(ISO_DIR) *.iso
	@printf "$(T)Cleaned!\n"

.PHONY: all run clean check-deps
