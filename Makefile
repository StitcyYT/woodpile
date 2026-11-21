BUILD = build
SRC = src

# Fixed physical address the bootloader copies the initramfs to.
# Must be above kernel .bss and below the 1 MiB low-mem reserve.
IRAMFS_ADDR = 10485760

# Resolution returned by vbe_get_resolution(). VBE uses this mode when the
# firmware supports it exactly; otherwise it picks the closest smaller mode.
VBE_WIDTH  ?= 1920
VBE_HEIGHT ?= 1080

NASM = nasm
CC = gcc
LD = ld

CFLAGS = -ffreestanding -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-common -c -I$(SRC)/kernel
XEC_CFLAGS = -O2 -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-common \
             -fno-stack-protector -fno-asynchronous-unwind-tables \
             -mno-red-zone -mno-sse -mno-sse2 -c -I$(SRC)/xec
XEC_AFLAGS = -ffreestanding -m64 -c
LDFLAGS = -T $(SRC)/kernel/linker.ld --oformat binary

OBJS = $(BUILD)/entry.o $(BUILD)/start.o $(BUILD)/vga_testing.o $(BUILD)/font.o \
       $(BUILD)/font_data.o $(BUILD)/framebuffer.o $(BUILD)/vbe.o \
       $(BUILD)/vbe_trampoline.o $(BUILD)/vbe_switch.o $(BUILD)/time.o \
       $(BUILD)/pmm.o $(BUILD)/kalloc.o $(BUILD)/isr.o $(BUILD)/serial.o \
       $(BUILD)/idt.o $(BUILD)/panic.o $(BUILD)/memtest.o $(BUILD)/initramfs.o \
       $(BUILD)/initramfs_ptr.o $(BUILD)/kbd.o $(BUILD)/mouse.o $(BUILD)/vfs.o \
       $(BUILD)/abi.o $(BUILD)/xec.o $(BUILD)/audio.o $(BUILD)/ata.o

XECS = $(BUILD)/qnt.xec $(BUILD)/tbi.xec $(BUILD)/lid.xec \
       $(BUILD)/rmf.xec $(BUILD)/inf.xec $(BUILD)/less.xec \
       $(BUILD)/rplay.xec
RPLAY_SRC ?= $(HOME)/Documents/Codex/2026-10-23/what-x20
MEDIA = $(BUILD)/media
INITRD = $(BUILD)/initrd
ISOCG ?= $(HOME)/.local/bin/isocg

X8_SRC = $(SRC)/xec/x8
X8_GAMES = $(BUILD)/games/snake.xec $(BUILD)/games/pong.xec
X8_PROGS = $(BUILD)/progs/paint.xec $(BUILD)/progs/notepad.xec \
           $(BUILD)/progs/term.xec
