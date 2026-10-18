import pathlib
import shutil
import subprocess
import sys
import tempfile


def create(esp_path, iso_path):
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        shutil.copyfile(esp_path, root / 'efi.img')
        subprocess.run([
            'xorriso', '-as', 'mkisofs', '-iso-level', '3',
            '-V', 'QUARTZIE_X64_UEFI', '-e', 'efi.img',
            '-no-emul-boot', '-o', str(pathlib.Path(iso_path).resolve()),
            str(root),
        ], check=True)


if __name__ == '__main__':
    create(sys.argv[1], sys.argv[2])
