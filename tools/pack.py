#!/usr/bin/env python3
"""Linux port of fm1_open/sdk/Package.cs (MIT, kagaimiq jl-misctools + M-VAVE).

usage: pack.py <sdk.elf-sections-dir> <resources-dir> <out.fwsc> <FM-1_8xxxxxx>
  where sections-dir holds text.bin data.bin ram0_data.bin cache_ram_data.bin
"""
import re
import sys
import os

AREA, ENTRY, APPDATA = 0x4000, 0x02000120, 0x120
FLASH_LIMIT, USER_SIZE, BT_SIZE, MIN_VM = 0xFF000, 536576, 4096, 32768
PUBLIC = 0x980F


def _table():
    t = []
    for i in range(256):
        v = i << 8
        for _ in range(8):
            v = ((v << 1) ^ (0x1021 if v & 0x8000 else 0)) & 0xFFFF
        t.append(v)
    return t


T = _table()


def crc(b, begin, length):
    v = 0
    for x in b[begin:begin + length]:
        v = ((v << 8) ^ T[(v >> 8) ^ x]) & 0xFFFF
    return v


def cipher(b, begin, length, state):
    for i in range(begin, begin + length):
        b[i] ^= state & 0xFF
        state = ((state << 1) ^ (0x1021 if state & 0x8000 else 0)) & 0xFFFF


def roundup(n, a):
    return (n + a - 1) & -a


class Image:
    def __init__(self, size, fill):
        self.b = bytearray([fill]) * size

    def number(self, off, value, width):
        for n in range(width):
            self.b[off + n] = (value >> (8 * n)) & 0xFF

    def copy(self, off, data):
        self.b[off:off + len(data)] = data

    def text(self, off, s):
        self.copy(off, s.encode('ascii'))

    def checksum(self, field, off, size):
        self.number(field, crc(self.b, off, size), 2)

    def record(self, off, name, c, addr, size, attrs, last, extra):
        self.b[off:off + 32] = bytes(32)
        self.number(off + 2, c, 2)
        self.number(off + 4, addr, 4)
        self.number(off + 8, size, 4)
        self.number(off + 12, attrs, 2)
        self.number(off + 14, last, 2)
        for n in range(16, 32):
            self.b[off + n] = 255
        self.text(off + 16, name)
        self.b[off + 16 + len(name)] = 0
        if extra is not None:
            self.number(off + 28, extra, 4)
        self.checksum(off, off + 2, 30)


def u16(b, o):
    return b[o] | b[o + 1] << 8


def i32(b, o):
    return int.from_bytes(b[o:o + 4], 'little', signed=True)


def resource(d, name, size):
    b = open(os.path.join(d, name), 'rb').read()
    if len(b) != size:
        raise ValueError('Unexpected FM1 resource: ' + name)
    return b


def encode_boot(plain):
    r = bytearray(plain)
    banks = u16(plain, 0)
    if banks < 1 or banks > len(plain) // 16:
        raise ValueError('Invalid boot directory.')
    prev = banks * 16
    for i in range(banks):
        h = i * 16
        length, start = u16(plain, h + 2), i32(plain, h + 8)
        if (length == 0 or start < prev or start > len(plain) - length or
                crc(plain, h, 14) != u16(plain, h + 14) or
                crc(plain, start, length) != u16(plain, h + 12)):
            raise ValueError('Invalid boot bank.')
        cipher(r, h, 16, 0xFFFF)
        cipher(r, start, length, 0xFFFF)
        prev = start + length
    return bytes(r)


