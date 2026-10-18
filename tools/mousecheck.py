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


def receive_exact(stream, length):
    data = bytearray()
    while len(data) < length:
        fragment = stream.recv(length - len(data))
        if not fragment:
            raise RuntimeError('VNC connection closed unexpectedly')
        data.extend(fragment)
    return bytes(data)


def send_vnc_mouse():
    import struct
    with socket.create_connection(('127.0.0.1', 5909), timeout=5) as stream:
        stream.settimeout(5)
        version = receive_exact(stream, 12)
        if not version.startswith(b'RFB '):
            raise RuntimeError(f'Invalid VNC banner: {version!r}')
        stream.sendall(b'RFB 003.008\n')
        count = receive_exact(stream, 1)[0]
        if count == 0:
            length = struct.unpack('>I', receive_exact(stream, 4))[0]
            raise RuntimeError(receive_exact(stream, length).decode(errors='replace'))
        methods = receive_exact(stream, count)
        if 1 not in methods:
            raise RuntimeError(f'VNC authentication required: {methods!r}')
        stream.sendall(b'\x01')
        if receive_exact(stream, 4) != b'\x00\x00\x00\x00':
            raise RuntimeError('VNC rejected authentication')
        stream.sendall(b'\x01')
        header = receive_exact(stream, 24)
        title_length = struct.unpack_from('>I', header, 20)[0]
        receive_exact(stream, title_length)
        width, height = struct.unpack_from('>HH', header, 0)
        for x, y in ((width // 2, height // 2), (width // 3, height // 3),
                     (width // 2 + 160, height // 2 + 90),
                     (width // 2 + 210, height // 2 + 130)):
            stream.sendall(struct.pack('>BBHH', 5, 0, x, y))
            time.sleep(0.2)
        print('VNC pointer events sent at', width, height)


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
            '-m', '512', '-vga', 'std', '-display', 'none', '-vnc', '127.0.0.1:9',
            '-nic', 'none', '-no-reboot',
            '-drive', f'if=pflash,format=raw,readonly=on,file={code}',
            '-drive', f'if=pflash,format=raw,file={nvram}',
            '-serial', f'file:{serial}',
            '-qmp', f'unix:{qmp},server=on,wait=off',
            '-cdrom', str(image), '-boot', 'order=d',
        ]
        if usb:
            command += ['-usb', '-device', 'usb-tablet']
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
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
                sock.sendall(b'{"execute":"query-mice"}\r\n')
                devices = json.loads(sock.recv(4096).decode('ascii'))['return']
                print('QMP mice:', devices)
                if usb:
                    tablet = next((item['index'] for item in devices if item['absolute']), None)
                    if tablet is None:
                        raise RuntimeError('QEMU did not register the USB tablet')
                    selection = {'execute': 'human-monitor-command', 'arguments': {'command-line': f'mouse_set {tablet}'}}
                    sock.sendall(json.dumps(selection).encode('ascii') + b'\r\n')
                    print('Tablet selection:', sock.recv(4096))
                    sock.sendall(b'{"execute":"query-mice"}\r\n')
                    print('Selected mice:', sock.recv(4096))
                events = ([
                    {'type': 'abs', 'data': {'axis': 'x', 'value': 24000}},
                    {'type': 'abs', 'data': {'axis': 'y', 'value': 33000}},
                ] if usb else [
                    {'type': 'rel', 'data': {'axis': 'x', 'value': 90}},
                    {'type': 'rel', 'data': {'axis': 'y', 'value': 40}},
                ])
                command = {'execute': 'input-send-event', 'arguments': {'events': events}}
                sock.sendall(json.dumps(command).encode('ascii') + b'\r\n')
                response = sock.recv(4096)
                if b'"return"' not in response:
                    raise RuntimeError(f'QMP mouse injection failed: {response!r}')
                if not usb:
                    extra = {'execute': 'human-monitor-command', 'arguments': {'command-line': 'mouse_move 60 25'}}
                    sock.sendall(json.dumps(extra).encode('ascii') + b'\r\n')
                    print('HMP mouse move:', sock.recv(4096))
            send_vnc_mouse()
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
