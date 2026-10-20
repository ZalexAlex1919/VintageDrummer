"""Validate source resource wiring; on Windows also inspect a newly built DLL."""
import argparse
import ctypes
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'vintage_drummer'


def resources():
    header = (SRC / 'resources/resource.h').read_text()
    ids = {name: int(value) for name, value in re.findall(r'#define (IDR_\w+) (\d+)', header)}
    cpp = (SRC / 'VintageDrummer.cpp').read_text()
    used = set(re.findall(r'\bIDR_\w+\b', cpp))
    if used != set(ids) or len(set(ids.values())) != len(ids):
        raise RuntimeError('Resource IDs are missing, unused or duplicated')
    rc = (SRC / 'resources/plugin.rc.in').read_text()
    entries = re.findall(r'^(IDR_\w+) RCDATA "@CMAKE_CURRENT_SOURCE_DIR@/(.+)"$', rc, re.M)
    if len(entries) != len(ids) or {name for name, _ in entries} != set(ids):
        raise RuntimeError('RC table does not match resource IDs')
    for name, filename in entries:
        data = (SRC / filename).read_bytes()
        suffix = Path(filename).suffix
        magic = {'.png': b'\x89PNG\r\n\x1a\n', '.wav': b'RIFF', '.mid': b'MThd'}[suffix]
        if not data.startswith(magic):
            raise RuntimeError(f'Invalid asset: {filename}')
        yield ids[name], data, filename


def check_dll(path, entries):
    if sys.platform != 'win32':
        raise RuntimeError('--plugin requires Windows x64 Python')
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 0x3c)[0]
    if data[:2] != b'MZ' or data[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', data, offset+4)[0] != 0x8664:
        raise RuntimeError('Expected an x64 Windows PE module')
    from ctypes import wintypes as w
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [w.LPCWSTR, w.HANDLE, w.DWORD]
    kernel.LoadLibraryExW.restype = w.HMODULE
    kernel.FindResourceW.argtypes = [w.HMODULE, w.LPCWSTR, w.LPCWSTR]
    kernel.FindResourceW.restype = w.HANDLE
    kernel.SizeofResource.argtypes = [w.HMODULE, w.HANDLE]
    kernel.SizeofResource.restype = w.DWORD
    kernel.LoadResource.argtypes = [w.HMODULE, w.HANDLE]
    kernel.LoadResource.restype = w.HANDLE
    kernel.LockResource.argtypes = [w.HANDLE]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [w.HMODULE]
    module = kernel.LoadLibraryExW(str(path.resolve()), None, 2)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        # MAKEINTRESOURCE pointers, not decimal strings.
        for number, original, label in entries:
            res = kernel.FindResourceW(module, ctypes.cast(number, w.LPCWSTR), ctypes.cast(10, w.LPCWSTR))
            if not res:
                raise RuntimeError(f'Missing embedded resource: {label}')
            size = kernel.SizeofResource(module, res)
            ptr = kernel.LockResource(kernel.LoadResource(module, res))
            if not ptr or ctypes.string_at(ptr, size) != original:
                raise RuntimeError(f'Embedded bytes differ: {label}')
        font = kernel.FindResourceW(module, 'ROBOTO-REGULAR.TTF', 'TTF')
        original_font = ROOT / '.deps/iPlug2/Examples/IPlugEffect/resources/fonts/Roboto-Regular.ttf'
        if not font:
            raise RuntimeError('Embedded Roboto font missing')
        ptr = kernel.LockResource(kernel.LoadResource(module, font))
        if ctypes.string_at(ptr, kernel.SizeofResource(module, font)) != original_font.read_bytes():
            raise RuntimeError('Embedded font bytes differ')
    finally:
        kernel.FreeLibrary(module)
    # Real loader check catches missing imports; this does not instantiate/process audio.
    plugin = ctypes.WinDLL(str(path.resolve()))
    for name in ('GetPluginFactory', 'InitDll', 'ExitDll'):
        getattr(plugin, name)
    print('PASS: x64 PE, VST3 exports, loader and byte-for-byte embedded resources')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plugin', type=Path)
    parser.add_argument('--dependencies', action='store_true')
    args = parser.parse_args()
    entries = list(resources())
    print(f'PASS: {len(entries)} resource IDs, paths and file signatures')
    if args.dependencies:
        lock = json.loads((ROOT / 'dependencies.lock.json').read_text())
        for key, path in [('iPlug2', ROOT / '.deps/iPlug2'),
                          ('VST3_SDK', ROOT / '.deps/iPlug2/Dependencies/IPlug/VST3_SDK')]:
            actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
            if actual != lock[key]['commit']:
                raise RuntimeError(f'{key} revision differs from lock')
        subprocess.run(['git', '-C', str(ROOT / '.deps/iPlug2'), 'diff', '--exit-code',
                        'HEAD', '--', '.', ':!Dependencies/IPlug/VST3_SDK'], check=True)
        sdk = ROOT / '.deps/iPlug2/Dependencies/IPlug/VST3_SDK'
        subprocess.run(['git', '-C', str(sdk), 'diff', '--exit-code', 'HEAD'], check=True)
        status = subprocess.check_output(['git', '-C', str(sdk), 'submodule', 'status'], text=True)
        if any(line.startswith(('-', '+', 'U')) for line in status.splitlines()
               if any(f' {name} ' in line for name in ('base', 'pluginterfaces', 'public.sdk', 'cmake'))):
            raise RuntimeError('VST3 SDK submodule revision mismatch')
        print('PASS: dependency revisions')
    if args.plugin:
        check_dll(args.plugin, entries)


if __name__ == '__main__':
    main()
