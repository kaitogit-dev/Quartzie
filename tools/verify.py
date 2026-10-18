import pathlib
import struct
import sys
import subprocess

SECTOR = 2048


def check(iso_path, boot_path, kernel_path, elf_path):
    iso = pathlib.Path(iso_path).read_bytes()
    boot = pathlib.Path(boot_path).read_bytes()
    kernel = pathlib.Path(kernel_path).read_bytes()

    def sector(n):
        return iso[n * SECTOR:(n + 1) * SECTOR]

    assert len(iso) % SECTOR == 0
    assert sector(16)[:7] == b'\x01CD001\x01'
    assert sector(17)[:7] == b'\x00CD001\x01'
    assert sector(17)[1:31] == b'CD001\x01EL TORITO SPECIFICATION\x00'
    assert sector(17)[31:39] == bytes(8)
    assert sector(17)[7:30] == b'EL TORITO SPECIFICATION'
    assert sector(18)[:7] == b'\xffCD001\x01'
    assert struct.unpack_from('<I', sector(16), 80)[0] == len(iso) // SECTOR
    assert struct.unpack_from('>I', sector(16), 84)[0] == len(iso) // SECTOR
    assert struct.unpack_from('<H', sector(16), 128)[0] == SECTOR
    assert struct.unpack_from('<I', sector(17), 71)[0] == 20
    assert struct.unpack_from('<I', sector(16), 158)[0] == 22
    assert sector(19)[:10] == b'\x01\x00\x16\x00\x00\x00\x01\x00\x00\x00'
    catalog = sector(20)
    entry = catalog[:32]
    assert entry[0] == 1
    assert entry[30:32] == b'\x55\xaa'
    assert sum(struct.unpack('<16H', entry)) % 65536 == 0
    initial = catalog[32:64]
    assert initial[0] == 0x88
    assert initial[1] == 0
    assert struct.unpack_from('<H', initial, 6)[0] == 4
    boot_lba = struct.unpack_from('<I', initial, 8)[0]
    assert boot_lba == 32
    assert boot_lba * SECTOR < len(iso)
    assert len(boot) == 512 and boot[510:512] == b'\x55\xaa'
    assert sector(boot_lba)[:512] == boot
    dap_offset = boot.index(bytes.fromhex('10 00 01 00 00 00 00 10 21 00 00 00'))
    message_offset = boot.index(b'Quartzie: CD read failed')
    assert b'\xbe' + struct.pack('<H', 0x7c00 + dap_offset) in boot
    assert b'\xbe' + struct.pack('<H', 0x7c00 + message_offset) in boot
    assert b'\x8b\x36' + struct.pack('<H', 0x7c00 + dap_offset) not in boot
    symbols = {}
    for row in subprocess.check_output(['nm', '-n', elf_path], text=True).splitlines():
        parts = row.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    stage = symbols['pm_entry'] - 0x10000
    segment = kernel[stage:stage + 60]
    assert b'\xbf' + struct.pack('<I', symbols['__bss_start']) in segment
    assert b'\xb9' + struct.pack('<I', symbols['__bss_end']) in segment
    assert b'\x8b\x3d' + struct.pack('<I', symbols['__bss_start']) not in segment
    assert b'\x8b\x0d' + struct.pack('<I', symbols['__bss_end']) not in segment
    assert sector(boot_lba)[512:] == bytes(SECTOR - 512)
    assert iso[(boot_lba + 1) * SECTOR:(boot_lba + 1) * SECTOR + len(kernel)] == kernel
    assert iso[(boot_lba + 1) * SECTOR + len(kernel):(boot_lba + 81) * SECTOR] == bytes(80 * SECTOR - len(kernel))
    assert b'\x21\x00\x00\x00\x00\x00\x00\x00' in boot
    assert b'\x50\x00' in boot
    assert b'\xc7\x06\x16\x72\x80\x07' in kernel
    assert b'\xc7\x06\x18\x72\x38\x04' in kernel
    assert b'\xc7\x06\x14\x72\x00\x1e' in kernel
    assert b'\xb8\x80\x07' in kernel[:512]
    assert b'\xb8\x38\x04' in kernel[:512]
    assert b'QUARTZIE_0_25' in sector(16)
    assert b'QUARTZIE 0.25' in entry
    assert b'KERNEL32.DLL' in kernel
    assert b'runpe ' in kernel
    for symbol in ('cm_pe_load', 'cm_proc_launch_pe', 'cm_pe_error',
                   'cm_enter_userspace', 'cm_irq0_stub', 'cm_syscall_stub',
                   'cm_gp_stub', 'cm_pf_stub', 'cm_proc_initialize',
                   'cm_proc_interrupt', 'cm_proc_has_work', 'cm_irq_unexpected_stub', 'cm_user_binary_start', 'cm_user_binary_end'):
        assert symbol in symbols, f'Missing protected-mode process symbol: {symbol}'
    user_binary_start = symbols['cm_user_binary_start'] - 0x10000
    user_binary_end = symbols['cm_user_binary_end'] - 0x10000
    assert 0 < user_binary_end - user_binary_start <= 4096
    assert b'\xcd\x80' in kernel[user_binary_start:user_binary_end]
    assert b'\xfa' in kernel[user_binary_start:user_binary_end]
    assert b'\x0f\x22\xd8' in kernel or b'\x0f\x22\xc3' in kernel
    assert b'\xcf' in kernel[symbols['cm_enter_userspace'] - 0x10000:
                             symbols['cm_user_binary_start'] - 0x10000]

    folder = sector(22)
    names = []
    at = 0
    while folder[at]:
        entry_len = folder[at]
        name_len = folder[at + 32]
        names.append(folder[at + 33:at + 33 + name_len])
        at += entry_len
    assert b'BOOT.CAT;1' in names
    assert b'QUARTZIE.BIN;1' in names
    assert b'README.TXT;1' in names
    print('PASS: ISO 9660 filesystem and file records')
    print('PASS: SeaBIOS El Torito identifier and boot catalog checks')
    print('PASS: BIOS CD boot stage and DAP address operand')
    print('PASS: Protected mode kernel entry and BSS initialization operands')
    print('PASS: 1920x1080x32 VBE/Bochs mode-selection instructions and 7680-byte pitch')
    print('PASS: ring-3 IRET, CR3 switching, syscall gate, timer IRQ, unexpected-IRQ recovery and protection-fault stubs')
    print(f'ISO: {len(iso)} bytes')
    print(f'KERNEL: {len(kernel)} bytes')


if __name__ == '__main__':
    check(*sys.argv[1:])
