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


def send_vnc_mouse(click_start=False):
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
        for i in range(48):
            x = width // 3 + (i * 13) % (width // 3)
            y = height // 3 + (i * 7) % (height // 3)
            stream.sendall(struct.pack('>BBHH', 5, 0, x, y))
            time.sleep(0.018)
        if click_start:
            x = (width - 555) // 2 + 37
            y = height - 89 + 34
            stream.sendall(struct.pack('>BBHH', 5, 0, x, y))
            time.sleep(0.65)
            stream.sendall(struct.pack('>BBHH', 5, 1, x, y))
            time.sleep(0.75)
            stream.sendall(struct.pack('>BBHH', 5, 0, x, y))
            time.sleep(0.25)
        print('VNC pointer events sent at', width, height)


def send_qmp_start_click(socket_path):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as stream:
        stream.settimeout(6)
        stream.connect(str(socket_path))
        stream.recv(4096)
        stream.sendall(b'{"execute":"qmp_capabilities"}\r\n')
        stream.recv(4096)
        def event(events):
            command = {'execute': 'input-send-event', 'arguments': {'events': events}}
            stream.sendall(json.dumps(command).encode('ascii') + b'\r\n')
            response = stream.recv(4096)
            if b'"return"' not in response:
                raise RuntimeError(f'Mouse event failed: {response!r}')
        event([
            {'type': 'abs', 'data': {'axis': 'x', 'value': round(719 * 32767 / 1919)}},
            {'type': 'abs', 'data': {'axis': 'y', 'value': round(1025 * 32767 / 1079)}},
        ])
        time.sleep(0.45)
        event([{'type': 'btn', 'data': {'button': 'left', 'down': True}}])
        time.sleep(0.7)
        event([{'type': 'btn', 'data': {'button': 'left', 'down': False}}])


def verify_mouse(image, usb=False, host_cursor=False):
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
        if host_cursor:
            command += ['-fw_cfg', 'name=opt/quartzie/hostcursor,string=1']
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            before = wait_ready(process, serial, 35)
            print(before.decode('ascii', errors='replace'))
            if host_cursor and b'QUARTZIE-HOST-CURSOR' not in before:
                raise RuntimeError('Host cursor mode was not selected')
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
                    sock.recv(4096)
                    sock.sendall(b'{"execute":"query-mice"}\r\n')
                    selected = json.loads(sock.recv(4096).decode('ascii'))['return']
                    if not any(item['current'] and item['absolute'] for item in selected):
                        raise RuntimeError(f'USB tablet was not selected: {selected!r}')
                    print('Selected USB tablet:', selected)
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
            send_vnc_mouse(click_start=host_cursor)
            if host_cursor:
                send_qmp_start_click(qmp)
            end = time.monotonic() + 6
            while time.monotonic() < end:
                data = serial.read_bytes()
                if b'QUARTZIE-POINTER-CONTINUOUS' in data and (not host_cursor or b'QUARTZIE-START-CLICK' in data):
                    print('PASS: QEMU cursor received movement and correct Start button click')
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
    verify_mouse(pathlib.Path(sys.argv[1]).resolve(), len(sys.argv) > 2 and sys.argv[2] in ('usb', 'usb-host'), len(sys.argv) > 2 and sys.argv[2] == 'usb-host')
