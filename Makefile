VERSION = 0.38.7
CC = clang
LINK = lld-link
PYTHON = python3
FONT_REGULAR ?= /usr/share/fonts/opentype/inter/Inter-Regular.otf
FONT_MEDIUM ?= /usr/share/fonts/opentype/inter/Inter-SemiBold.otf
CFLAGS = --target=x86_64-pc-windows-msvc -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror -Isrc
OBJECTS = build/boot.obj build/ui.obj build/font.obj build/ps2.obj build/pe64.obj build/ntcore.obj build/winapi.obj build/fileboot.obj build/hostcursor.obj build/embed_app.obj build/icons.obj
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
build/icons.c: tools/icons.py assets/icons.zip assets/start-icon.png.base64 | build
	$(PYTHON) tools/icons.py assets/icons.zip $@
build/icons.obj: build/icons.c src/icons.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/BOOTX64.EFI: $(OBJECTS)
	$(LINK) /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64 /out:$@ $(OBJECTS)
build/embed_app.c: build/winapp.exe tools/embed.py
	$(PYTHON) tools/embed.py build/winapp.exe $@
build/embed_app.obj: build/embed_app.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/esp.img: build/BOOTX64.EFI build/winapp.exe tools/esp.py
	$(PYTHON) tools/esp.py build/BOOTX64.EFI $@ build/winapp.exe
build/esp-iso.img: build/BOOTX64.EFI build/winapp.exe tools/compact_esp.py
	$(PYTHON) tools/compact_esp.py build/BOOTX64.EFI $@ build/winapp.exe
Quartzie-x64-uefi.img: build/esp.img tools/gpt.py
	$(PYTHON) tools/gpt.py build/esp.img $@
Quartzie-x64.iso: build/esp-iso.img tools/iso.py
	$(PYTHON) tools/iso.py $< $@
build/ui-test: tests/ui.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ui.c src/ui.c build/font.c build/icons.c -o $@
build/ps2-test: tests/ps2.c src/ps2.c src/ps2.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ps2.c src/ps2.c -o $@
build/glass-test: tests/glass.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/glass.c build/font.c build/icons.c -o $@
build/window-test: tests/window.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/window.c src/ui.c build/font.c build/icons.c -o $@
test: build/start-test build/timing-test build/icons-test build/ntcore-test build/pe-test build/winapp.exe build/ui-test build/ps2-test build/glass-test build/window-test build/cursor-test build/layout-test build/tween-test build/transitions-test
	./build/icons-test
	./build/start-test
	./build/timing-test
	./build/ntcore-test
	./build/pe-test build/winapp.exe
	./build/ui-test
	./build/ps2-test
	./build/glass-test
	./build/window-test
	./build/cursor-test
	./build/layout-test
	./build/tween-test
	./build/transitions-test
verify: all test
	$(PYTHON) tools/verify.py Quartzie-x64.iso Quartzie-x64-uefi.img build/esp.img build/BOOTX64.EFI build/esp-iso.img
	$(PYTHON) tools/check_app.py build/winapp.exe Quartzie-x64.iso Quartzie-x64-uefi.img
clean:
	rm -rf build Quartzie-x64.iso Quartzie-x64-uefi.img Quartzie-$(VERSION)-x64.iso Quartzie-$(VERSION)-x64.img
build/start-test: tests/start.c src/ui.c src/ui.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/start.c src/ui.c build/font.c build/icons.c -o $@
build/transitions-test: tests/transitions.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/transitions.c src/ui.c build/font.c build/icons.c -o $@
build/tween-test: tests/tween.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/tween.c src/ui.c build/font.c build/icons.c -o $@
build/layout-test: tests/layout.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/layout.c src/ui.c build/font.c build/icons.c -o $@
build/cursor-test: tests/cursor.c src/boot.c src/ui.c src/ps2.c src/winapi.c src/ntcore.c src/pe64.c src/fileboot.c src/hostcursor.c src/ui.h src/efi.h build/font.c build/icons.c build/embed_app.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/cursor.c src/ui.c src/ps2.c src/winapi.c src/ntcore.c src/pe64.c src/fileboot.c src/hostcursor.c build/font.c build/icons.c build/embed_app.c -o $@
build/ntcore-test: tests/ntcore.c src/ntcore.c src/ntcore.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/ntcore.c src/ntcore.c -o $@
build/kernel32.lib: tests/kernel32.def | build
	$(LINK) /dll /noentry /machine:x64 /def:tests/kernel32.def /out:build/kernel32.dll /implib:$@
build/winapp.obj: tests/win32_app.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/winapp.exe: build/winapp.obj build/kernel32.lib
	$(LINK) /subsystem:console /entry:mainCRTStartup /nodefaultlib /dynamicbase /machine:x64 /out:$@ build/winapp.obj build/kernel32.lib
build/pe-test: tests/pe64.c src/pe64.c src/pe64.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/pe64.c src/pe64.c -o $@
build/icons-test: tests/icons.c build/icons.c src/icons.h | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/icons.c build/icons.c -o $@
build/timing-test: tests/timing.c src/ui.c src/ui.h src/font.h src/icons.h build/font.c build/icons.c | build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/timing.c src/ui.c build/font.c build/icons.c -o $@
.PHONY: all release test verify clean