def build_flash(app, d, end, vm_end):
    boot = resource(d, 'uboot.boot', 14488)
    config = resource(d, 'boot-config.bin', 721)
    tool = resource(d, 'cfg_tool.bin', 383)
    img, st = Image(end, 255), Image(34 + len(config), 0)
    for bit in range(16):
        st.b[bit] = 16
        st.b[16 + bit] = 16 ^ (255 if ((PUBLIC >> bit) & 1) == 0 else 0)
    st.checksum(32, 0, 32)
    st.copy(34, config)
    img.copy(0, bytes([0, 0, 0x20, 2, 0x2f, 0x10, 0x4c, 0xc9, 0, 0xf0, 0x0f, 0, 0x10, 0x10, 0xa4, 0xff,
                       0x4e, 0x5d, 0x0b, 0x41, 0xe0, 0xcd, 0x78, 0x1d, 0xc8, 0x56, 0x41, 0x7f, 0xb3, 0x67, 0xcf, 0x9f]))
    img.checksum(0, 2, 30)
    config_at = 160 + len(boot)
    img.record(32, 'uboot.boot', crc(boot, 0, len(boot)), 160, len(boot), 0, 0, None)
    img.record(64, 'isd_config.ini', crc(st.b, 0, len(st.b)), config_at, len(st.b), 0x8002, 0, None)
    img.record(96, 'app_dir_head', 65535, AREA, 0xFFFFFFFF, 0xff81, 0, None)
    img.record(128, 'key_mac', 65535, FLASH_LIMIT, 4096, 0x0112, 65535, None)
    for p in range(0, 160, 32):
        cipher(img.b, p, 32, 0xFFFF)
    img.copy(160, encode_boot(boot))
    img.copy(config_at, st.b)
    payload = APPDATA + len(app) + len(tool)
    a = AREA + APPDATA
    img.copy(a, app)
    img.copy(a + len(app), tool)
    img.record(AREA + 32, 'app.bin', crc(app, 0, len(app)), APPDATA, len(app), 0xff82, 0, None)
    img.record(AREA + 64, 'cfg_tool.bin', crc(tool, 0, len(tool)), APPDATA + len(app), len(tool), 0xff82, 0, None)
    img.record(AREA + 96, 'VM', 65535, end, vm_end - end, 0x8112, 0, MIN_VM)
    img.record(AREA + 128, 'PRCT', 65535, 0, end, 0x8292, 0, 0x45444f43)
    img.record(AREA + 160, 'BTIF', 65535, vm_end, BT_SIZE, 0x8192, 1, 0x4f545541)
    img.record(AREA, 'app_area_head', crc(img.b, AREA + 32, payload - 32), ENTRY, payload, 0xff83, 0, None)
    reserved = AREA + payload
    img.record(reserved + 32, 'USER', 65535, FLASH_LIMIT - USER_SIZE, USER_SIZE, 0x8192, 1, 0x4f545541)
    img.record(reserved, 'EXT_RESERVED', crc(img.b, reserved + 32, 32), 32, 64, 0xff93, 1, None)
    for p in range(0, payload + 64, 32):
        cipher(img.b, AREA + p, min(32, payload + 64 - p), PUBLIC ^ (p >> 2))
    return img


def wrap(flash, loader, product):
    if not 1 <= len(loader) < 32768:
        raise ValueError('Invalid FM1 update loader.')
    loader_at = 1024 + roundup(len(flash.b), 32)
    total = loader_at + roundup(len(loader), 32)
    u = Image(total, 255)
    u.b[0:224] = bytes(224)
    files, names = [bytes(flash.b), loader], ['flash.bin', 'ota.bin']
    for i, f in enumerate(files):
        r = 64 + i * 80
        u.number(r, 0 if i == 0 else 100, 2)
        u.number(r + 2, i, 2)
        u.number(r + 4, crc(f, 0, len(f)), 2)
        u.number(r + 8, 1024 if i == 0 else loader_at, 4)
        u.number(r + 12, len(f), 4)
        u.number(r + 16, roundup(len(f), 32), 4)
        u.text(r + 64, names[i])
        cipher(u.b, r, 80, 0xFFFF)
    u.number(4, total, 4)
    u.number(8, 2, 2)
    u.number(10, 4, 2)
    u.number(12, 512, 4)
    u.text(16, 'AC791N')
    u.checksum(2, 64, 160)
    u.checksum(0, 2, 62)
    cipher(u.b, 0, 64, 0xFFFF)
    u.copy(1024, flash.b)
    u.copy(loader_at, loader)
    out = bytearray()
    for s in range(total):
        out.append(u.b[s])
        if s < 940 and (s + 1) % 47 == 0:
            idx = s // 47
            out.append((ord(product[idx]) + idx + 1) & 0xFF if idx < len(product) else 125)
    return bytes(out)


def create(app, resources, product):
    if not re.fullmatch(r'FM-1_8[0-9]{6}', product):
        raise ValueError('Invalid FM1 update identity.')
    limit = FLASH_LIMIT - USER_SIZE - BT_SIZE - MIN_VM - AREA - APPDATA - 383 - 64
    if not app or len(app) > limit:
        raise ValueError('Application does not fit the FM1 layout (%d > %d).' % (len(app), limit))
    size = roundup(AREA + APPDATA + len(app) + 383 + 64, 4096)
    user_at = FLASH_LIMIT - USER_SIZE
    vm_end = user_at - BT_SIZE
    if size > vm_end - MIN_VM:
        raise ValueError('Insufficient FM1 settings space.')
    flash = build_flash(app, resources, size, vm_end)
    return wrap(flash, open(os.path.join(resources, 'ota.bin'), 'rb').read(), product), size, limit


if __name__ == '__main__':
    sec, res, out, product = sys.argv[1:5]
    app = b''.join(open(os.path.join(sec, s + '.bin'), 'rb').read()
                   for s in ('text', 'data', 'ram0_data', 'cache_ram_data'))
    pkg, size, limit = create(app, res, product)
    open(out, 'wb').write(pkg)
    print('app image %d bytes (limit %d), flash image %d, package %d' % (len(app), limit, size, len(pkg)))