X8_XECS = $(BUILD)/x8.xec $(X8_GAMES) $(X8_PROGS)
X8_LIB_SRCS = $(X8_SRC)/x8.c $(X8_SRC)/x8_winapi.c $(X8_SRC)/x8_cursor.c
X8_LIB_OBJS = $(BUILD)/x8.o $(BUILD)/x8_winapi.o $(BUILD)/x8_cursor.o
X8_CURSORS = $(wildcard assets/cursors/*)
X8_ICONS = $(wildcard assets/icons/*.ico)

all: $(BUILD)/os.img

$(BUILD)/initramfs_ptr.o: $(SRC)/kernel/initramfs_ptr.S
	$(NASM) -f elf64 $< -o $@

$(BUILD)/initramfs.o: $(SRC)/kernel/modules/initramfs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/kbd.o: $(SRC)/kernel/modules/kbd.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/mouse.o: $(SRC)/kernel/modules/mouse.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/vfs.o: $(SRC)/kernel/modules/vfs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/abi.o: $(SRC)/kernel/modules/abi.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/xec.o: $(SRC)/kernel/modules/xec.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/audio.o: $(SRC)/kernel/modules/audio.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/ata.o: $(SRC)/kernel/modules/ata.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/entry.o: $(SRC)/kernel/entry.asm
	$(NASM) -f elf64 $< -o $@

$(BUILD)/start.o: $(SRC)/kernel/start.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/vga_testing.o: $(SRC)/kernel/modules/vga_testing.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/font.o: $(SRC)/kernel/modules/font.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/font_data.o: $(SRC)/kernel/font_data.asm
	$(NASM) -f elf64 $< -o $@

$(BUILD)/framebuffer.o: $(SRC)/kernel/modules/framebuffer.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/vbe.o: $(SRC)/kernel/modules/vbe.c
	$(CC) $(CFLAGS) -DVBE_DEFAULT_WIDTH=$(VBE_WIDTH) -DVBE_DEFAULT_HEIGHT=$(VBE_HEIGHT) $< -o $@

$(BUILD)/vbe_trampoline.o: $(SRC)/kernel/vbe_trampoline.S
	$(CC) -ffreestanding -mno-red-zone -m64 -c -I$(SRC)/kernel -DTRAMPOLINE_PHYS=0xC000 -DSEL_CODE32=0x18 -DSEL_DATA32=0x20 -DSEL_CODE16=0x28 -DSEL_DATA16=0x30 -DSEL_CODE64=0x08 -DVBE_FUNC_CTRL_INFO=0x4F00 -DVBE_FUNC_MODE_INFO=0x4F01 -DVBE_FUNC_SET_MODE=0x4F02 $< -o $@

$(BUILD)/vbe_switch.o: $(SRC)/kernel/vbe_switch.S
	$(CC) -ffreestanding -mno-red-zone -m64 -c -DTRAMPOLINE_PHYS=0xC000 $< -o $@

$(BUILD)/time.o: $(SRC)/kernel/modules/time.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/pmm.o: $(SRC)/kernel/modules/pmm.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/kalloc.o: $(SRC)/kernel/modules/kalloc.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/isr.o: $(SRC)/kernel/isr.asm
	$(NASM) -f elf64 $< -o $@

$(BUILD)/serial.o: $(SRC)/kernel/modules/serial.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/idt.o: $(SRC)/kernel/modules/idt.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/panic.o: $(SRC)/kernel/modules/panic.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/memtest.o: $(SRC)/kernel/modules/memtest.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/kernel_raw.bin: $(OBJS) $(SRC)/kernel/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel_raw.bin $(BUILD)/initramfs.bin
	cp $< $@; \
	KERN_SIZE=$$(stat -c%s $@); \
	PAD=$$(((512 - (KERN_SIZE % 512)) % 512)); \
	if [ $$PAD -gt 0 ]; then dd if=/dev/zero bs=1 count=$$PAD >> $@ 2>/dev/null; fi; \
	IRAMFS_ADDR=$(IRAMFS_ADDR); \
	IRAMFS_SIZE=$$(stat -c%s $(BUILD)/initramfs.bin); \
	KERN_SIZE=$$(stat -c%s $@); \
	IRAMFS_LBA=$$((2 + KERN_SIZE / 512)); \
	printf "\x$$(printf '%02x' $$((IRAMFS_ADDR & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 8) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 16) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 24) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 32) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 40) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 48) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_ADDR >> 56) & 0xFF)))" | dd of=$@ bs=1 seek=46592 conv=notrunc 2>/dev/null; \
	printf "\x$$(printf '%02x' $$((IRAMFS_SIZE & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 8) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 16) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 24) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 32) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 40) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 48) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_SIZE >> 56) & 0xFF)))" | dd of=$@ bs=1 seek=46600 conv=notrunc 2>/dev/null; \
	printf "\x$$(printf '%02x' $$((IRAMFS_LBA & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 8) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 16) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 24) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 32) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 40) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 48) & 0xFF)))\x$$(printf '%02x' $$(((IRAMFS_LBA >> 56) & 0xFF)))" | dd of=$@ bs=1 seek=46608 conv=notrunc 2>/dev/null; \
	echo "patched initramfs ptr=0x$$(printf '%x' $$IRAMFS_ADDR) size=$$IRAMFS_SIZE lba=$$IRAMFS_LBA"

# ---- .xec commands: flat binaries with an XEC1 header ----
define XEC_RULE
$(BUILD)/$(1).xec: $(SRC)/xec/$(1).c $(SRC)/xec/abi_stubs.S $(SRC)/xec/util.c \
                   $(SRC)/xec/inflate.c $(SRC)/xec/inflate.h \
                   $(SRC)/xec/xecabi.h $(SRC)/xec/xec.ld tools/elf2xec.py
	$(CC) $(XEC_CFLAGS) $$< -o $(BUILD)/$(1)_cmd.o
	$(CC) $(XEC_CFLAGS) $(SRC)/xec/util.c -o $(BUILD)/$(1)_util.o
	$(CC) $(XEC_CFLAGS) $(SRC)/xec/inflate.c -o $(BUILD)/$(1)_inflate.o
	$(CC) $(XEC_AFLAGS) $(SRC)/xec/abi_stubs.S -o $(BUILD)/$(1)_abi.o
	$(LD) -T $(SRC)/xec/xec.ld --defsym XEC_BASE=$(2) -o $(BUILD)/$(1).elf \
	    $(BUILD)/$(1)_abi.o $(BUILD)/$(1)_cmd.o $(BUILD)/$(1)_util.o \
	    $(BUILD)/$(1)_inflate.o
	python3 tools/elf2xec.py $(BUILD)/$(1).elf $$@ $(2)
endef

$(eval $(call XEC_RULE,qnt,0x900000))
$(eval $(call XEC_RULE,tbi,0x910000))
$(eval $(call XEC_RULE,lid,0x920000))
$(eval $(call XEC_RULE,rmf,0x930000))
$(eval $(call XEC_RULE,inf,0x940000))
$(eval $(call XEC_RULE,less,0x950000))
$(eval $(call XEC_RULE,rplay,0x960000))

# ---- X8: a tiny framebuffer window session and its bundled apps ----
$(BUILD)/games $(BUILD)/games/ $(BUILD)/progs $(BUILD)/progs/:
	mkdir -p $@

$(BUILD)/x8.o: $(X8_SRC)/x8.c $(X8_SRC)/x8.h $(SRC)/xec/xecabi.h
	$(CC) $(XEC_CFLAGS) $< -o $@

$(BUILD)/x8_winapi.o: $(X8_SRC)/x8_winapi.c $(X8_SRC)/x8_winapi.h \
                      $(X8_SRC)/x8.h $(SRC)/xec/xecabi.h
	$(CC) $(XEC_CFLAGS) $< -o $@

$(BUILD)/x8_cursor.o: $(X8_SRC)/x8_cursor.c $(X8_SRC)/x8_cursor.h \
                      $(X8_SRC)/x8.h $(SRC)/xec/xecabi.h
	$(CC) $(XEC_CFLAGS) $< -o $@

define X8_XEC_RULE
$(1): $(2) $(X8_LIB_OBJS) $(SRC)/xec/abi_stubs.S $(SRC)/xec/util.c \
      $(SRC)/xec/inflate.c $(SRC)/xec/inflate.h \
      $(SRC)/xec/xecabi.h $(SRC)/xec/xec.ld tools/elf2xec.py | $(dir $(1))
	$(CC) $(XEC_CFLAGS) $(2) -o $(BUILD)/$(4)_cmd.o
	$(CC) $(XEC_CFLAGS) $(SRC)/xec/util.c -o $(BUILD)/$(4)_util.o
	$(CC) $(XEC_CFLAGS) $(SRC)/xec/inflate.c -o $(BUILD)/$(4)_inflate.o
	$(CC) $(XEC_AFLAGS) $(SRC)/xec/abi_stubs.S -o $(BUILD)/$(4)_abi.o
	$(LD) -T $(SRC)/xec/xec.ld --defsym XEC_BASE=$(3) -o $(BUILD)/$(4).elf \
	    $(BUILD)/$(4)_abi.o $(BUILD)/$(4)_cmd.o $(X8_LIB_OBJS) \
	    $(BUILD)/$(4)_util.o $(BUILD)/$(4)_inflate.o
	python3 tools/elf2xec.py $(BUILD)/$(4).elf $(1) $(3)
endef

# X8 keeps five cursor images in BSS (~630 KB).  Its old 0x970000 base was
# overwritten by Paint and the other child apps, which killed the cursor on
# return.  0xE00000 is above the initramfs and outside every child's range.
$(eval $(call X8_XEC_RULE,$(BUILD)/x8.xec,$(X8_SRC)/x8_session.c,0xE00000,x8))
$(eval $(call X8_XEC_RULE,$(BUILD)/games/snake.xec,$(X8_SRC)/games/snake.c,0x980000,snake))
$(eval $(call X8_XEC_RULE,$(BUILD)/games/pong.xec,$(X8_SRC)/games/pong.c,0x990000,pong))
$(eval $(call X8_XEC_RULE,$(BUILD)/progs/paint.xec,$(X8_SRC)/progs/paint.c,0x9A0000,paint))
$(eval $(call X8_XEC_RULE,$(BUILD)/progs/notepad.xec,$(X8_SRC)/progs/notepad.c,0x9B0000,notepad))
$(eval $(call X8_XEC_RULE,$(BUILD)/progs/term.xec,$(X8_SRC)/progs/term.c,0x1500000,term))

# ---- PLAY3 media: keep every container playable via raw conversion ----
$(MEDIA): tools/rplay_prep.py
	python3 tools/rplay_prep.py $(RPLAY_SRC) $@

# ---- initramfs staging ----
$(INITRD): $(XECS) $(X8_XECS) $(MEDIA) assets/cursors assets/icons
	rm -rf $@
	mkdir -p $@
	cp -a initramfs/. $@/
	rm -f $@/sys32/boot16.co $@/sys32/run64.co $@/sys32/att.co \
	      $@/users/nuser.so $@/boot/floader.conf
	rm -rf $@/boot $@/users/nuser $@/assets
	mkdir -p $@/sys32/bin $@/sys32/lib/cursors $@/media $@/games $@/progs
	cp -a $(MEDIA)/. $@/media/
	cp -a assets/cursors/. $@/sys32/lib/cursors/
	mkdir -p $@/sys32/icons
	cp $(X8_ICONS) $@/sys32/icons/
	cp $(XECS) $@/sys32/bin/
	cp $(BUILD)/x8.xec $@/sys32/bin/
	cp $(X8_GAMES) $@/games/
	cp $(X8_PROGS) $@/progs/

$(BUILD)/initramfs.bin: $(INITRD) tools/mkinitramfs.py
	python3 tools/mkinitramfs.py $@ $(INITRD)

$(BUILD)/stage2.bin: $(SRC)/bootloader/stage2.asm $(BUILD)/kernel.bin $(BUILD)/initramfs.bin
	COMBINED=$$(( $$(stat -c%s $(BUILD)/kernel.bin) + $$(stat -c%s $(BUILD)/initramfs.bin) )); \
	KERN_SEC=$$(( $$(stat -c%s $(BUILD)/kernel.bin) / 512 )); \
	TOTAL=$$(( 1 + ($$COMBINED + 511) / 512 )); \
	$(NASM) -f bin -DTOTAL_SECTORS=$$TOTAL -DKERN_SECTORS=$$KERN_SEC -DIRAMFS_DST=$(IRAMFS_ADDR) $< -o $@

$(BUILD)/stage1.bin: $(SRC)/bootloader/stage1.asm $(BUILD)/kernel.bin $(BUILD)/initramfs.bin
	KERN_SEC=$$(( $$(stat -c%s $(BUILD)/kernel.bin) / 512 )); \
	TOTAL=$$(( 2 + KERN_SEC )); \
	IRAMFS_LBA=$$(( 2 + KERN_SEC )); \
	IRAMFS_SECS=$$(( ($$(stat -c%s $(BUILD)/initramfs.bin) + 511) / 512 )); \
	$(NASM) -f bin -DTOTAL_SECTORS=$$TOTAL -DIRAMFS_ADDR=$(IRAMFS_ADDR) \
		-DIRAMFS_LBA=$$IRAMFS_LBA -DIRAMFS_SECS=$$IRAMFS_SECS $< -o $@

$(BUILD)/os.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin $(BUILD)/initramfs.bin
	cat $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin > $@; \
	cat $(BUILD)/initramfs.bin >> $@; \
	truncate -s 16777216 $@

iso: $(BUILD)/os.img
	mkdir -p $(BUILD)/iso-stage
	cp $(BUILD)/os.img $(BUILD)/iso-stage/os.img
	$(ISOCG) -o $(BUILD)/os.iso -V WOODPILE_OS \
		-b $(BUILD)/iso-stage/os.img --hd-emul --boot-load-size 1 \
		$(BUILD)/iso-stage

run: $(BUILD)/os.img
	@xdotool mousemove 0 0
	qemu-system-x86_64 -display gtk,full-screen=on -vga std \
		-global VGA.xres=$(VBE_WIDTH) -global VGA.yres=$(VBE_HEIGHT) \
		-drive file=$(BUILD)/os.img,format=raw,index=0,media=disk -m 128M -serial stdio

run-iso: iso
	@xdotool mousemove 0 0
	qemu-system-x86_64 -display gtk,full-screen=on -vga std \
		-global VGA.xres=$(VBE_WIDTH) -global VGA.yres=$(VBE_HEIGHT) \
		-cdrom $(BUILD)/os.iso -m 128M -serial stdio

run-bg: $(BUILD)/os.img
	qemu-system-x86_64 -drive file=$(BUILD)/os.img,format=raw,index=0,media=disk -m 128M -display none -vnc :99 -serial stdio

clean:
	rm -f $(BUILD)/*.bin $(BUILD)/*.o $(BUILD)/*.co $(BUILD)/*.elf $(BUILD)/*.xec
	rm -f $(BUILD)/*.iso
	rm -rf $(BUILD)/games $(BUILD)/progs $(BUILD)/iso-stage
	rm -rf $(INITRD) $(MEDIA)

.PHONY: all clean run run-iso run-bg iso
