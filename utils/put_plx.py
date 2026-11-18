#!/usr/bin/env python3
import struct, sys

if len(sys.argv) != 4:
    print("Usage: put_plx.py <disk.img> <prog.plx> <name>")
    sys.exit(1)

img_path, plx_path, name = sys.argv[1], sys.argv[2], sys.argv[3].lstrip("/")
data = open(plx_path, "rb").read()
assert 0 < len(data) <= 1024, "File must be 1..1024 bytes"
name_b = name.encode("ascii")

with open(img_path, "r+b") as f:
    def rd(blk):
        f.seek(blk * 1024); return bytearray(f.read(1024))
    def wr(blk, buf):
        f.seek(blk * 1024); f.write(buf)

    sb = rd(1)
    assert struct.unpack_from("<H", sb, 0x38)[0] == 0xEF53, "Run 'install' in Pillarix first!"

    bbm = rd(3)
    ibm = rd(4)

    # Ищем свободный инод >= 35
    ino = next(i for i in range(35, 257) if not (ibm[(i - 1) // 8] & (1 << ((i - 1) % 8))))
    ibm[(ino - 1) // 8] |= 1 << ((ino - 1) % 8)
    wr(4, ibm)

    # Ищем свободный блок >= 1150
    blk = next(b for b in range(1150, 8192) if not (bbm[b // 8] & (1 << (b % 8))))
    bbm[blk // 8] |= 1 << (blk % 8)
    wr(3, bbm)

    # Пишем данные файла
    fbuf = bytearray(1024)
    fbuf[:len(data)] = data
    wr(blk, fbuf)

    # Пишем инод (режим 0x81ED, 1 экстент)
    it_blk = 5 + (ino - 1) // 4
    it_off = ((ino - 1) % 4) * 256
    it = rd(it_blk)
    it[it_off:it_off + 256] = b"\x00" * 256
    struct.pack_into("<HHIIII", it, it_off, 0x81ED, 0, len(data), 0, 0, 0)
    struct.pack_into("<HHII", it, it_off + 24, 0, 1, 2, 0x00080000)
    # extent_header + extent в i_block (+40)
    struct.pack_into("<HHHHI", it, it_off + 40, 0xF30A, 1, 4, 0, 0)
    struct.pack_into("<IHHI", it, it_off + 52, 0, 1, 0, blk)
    wr(it_blk, it)

    # Добавляем запись в корневой каталог (блок 1125)
    dbuf = rd(1125)
    pos = 0
    while pos < 1024:
        e_ino, rec_len, nlen, _ = struct.unpack_from("<IHBB", dbuf, pos)
        if pos + rec_len >= 1024:
            actual = ((8 + nlen + 3) & ~3) if e_ino != 0 else 0
            need = (8 + len(name_b) + 3) & ~3
            assert pos + actual + need <= 1024, "Root dir full"
            if e_ino != 0:
                struct.pack_into("<H", dbuf, pos + 4, actual)
                pos += actual
            rem = 1024 - pos
            struct.pack_into("<IHBB", dbuf, pos, ino, rem, len(name_b), 1)
            dbuf[pos + 8:pos + 8 + len(name_b)] = name_b
            break
        pos += rec_len
    wr(1125, dbuf)

print(f"Written /{name} ({len(data)} bytes) -> inode {ino}, block {blk}")
