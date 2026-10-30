#!/usr/bin/env python3
"""Linux/wine port of fm1_open/sdk/Build.ps1 (-FirmwareOnly). NOT yet run against the real toolchain.

usage: build_fw.py <fm1_open dir> <JL toolchain pi32 dir, e.g. ~/fm1/wine/drive_c/JL/pi32>
Needs: wine, the JieLi 2.5.3 toolchain installed, python3. Output: <fm1_open>/build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc
"""
import os, sys, subprocess, hashlib, re, glob, json

root = os.path.abspath(sys.argv[1]); tc = os.path.abspath(sys.argv[2])
bin_ = os.path.join(tc, 'bin')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pack

env = dict(os.environ)
env.setdefault('WINEPREFIX', os.path.expanduser('~/fm1/wine')); env['WINEDEBUG'] = '-all'

def wine(exe, args, cwd, check=True):
    r = subprocess.run(['wine', os.path.join(bin_, exe)] + args, cwd=cwd, env=env, capture_output=True, text=True)
    if r.stdout.strip(): print(r.stdout.strip())
    if r.stderr.strip(): print(r.stderr.strip())
    if check and r.returncode:
        raise SystemExit('%s failed (%d)' % (exe, r.returncode))
    return r

# ---- version / product ------------------------------------------------------------
ver = open(os.path.join(root, 'app/version.h')).read()
release = re.search(r'#define FM1_RELEASE_VERSION "(V\d+)"', ver).group(1)
product = re.search(r'#define FM1_UPDATE_PRODUCT "(FM-1_8\d{6})"', ver).group(1)
print('building FM1 Open', release, product)

build = os.path.join(root, 'build/dx7'); obj = os.path.join(build, 'obj'); gen = os.path.join(build, 'generated')
os.makedirs(obj, exist_ok=True); os.makedirs(gen, exist_ok=True)
for f in glob.glob(obj + '/*'): os.remove(f)

flags = ['-target', 'pi32v2', '-integrated-as', '-mcpu=r3', '-mfprev1', '-Wuninitialized', '-Wno-invalid-noreturn',
         '-fno-common', '-flto', '-fallow-pointer-null', '-fprefer-gnu-section', '-femulated-tls',
         '-Wno-shift-negative-value', '-Wframe-larger-than=2560', '-fms-extensions', '-fno-unwind-tables',
         '-ffunction-sections', '-fdata-sections', '-fmessage-length=0', '-cl-single-precision-constant',
         '-mllvm', '-pi32v2-large-program=true', '-D__GCC_PI32V2__', '-O2']
# relative include paths (cwd = build dir) keep wine's clang away from unix-path parsing
for d in ('sdk/include', 'sdk/include/libc', 'app/synth', 'sdk/config', 'app', 'app/board'):
    flags.append('-I' + os.path.relpath(os.path.join(root, d), build))

sources = []
for dp, dn, fn in os.walk(os.path.join(root, 'app')):
    if os.path.abspath(dp).startswith(os.path.join(root, 'app/synth')): continue
    for f in fn:
        if f.endswith(('.c', '.S')): sources.append(os.path.relpath(os.path.join(dp, f), root).replace('\\', '/'))
sources.sort()
OZ = {'app/project.c', 'app/transfer.c', 'app/samples.c', 'app/presets.c', 'app/midi_router.c',
      'app/fm_factory.c', 'app/fm_library.c'}
objects = []
for i, s in enumerate(sources):
    out = 'obj/%d_%s.obj' % (i, re.sub(r'[^a-zA-Z0-9._-]', '_', s))
    opts = list(flags)
    if s in ('app/transfer.c', 'app/midi_router.c'): opts.append('-fno-lto')
    if s == 'app/board/panel.c': opts += ['-fno-lto', '-fno-pic']
    opts += ['-fwrapv', '-fno-fast-math', '-ffp-contract=off']
    if s in OZ: opts.append('-Oz')
    if s == 'app/ctrl.c': opts.append('-fno-lto')
    print('compile', s)
    wine('clang.exe', opts + ['-MMD', '-MF', out + '.d', '-c', os.path.relpath(os.path.join(root, s), build), '-o', out], build)
    objects.append(out)

