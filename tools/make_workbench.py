#!/usr/bin/env python3
"""Build the integrated FM-1 workbench page (firmware embedded) like tools/Build-Web.ps1 -PackageJson does.
usage: make_workbench.py <fm1_open dir> <package.fwsc> <build_id_hex64> <out.html> [source_url]
The browser simulator is replaced by a stub (no wasm toolchain here), so the page flashes/updates but its Preview tab is empty.
"""
import sys, os, json, base64, hashlib, html, re
root, fw, build_id, out = sys.argv[1:5]
src_url = sys.argv[5] if len(sys.argv) > 5 else 'https://example.invalid/fm1-looper-source.zip'
read = lambda p, m='r': open(os.path.join(root, p), m + ('' if 'b' in m else ''), **({} if 'b' in m else {'encoding': 'utf-8'})).read()
ver = read('app/version.h')
release = re.search(r'#define FM1_RELEASE_VERSION "(V\d+)"', ver).group(1)
product = re.search(r'#define FM1_UPDATE_PRODUCT "(FM-1_8\d{6})"', ver).group(1)
data = open(fw, 'rb').read()
stock = read('sdk/firmware/FM-1_015.fwsc', 'rb'); stock_hash = hashlib.sha256(stock).hexdigest()
assert stock_hash == 'db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a', 'stock recovery image mismatch'
assert re.fullmatch(r'[0-9a-f]{64}', build_id)
pkg = dict(schema=1, product=product, profile='dx7', family=7, release_version=release, build_id=build_id,
           sha256=hashlib.sha256(data).hexdigest(), bytes=len(data),
           loader_sha256=hashlib.sha256(read('sdk/tools/pack/ota.bin', 'rb')).hexdigest(), hardware_validated=False,
           data=base64.b64encode(data).decode(),
           stock=dict(schema=1, kind='stock', product='FM-1_015', profile='M-VAVE Original V15', family=7, release_version='V15',
                      build_id=stock_hash, sha256=stock_hash, bytes=len(stock),
                      loader_sha256='ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541',
                      data=base64.b64encode(stock).decode()))
sim = dict(schema=1, release_version=release, sha256='0' * 64, source_sha256='0' * 64, sample_bank='none', data='')
notices = ['APP: GPLv3 with the FM1 SDK linking exception; no warranty. SDK and firmware resources have separate redistribution terms in THIRD_PARTY.md. Source: ' + src_url,
           'FM-1 V15 recovery firmware: https://www.m-vave.com/download (PC Firmware / FM-1 V15). Distributed by M-VAVE under Apache-2.0.']
for f in ('THIRD_PARTY.md', 'LICENSE.md'): notices.append('\n===== %s =====\n%s' % (f, read(f)))
page = read('sdk/Update.html.in')
rep = {'/*FM1_SOURCE_URL*/': html.escape(src_url), '/*FM1_PAYLOAD*/': 'const FM1_PACKAGE = ' + json.dumps(pkg, separators=(',', ':')) + ';',
       '/*FM1_SIMULATOR*/': 'const FM1_SIMULATOR = ' + json.dumps(sim, separators=(',', ':')) + ';',
       '/*FM1_I18N*/': read('sdk/I18n.js'), '/*FM1_SCRIPT*/': read('sdk/WebUpdate.js'), '/*FM1_SIM_SCRIPT*/': read('sdk/Simulator.js'),
       '/*FM1_SAMPLE_SCRIPT*/': read('sdk/Samples.js'), '/*FM1_NOTICES*/': html.escape('\n'.join(notices))}
for k, v in rep.items(): page = page.replace(k, v)
assert not re.search(r'/\*FM1_[A-Z_]+\*/', page), 'unexpanded placeholder'
open(out, 'w', encoding='utf-8').write(page)
print('wrote', out, len(page), 'bytes; package', pkg['bytes'], 'sha256', pkg['sha256'][:16])
