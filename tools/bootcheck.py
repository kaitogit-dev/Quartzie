import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
OVMF_DIR = pathlib.Path('/usr/share/OVMF')


def choose_firmware():
    for code, variables in (
        ('OVMF_CODE_4M.fd', 'OVMF_VARS_4M.fd'),
        ('OVMF_CODE.fd', 'OVMF_VARS.fd'),
    ):
        if (OVMF_DIR / code).exists() and (OVMF_DIR / variables).exists():
            return OVMF_DIR / code, OVMF_DIR / variables
    raise RuntimeError('OVMF firmware files not found')


def run_image(image, kind):
    code, variables = choose_firmware()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        nvram = tmp / 'vars.fd'
        shutil.copyfile(variables, nvram)
        serial = tmp / 'serial.log'
        monitor = tmp / 'monitor.sock'
        command = [
            'qemu-system-x86_64', '-machine', 'q35,accel=tcg',
            '-m', '512', '-vga', 'std', '-display', 'none',
            '-net', 'none', '-no-reboot',
            '-drive', f'if=pflash,format=raw,readonly=on,file={code}',
            '-drive', f'if=pflash,format=raw,file={nvram}',
            '-serial', f'file:{serial}',
            '-monitor', f'unix:{monitor},server,nowait',
        ]
        if kind == 'iso':
            command += ['-cdrom', str(image), '-boot', 'order=d']
        else:
            command += ['-drive', f'file={image},format=raw,if=virtio', '-boot', 'order=c']
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            for _ in range(180):
                if serial.exists() and b'QUARTZIE-DESKTOP-READY' in serial.read_bytes():
                    if b'QUARTZIE-WIN32-READY' not in serial.read_bytes() or b'Quartzie Win32 PE64 app' not in serial.read_bytes():
                        raise RuntimeError(f'{kind} did not execute imported Win32 APIs: {serial.read_text(errors="replace")}')
                    if b'QUARTZIE-MODE-1920x1080' not in serial.read_bytes():
                        raise RuntimeError(f'{kind} did not select 1920x1080: {serial.read_text(errors="replace")}')
                    snapshot = ROOT / 'build' / f'{kind}-screen.ppm'
                    if monitor.exists():
                        try:
                            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as conn:
                                conn.settimeout(3)
                                conn.connect(str(monitor))
                                conn.recv(2048)
                                conn.sendall(f'screendump {snapshot}\n'.encode('ascii'))
                        except OSError:
                            pass
                    print(f'PASS: OVMF reached x64 desktop from {kind}')
                    return
                if process.poll() is not None:
                    raise RuntimeError(f'QEMU exited with status {process.returncode}')
                time.sleep(0.25)
            raise RuntimeError(f'Timed out booting {kind}, serial output: {serial.read_text(errors="replace") if serial.exists() else "none"}')
        finally:
            process.terminate()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


if __name__ == '__main__':
    run_image((ROOT / sys.argv[1]).resolve(), sys.argv[2])