# ---- build identity ---------------------------------------------------------------------
h = hashlib.sha256()
for dp, dn, fn in sorted(os.walk(os.path.join(root, 'app'))):
    for f in sorted(fn): h.update(open(os.path.join(dp, f), 'rb').read())
ident = h.digest()
user_bytes, family = 536576, 7
open(os.path.join(gen, 'build_identity.c'), 'w').write(
    '#include "layout.h"\nconst struct fm1_firmware_layout fm1_layout = {%dU,%dU};\n'
    'const unsigned char fm1_build_id[32] = {%s};\nconst unsigned char fm1_update_identity[27] = "%s";\n'
    % (user_bytes, family, ','.join('0x%02x' % b for b in ident), product))
wine('clang.exe', flags + ['-c', 'generated/build_identity.c', '-o', 'obj/build_identity.obj'], build)
objects.append('obj/build_identity.obj')

# ---- linker script -------------------------------------------------------------------------
ld = open(os.path.join(root, 'sdk/config/linker.ld.in')).read()
ld = ld.replace('@SBC_DECODER@', 'sbc_decoder\n').replace('@ACL_TX_EXTRA@', ' + 1024').replace('@ACL_RX_EXTRA@', ' + 4096')
open(os.path.join(gen, 'linker'), 'w').write(ld)
link = ['--plugin', 'C:\\JL\\pi32\\bin\\LLVMgold.dll' if False else os.path.join(bin_, 'LLVMgold.dll'), '-o', 'sdk.elf'] + objects + [
    '-flto', '--plugin-opt=-pi32v2-always-use-itblock=false', '--plugin-opt=-enable-ipra=true',
    '--plugin-opt=-pi32v2-merge-max-offset=4096', '--plugin-opt=-pi32v2-enable-simd=true', '--plugin-opt=mcpu=r3',
    '--plugin-opt=-global-merge-on-const', '--plugin-opt=-inline-threshold=5', '--plugin-opt=-inline-max-allocated-size=32',
    '--plugin-opt=-inline-normal-into-special-section=true',
    '--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar',
    '--plugin-opt=-pi32v2-enable-rep-memop', '--plugin-opt=-emulated-tls', '--sort-common',
    '--plugin-opt=-pi32v2-large-program=true', '--plugin-opt=-pi32v2-csync-before-rti=true', '--start-group',
    '--whole-archive', '../../sdk/lib/libfm1_engine.a', '--no-whole-archive',
    '../../sdk/lib/libc.a', '../../sdk/lib/libcompiler_rt.a', '--end-group', '--gc-sections', '-Tgenerated/linker',
    '-M=sdk.map', '--undefined=connectivity_wl82_bluetooth_start', '--undefined=audio_app_bluetooth_open',
    '--undefined=bt_get_local_name', '--undefined=bt_get_mac_addr']
print('link')
wine('pi32v2-ld.exe', link, build)

# ---- package -------------------------------------------------------------------------------------
elf = os.path.join(build, 'sdk.elf')
data = open(elf, 'rb').read()
entry = int.from_bytes(data[24:28], 'little')
if entry != 0x02000120: raise SystemExit('unexpected entry point 0x%x (expected 0x02000120)' % entry)
secs = os.path.join(gen, 'sections'); os.makedirs(secs, exist_ok=True)
for s in ('text', 'data', 'ram0_data', 'cache_ram_data'):
    wine('llvm-objcopy.exe', ['-O', 'binary', '-j', '.' + s, os.path.relpath(elf, gen), 'sections/%s.bin' % s], gen)
app = b''.join(open(os.path.join(secs, s + '.bin'), 'rb').read() for s in ('text', 'data', 'ram0_data', 'cache_ram_data'))
pkg, size, limit = pack.create(app, os.path.join(root, 'sdk/tools/pack'), product)
outp = os.path.join(gen, 'FM1_OFFICIAL_OPEN.fwsc'); open(outp, 'wb').write(pkg)
print('OK  app image %d bytes (limit %d), package %d bytes -> %s' % (len(app), limit, len(pkg), outp))
print('sha256', hashlib.sha256(pkg).hexdigest())
page = os.path.join(root, 'build/FM1-DAW-Workbench.html')
subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'make_workbench.py'), root, outp, ident.hex(), page], check=True)
print('FLASH PAGE:', page)
