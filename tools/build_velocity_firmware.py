#!/usr/bin/env python3
"""Build Sampler v0.37 with the Linux JieLi compiler and its bundled SDK.

Compiler and linker flags follow firmware/sdk/Build.ps1 and tools/build_fw.py
in ferluht/fm1-sampler. Packaging uses the pinned upstream tools/pack.py.
"""

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


COMPILE_FLAGS = (
    '-target', 'pi32v2', '-integrated-as', '-mcpu=r3', '-mfprev1',
    '-Wuninitialized', '-Wno-invalid-noreturn', '-fno-common', '-flto',
    '-fallow-pointer-null', '-fprefer-gnu-section', '-femulated-tls',
    '-Wno-shift-negative-value', '-Wframe-larger-than=2560', '-fms-extensions',
    '-fno-unwind-tables', '-ffunction-sections', '-fdata-sections', '-fmessage-length=0',
    '-cl-single-precision-constant', '-mllvm', '-pi32v2-large-program=true',
    '-D__GCC_PI32V2__', '-O2', '-fwrapv', '-fno-fast-math', '-ffp-contract=off',
)
NATIVE_UNITS = {'transfer.c', 'midi_router.c', 'ctrl.c', 'panel.c'}
COMPACT_UNITS = {'project.c', 'transfer.c', 'samples.c', 'presets.c',
                 'midi_router.c', 'fm_factory.c', 'fm_library.c'}
LINK_FLAGS = (
    '-pi32v2-always-use-itblock=false', '-enable-ipra=true',
    '-pi32v2-merge-max-offset=4096', '-pi32v2-enable-simd=true', 'mcpu=r3',
    '-global-merge-on-const', '-inline-threshold=5', '-inline-max-allocated-size=32',
    '-inline-normal-into-special-section=true', '-pi32v2-enable-rep-memop',
    '-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar', '-emulated-tls',
    '-pi32v2-large-program=true', '-pi32v2-csync-before-rti=true',
)
REQUIRED_SYMBOLS = ('connectivity_wl82_bluetooth_start', 'audio_app_bluetooth_open',
                    'bt_get_local_name', 'bt_get_mac_addr')
APP_SECTIONS = ('text', 'data', 'ram0_data', 'cache_ram_data')
INPUT_SRAM_SYMBOLS = ('fm1_input_tick', 'sampler_adc_finish', 'sense_sample', 'sense_probe')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(tools, work, tool, *args):
    subprocess.run([tools / tool, *args], cwd=work, check=True)


def verify_sdk(root):
    manifest_file = root / 'sdk/manifest.json'
    manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
    if manifest['abi'] != 131072:
        raise SystemExit('Der Sampler benötigt das SDK für 128 Audioframes.')
    for name, expected in manifest['files'].items():
        if digest(root / name) != expected:
            raise SystemExit(f'Die Prüfsumme von {name} stimmt nicht.')
    return digest(manifest_file)


def compile_app(root, tools, work, sdk_digest):
    app = root / 'app'
    inputs = sorted(path for path in app.rglob('*') if path.is_file())
    flags = list(COMPILE_FLAGS)
    for directory in ('sdk/include', 'sdk/include/libc', 'app/synth', 'sdk/config', 'app', 'app/board'):
        flags.extend(('-I', root / directory))
    objects = []
    for source in inputs:
        if source.suffix not in ('.c', '.S') or app / 'synth' in source.parents:
            continue
        options = []
        if source.name in NATIVE_UNITS:
            options.append('-fno-lto')
        if source.name == 'panel.c':
            options.append('-fno-pic')
        if source.name in COMPACT_UNITS:
            options.append('-Oz')
        obj = work / f'{len(objects)}_{source.stem}.o'
        print(f'Kompiliere {source.relative_to(root)}', flush=True)
        run(tools, work, 'clang', *flags, *options, '-c', source, '-o', obj)
        objects.append(obj)

    version = (app / 'version.h').read_text(encoding='utf-8')
    match = re.search(r'(?m)^#define FM1_UPDATE_PRODUCT "(FM-1_8[0-9]{6})"$', version)
    if not match:
        raise SystemExit('FM1_UPDATE_PRODUCT fehlt oder ist ungültig.')
    product = match[1]
    identity = ['dx7', sdk_digest]
    identity.extend(f'{path.relative_to(root).as_posix()}:{digest(path)}' for path in inputs)
    identity_bytes = hashlib.sha256('\n'.join(identity).encode()).digest()
    identity_source = work / 'build_identity.c'
    identity_source.write_text(
        '#include "layout.h"\n'
        'const struct fm1_firmware_layout fm1_layout = {536576U,7U};\n'
        'const unsigned char fm1_build_id[32] = {' + ','.join(map(str, identity_bytes)) + '};\n'
        f'const unsigned char fm1_update_identity[27] = "{product}";\n',
        encoding='utf-8',
    )
    identity_object = work / 'build_identity.o'
    run(tools, work, 'clang', *flags, '-c', identity_source, '-o', identity_object)
    objects.append(identity_object)
    return objects, product


