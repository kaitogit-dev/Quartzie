CC = clang
LINK = lld-link
PYTHON = python3
CFLAGS = --target=x86_64-pc-windows-msvc -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror -Isrc
OBJECTS = build/boot.obj build/ui.obj build/font.obj
all: Quartzie-x64.iso Quartzie-x64-uefi.img
build:
	mkdir -p build
build/%.obj: src/%.c | build
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
	$(PYTHON) tools/iso.py build/esp.img $@
build/ui-test: tests/ui.c src/ui.c src/font.c src/ui.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ui.c src/ui.c src/font.c -o $@
test: build/ui-test
	./build/ui-test
verify: all test
	$(PYTHON) tools/verify.py Quartzie-x64.iso Quartzie-x64-uefi.img build/esp.img build/BOOTX64.EFI build/esp-iso.img
clean:
	rm -rf build Quartzie-x64.iso Quartzie-x64-uefi.img
.PHONY: all test verify clean
