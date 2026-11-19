#!/usr/bin/env python3
import struct, sys

if len(sys.argv) != 4:
    print("Usage: put_plx.py <disk.img> <prog.plx> <dest_path>")
    sys.exit(1)

img_path, plx_path, raw_path = sys.argv[1], sys.argv[2], sys.argv[3].strip("/")
assert raw_path, "Destination path cannot be empty"

parts = raw_path.split("/")
dir_parts = parts[:-1]
file_name = parts[-1]
name_b = file_name.encode("ascii")

data = open(plx_path, "rb").read()
assert 0 < len(data) <= 4 * 1024 * 1024, "File must be 1..4194304 bytes"
num_blocks = (len(data) + 1023) // 1024

with open(img_path, "r+b") as f:
    def rd(blk):
        f.seek(blk * 1024)
        return bytearray(f.read(1024))

    def wr(blk, buf):
        f.seek(blk * 1024)
        f.write(buf)

    bbm = rd(3)
    ibm = rd(4)

    def alloc_inode():
        ino = next(i for i in range(35, 257) if not (ibm[(i - 1) // 8] & (1 << ((i - 1) % 8))))
        ibm[(ino - 1) // 8] |= 1 << ((ino - 1) % 8)
        wr(4, ibm)
        return ino

    def alloc_blocks(count):
        blk = None
        for b in range(1150, 16384 - count):
            if all(not (bbm[(b + k) // 8] & (1 << ((b + k) % 8))) for k in range(count)):
                blk = b
                break
        assert blk is not None, "No contiguous free blocks found"
        for k in range(count):
            bbm[(blk + k) // 8] |= 1 << ((blk + k) % 8)
        wr(3, bbm)
        return blk

    def get_inode_data_block(ino):
        it_blk = 5 + (ino - 1) // 4
        it_off = ((ino - 1) % 4) * 256
        it = rd(it_blk)
        mode = struct.unpack_from("<H", it, it_off)[0]
        assert (mode & 0xF000) == 0x4000, f"Inode {ino} is not a directory"
        magic = struct.unpack_from("<H", it, it_off + 40)[0]
        assert magic == 0xF30A, f"Inode {ino} does not use extents"
        _, _, _, ee_start_lo = struct.unpack_from("<IHHI", it, it_off + 52)
        return ee_start_lo

    def add_dir_entry(dir_blk, entry_ino, entry_name_b, file_type):
        dbuf = rd(dir_blk)
        pos = 0
        need = (8 + len(entry_name_b) + 3) & ~3
        while pos < 1024:
            e_ino, rec_len, nlen, _ = struct.unpack_from("<IHBB", dbuf, pos)
            if rec_len == 0:
                # Блок пустой или повреждён в конце — пишем до конца блока
                assert pos + need <= 1024, "Target directory block full"
                struct.pack_into("<IHBB", dbuf, pos, entry_ino, 1024 - pos, len(entry_name_b), file_type)
                dbuf[pos + 8:pos + 8 + len(entry_name_b)] = entry_name_b
                break
            if pos + rec_len >= 1024:
                actual = ((8 + nlen + 3) & ~3) if e_ino != 0 else 0
                assert pos + actual + need <= 1024, "Target directory block full"
                if e_ino != 0:
                    struct.pack_into("<H", dbuf, pos + 4, actual)
                    pos += actual
                rem = 1024 - pos
                struct.pack_into("<IHBB", dbuf, pos, entry_ino, rem, len(entry_name_b), file_type)
                dbuf[pos + 8:pos + 8 + len(entry_name_b)] = entry_name_b
                break
            pos += rec_len
        wr(dir_blk, dbuf)

    def create_subdir(parent_ino, parent_blk, dname_b):
        new_ino = alloc_inode()
        new_blk = alloc_blocks(1)

        new_dbuf = bytearray(1024)
        struct.pack_into("<IHBB", new_dbuf, 0, new_ino, 12, 1, 2)
        new_dbuf[8:9] = b"."
        struct.pack_into("<IHBB", new_dbuf, 12, parent_ino, 1012, 2, 2)
        new_dbuf[20:22] = b".."
        wr(new_blk, new_dbuf)

        it_blk = 5 + (new_ino - 1) // 4
        it_off = ((new_ino - 1) % 4) * 256
        it = rd(it_blk)
        it[it_off:it_off + 256] = b"\x00" * 256
        struct.pack_into("<HHIIII", it, it_off, 0x41ED, 0, 1024, 0, 0, 0)
        struct.pack_into("<HHII", it, it_off + 24, 0, 2, 2, 0x00080000)
        struct.pack_into("<HHHHI", it, it_off + 40, 0xF30A, 1, 4, 0, 0)
        struct.pack_into("<IHHI", it, it_off + 52, 0, 1, 0, new_blk)
        wr(it_blk, it)

        add_dir_entry(parent_blk, new_ino, dname_b, 2)
        return new_ino

    sb = rd(1)
    assert struct.unpack_from("<H", sb, 0x38)[0] == 0xEF53, "Run 'install' in Pillarix first!"

    # 1. Ищем или создаём цепочку директорий от корня (ino = 2)
    curr_ino = 2
    for dname in dir_parts:
        dname_b = dname.encode("ascii")
        dir_blk = get_inode_data_block(curr_ino)
        dbuf = rd(dir_blk)
        pos = 0
        found_ino = None
        while pos < 1024:
            e_ino, rec_len, nlen, _ = struct.unpack_from("<IHBB", dbuf, pos)
            if e_ino != 0 and dbuf[pos + 8:pos + 8 + nlen] == dname_b:
                found_ino = e_ino
                break
            if rec_len == 0 or pos + rec_len >= 1024:
                break
            pos += rec_len
        if found_ino is None:
            found_ino = create_subdir(curr_ino, dir_blk, dname_b)
        curr_ino = found_ino

    target_dir_blk = get_inode_data_block(curr_ino)

    # 2. Выделяем инод и блоки под бинарник
    ino = alloc_inode()
    blk = alloc_blocks(num_blocks)

    # 3. Записываем данные файла
    fbuf = bytearray(num_blocks * 1024)
    fbuf[:len(data)] = data
    for k in range(num_blocks):
        wr(blk + k, fbuf[k * 1024:(k + 1) * 1024])

    # 4. Записываем инод файла
    it_blk = 5 + (ino - 1) // 4
    it_off = ((ino - 1) % 4) * 256
    it = rd(it_blk)
    it[it_off:it_off + 256] = b"\x00" * 256
    struct.pack_into("<HHIIII", it, it_off, 0x81ED, 0, len(data), 0, 0, 0)
    struct.pack_into("<HHII", it, it_off + 24, 0, 1, num_blocks * 2, 0x00080000)
    struct.pack_into("<HHHHI", it, it_off + 40, 0xF30A, 1, 4, 0, 0)
    struct.pack_into("<IHHI", it, it_off + 52, 0, num_blocks, 0, blk)
    wr(it_blk, it)

    # 5. Добавляем или обновляем запись файла в целевой директории (file_type = 1)
    dbuf = rd(target_dir_blk)
    pos = 0
    updated = False
    while pos < 1024:
        e_ino, rec_len, nlen, _ = struct.unpack_from("<IHBB", dbuf, pos)
        if e_ino != 0 and dbuf[pos + 8:pos + 8 + nlen] == name_b:
            struct.pack_into("<I", dbuf, pos, ino)
            updated = True
            break
        if rec_len == 0 or pos + rec_len >= 1024:
            break
        pos += rec_len

    if not updated:
        add_dir_entry(target_dir_blk, ino, name_b, 1)
    else:
        wr(target_dir_blk, dbuf)

print(f"Written /{raw_path} ({len(data)} bytes) -> inode {ino}, blocks {blk}..{blk + num_blocks - 1}")