def link_app(sdk, tools, work, objects):
    linker = work / 'linker.ld'
    linker.write_text(
        (sdk / 'config/linker.ld.in').read_text(encoding='utf-8')
        .replace('@SBC_DECODER@', 'sbc_decoder\n')
        .replace('@ACL_TX_EXTRA@', ' + 1024')
        .replace('@ACL_RX_EXTRA@', ' + 4096'),
        encoding='utf-8',
    )
    elf = work / 'sampler.elf'
    print('Linke die Sampler-Firmware mit dem FM-1-SDK', flush=True)
    run(tools, work, 'ld', '--plugin', tools / 'LLVMgold.so', '-o', elf, *objects, '-flto',
        *(f'--plugin-opt={option}' for option in LINK_FLAGS), '--sort-common', '--start-group',
        '--whole-archive', sdk / 'lib/libfm1_engine.a', '--no-whole-archive',
        sdk / 'lib/libc.a', sdk / 'lib/libcompiler_rt.a', '--end-group',
        '--gc-sections', '-T', linker, '-Map=sdk.map',
        *(f'--undefined={name}' for name in REQUIRED_SYMBOLS))
    with elf.open('rb') as file:
        header = file.read(28)
    if header[:6] != b'\x7fELF\x01\x01' or int.from_bytes(header[24:28], 'little') != 0x02000120:
        raise SystemExit('Die Firmware hat einen unerwarteten Startpunkt.')
    map_text = (work / 'sdk.map').read_text(encoding='utf-8', errors='replace')
    heap = re.search(r'(?m)^\s*(0x[0-9a-fA-F]+)\s+_MALLOC_SIZE\s*=', map_text)
    if not heap or int(heap[1], 16) < 49152:
        raise SystemExit('Der Firmware stehen weniger als 48 KiB Laufzeit-Heap zur Verfügung.')
    symbols = subprocess.check_output([tools / 'nm', '-n', elf], cwd=work, text=True)
    for symbol in INPUT_SRAM_SYMBOLS:
        address = re.search(rf'(?m)^([0-9a-fA-F]+) [tT] {symbol}$', symbols)
        if not address or not 0x01C00000 <= int(address[1], 16) < 0x01C80000:
            raise SystemExit(f'Der Tasten-IRQ benötigt {symbol} im SRAM.')
    return elf


def main():
    root, tools, output = [Path(arg).resolve() for arg in sys.argv[1:]]
    work = output.parent / 'image'
    work.mkdir()
    sdk = root / 'sdk'
    objects, product = compile_app(root, tools, work, verify_sdk(root))
    elf = link_app(sdk, tools, work, objects)
    for section in APP_SECTIONS:
        run(tools, work, 'objcopy', '-O', 'binary', '-j', '.' + section,
            elf, work / f'{section}.bin')
    subprocess.run([sys.executable, root.parent / 'tools/pack.py', work,
                    sdk / 'tools/pack', output, product], check=True)
    print(f'SHA-256 {digest(output)}', flush=True)


if __name__ == '__main__':
    main()
