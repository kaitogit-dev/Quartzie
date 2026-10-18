import json
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from bootcheck import choose_firmware


def wait_ready(process, serial, timeout):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        data = serial.read_bytes() if serial.exists() else b''
        if b'QUARTZIE-DESKTOP-READY' in data:
            return data
        if process.poll() is not None:
            raise RuntimeError(f'QEMU exited with status {process.returncode}')
        time.sleep(0.1)
    raise RuntimeError('Desktop did not boot before mouse test')


def verify_mouse(image, usb=False):
    code, variables = choose_firmware()
    with tempfile.TemporaryDirectory() as folder:
        root = pathlib.Path(folder)
        nvram = root / 'vars.fd'
        shutil.copyfile(variables, nvram)
        serial = root / 'serial.log'
        qmp = root / 'qmp.sock'
        command = [
            'qemu-system-x86_64', '-machine', 'q35,accel=tcg',
            '-m', '512', '-vga', 'std', '-display', 'none',
            '-nic', 'none', '-no-reboot',
            '-drive', f'if=pflash,format=raw,readonly=on,file={code}',
            '-drive', f'if=pflash,format=raw,file={nvram}',
            '-serial', f'file:{serial}',
            '-qmp', f'unix:{qmp},server=on,wait=off',
            '-cdrom', str(image), '-boot', 'order=d',
        ]
        if usb:\n            command += ['-usb', '-device', 'usb-tablet']\n        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            before = wait_ready(process, serial, 35)
            print(before.decode('ascii', errors='replace'))
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
                sock.settimeout(5)
                sock.connect(str(qmp))
                sock.recv(4096)
                sock.sendall(b'{"execute":"qmp_capabilities"}\r\n')
                response = sock.recv(4096)
                if b'"return"' not in response:
                    raise RuntimeError(f'QMP capabilities failed: {response!r}')
                command = {'execute': 'input-send-event', 'arguments': {'events': [
                    {'type': 'rel', 'data': {'axis': 'x', 'value': 90}},
                    {'type': 'rel', 'data': {'axis': 'y', 'value': 40}},
                ]}}
                sock.sendall(json.dumps(command).encode('ascii') + b'\r\n')
                response = sock.recv(4096)
                if b'"return"' not in response:
                    raise RuntimeError(f'QMP mouse injection failed: {response!r}')
            end = time.monotonic() + 6
            while time.monotonic() < end:
                data = serial.read_bytes()
                if b'QUARTZIE-POINTER-MOVED' in data:
                    print('PASS: QEMU mouse movement reached Quartzie')
                    return
                time.sleep(0.15)
            raise RuntimeError(f'Pointer never moved after QMP mouse event: {serial.read_text(errors="replace")}')
        finally:
            process.terminate()
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()


if __name__ == '__main__':
    verify_mouse(pathlib.Path(sys.argv[1]).resolve(), len(sys.argv) > 2 and sys.argv[2] == 'usb')
