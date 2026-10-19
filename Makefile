VERSION = 0.35.1
CC = clang
LINK = lld-link
PYTHON = python3
FONT_REGULAR ?= /usr/share/fonts/opentype/inter/Inter-Regular.otf
FONT_MEDIUM ?= /usr/share/fonts/opentype/inter/Inter-SemiBold.otf
CFLAGS = --target=x86_64-pc-windows-msvc -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror -Isrc
OBJECTS = build/boot.obj build/ui.obj build/font.obj build/ps2.obj
all: Quartzie-x64.iso Quartzie-x64-uefi.img
release: all
	cp Quartzie-x64.iso Quartzie-$(VERSION)-x64.iso
	cp Quartzie-x64-uefi.img Quartzie-$(VERSION)-x64.img
build:
	mkdir -p build
build/%.obj: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/font.c: tools/font.py | build
	$(PYTHON) tools/font.py $@ $(FONT_REGULAR) $(FONT_MEDIUM)
build/font.obj: build/font.c src/font.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/BOOTX64.EFI: $(OBJECTS)
	$(LINK) /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64 /out:$@ $(OBJECTS)
build/esp.img: build/BOOTX64.EFI tools/esp.py
	$(PYTHON) tools/esp.py $< $@
build/esp-iso.img: build/BOOTX64.EFI tools/compact_esp.py
	$(PYTHON) tools/compact_esp.py $< $@
Quartzie-x64-uefi.img: build/esp.img tools/gpt.py
	$(PYTHON) tools/gpt.py build/esp.img $@
Quartzie-x64.iso: build/esp-iso.img tools/iso.py
	$(PYTHON) tools/iso.py $< $@
build/ui-test: tests/ui.c src/ui.c src/ui.h src/font.h build/font.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ui.c src/ui.c build/font.c -o $@
build/ps2-test: tests/ps2.c src/ps2.c src/ps2.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ps2.c src/ps2.c -o $@
build/glass-test: tests/glass.c src/ui.c src/ui.h src/font.h build/font.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/glass.c build/font.c -o $@
build/window-test: tests/window.c src/ui.c src/ui.h src/font.h build/font.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/window.c src/ui.c build/font.c -o $@
test: build/ui-test build/ps2-test build/glass-test build/window-test build/cursor-test
	./build/ui-test
	./build/ps2-test
	./build/glass-test
	./build/window-test
	./build/cursor-test
verify: all test
	$(PYTHON) tools/verify.py Quartzie-x64.iso Quartzie-x64-uefi.img build/esp.img build/BOOTX64.EFI build/esp-iso.img
clean:
	rm -rf build Quartzie-x64.iso Quartzie-x64-uefi.img Quartzie-$(VERSION)-x64.iso Quartzie-$(VERSION)-x64.img
build/cursor-test: tests/cursor.c src/boot.c src/ui.c src/ps2.c src/ui.h src/efi.h build/font.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/cursor.c src/ui.c src/ps2.c build/font.c -o $@
.PHONY: all release test verify clean
