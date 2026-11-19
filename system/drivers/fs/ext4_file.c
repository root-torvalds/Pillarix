#include "ext4_priv.h"

/* Файл у нас занимает не больше одного блока. Размер берём из инода, а если
 * там 0 или явная ерунда — оцениваем по последнему ненулевому байту. */
static uint32_t file_size_in_block(const Ext4Inode* inode, const uint8_t* data) {
    uint32_t size = inode->i_size_lo;
    if (size != 0 && size <= EXT4_BLOCK_SIZE) return size;

    size = 0;
    for (uint32_t i = 0; i < EXT4_BLOCK_SIZE; i++) {
        if (data[i] != 0) size = i + 1;
    }
    return size;
}

/* cat: печатает файл построчно, непечатные символы пропускает.
 * Возвращает число напечатанных строк. */
int ext4_cat(const char* path, int x, int y) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) {
        printf("cat: No such file or directory", x, y, 255, 80, 80, 255);
        return 1;
    }

    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) {
        printf("cat: Failed to read inode", x, y, 255, 80, 80, 255);
        return 1;
    }

    uint32_t block = ext4_inode_first_block(&inode);
    if (block == 0) {
        printf("cat: File has no blocks", x, y, 255, 80, 80, 255);
        return 1;
    }

    uint8_t data[EXT4_BLOCK_SIZE];
    ext4_zero(data, sizeof(data));
    ext4_read_block(block, data);

    uint32_t size = file_size_in_block(&inode, data);
    if (size == 0) {
        printf("cat: File is empty", x, y, 255, 255, 0, 255);
        return 1;
    }

    const int line_height = 20;
    int lines = 0;
    char line[256];
    int line_len = 0;

    for (uint32_t i = 0; i < size; i++) {
        char c = (char)data[i];

        if (c == '\n' || line_len >= 254) {
            line[line_len] = '\0';
            printf(line, x, y + lines * line_height, 255, 255, 255, 255);
            lines++;
            line_len = 0;
        } else if (c >= 32 && c <= 126) {
            line[line_len++] = c;
        }
    }

    if (line_len > 0) {
        line[line_len] = '\0';
        printf(line, x, y + lines * line_height, 255, 255, 255, 255);
        lines++;
    }
    return lines > 0 ? lines : 1;
}

int ext4_read_file_to_buffer(const char* path, void* buf, uint32_t max_size) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) return EXT4_E_NOENT;

    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) return EXT4_E_IO;
    if ((inode.i_mode & EXT4_S_IFMT) == EXT4_S_IFDIR) return EXT4_E_ISDIR;

    const struct ext4_extent_header* hdr = (const struct ext4_extent_header*)inode.i_block;
    if (hdr->eh_magic != EXT4_EXT_MAGIC || hdr->eh_entries == 0) return 0;

    uint32_t target_size = inode.i_size_lo;
    if (target_size == 0) return 0;
    if (target_size > max_size) target_size = max_size;

    uint8_t* dest = (uint8_t*)buf;
    uint32_t total_read = 0;
    uint8_t block_buf[EXT4_BLOCK_SIZE];

    const struct ext4_extent* exts =
    (const struct ext4_extent*)(inode.i_block + sizeof(struct ext4_extent_header));

    for (uint16_t e = 0; e < hdr->eh_entries && total_read < target_size; e++) {
        uint32_t start_blk = exts[e].e_ee_start_lo;
        uint16_t num_blks  = exts[e].e_ee_len;

        for (uint16_t b = 0; b < num_blks && total_read < target_size; b++) {
            ext4_zero(block_buf, sizeof(block_buf));
            if (!ext4_read_block(start_blk + b, block_buf)) {
                return (total_read > 0) ? (int)total_read : EXT4_E_IO;
            }

            uint32_t chunk = target_size - total_read;
            if (chunk > EXT4_BLOCK_SIZE) chunk = EXT4_BLOCK_SIZE;

            for (uint32_t i = 0; i < chunk; i++) {
                dest[total_read + i] = block_buf[i];
            }
            total_read += chunk;
        }
    }

    return (int)total_read;
}

/* touch передаёт data_size = 0 и пустую строку, редактор — настоящую длину.
 * Поэтому при нулевой длине считаем её по строке. */
static uint32_t data_length(const void* data, uint32_t data_size) {
    if (data_size != 0 || data == NULL) return data_size;

    const char* p = (const char*)data;
    uint32_t len = 0;
    while (p[len] != '\0' && len < EXT4_BLOCK_SIZE) len++;
    return len;
}

/* Блок с данными: сами данные и нули до конца */
static void fill_block(uint8_t* block, const void* data, uint32_t len) {
    ext4_zero(block, EXT4_BLOCK_SIZE);
    if (data == NULL) return;

    const uint8_t* src = (const uint8_t*)data;
    for (uint32_t i = 0; i < len; i++) block[i] = src[i];
}

/* Файл уже есть — переписываем его содержимое (так работает edit) */
static int overwrite_file(uint32_t ino, uint16_t mode, const void* data, uint32_t len) {
    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) return EXT4_E_FILE_INODE;

    uint32_t block = ext4_inode_first_block(&inode);
    if (block == 0) {
        block = ext4_alloc_block();
        if (block == 0) return EXT4_E_NO_BLOCK;
    }

    uint8_t buf[EXT4_BLOCK_SIZE];
    fill_block(buf, data, len);
    if (!ext4_write_block(block, buf)) return EXT4_E_DISK_WRITE;

    inode.i_mode        = mode;
    inode.i_size_lo     = len;
    inode.i_links_count = 1;
    inode.i_blocks_lo   = 2;
    ext4_set_extent(&inode, block, 1);

    if (!ext4_write_inode(ino, &inode)) return EXT4_E_INODE_WRITE;
    return 0;
}

/* Файла нет — заводим инод, блок и запись в каталоге (так работает touch) */
static int create_new_file(uint32_t dir_ino, Ext4Inode* dir_inode, uint32_t dir_block,
                           uint8_t* dir_buf, const char* name, uint16_t mode,
                           const void* data, uint32_t len) {
    int name_len = 0;
    while (name[name_len] != '\0') name_len++;

    /* Место в каталоге проверяем до выделения инода и блока,
     * иначе при отказе они останутся занятыми впустую. */
    if (!ext4_dir_has_room(dir_buf, name_len)) return EXT4_E_DIR_FULL;

    uint32_t ino = ext4_alloc_inode();
    if (ino == 0) return EXT4_E_NO_INODE;

    uint32_t block = ext4_alloc_block();
    if (block == 0) return EXT4_E_NO_BLOCK;

    uint8_t buf[EXT4_BLOCK_SIZE];
    fill_block(buf, data, len);
    if (!ext4_write_block(block, buf)) return EXT4_E_DISK_WRITE;

    Ext4Inode inode;
    ext4_zero(&inode, sizeof(inode));
    inode.i_mode        = mode;
    inode.i_size_lo     = len;
    inode.i_links_count = 1;
    inode.i_blocks_lo   = 2;
    ext4_set_extent(&inode, block, 1);
    if (!ext4_write_inode(ino, &inode)) return EXT4_E_INODE_WRITE;

    uint32_t added = ext4_dir_add_entry(dir_buf, name, ino, EXT4_FT_REG);
    if (!ext4_write_block(dir_block, dir_buf)) return EXT4_E_DISK_WRITE;

    dir_inode->i_size_lo += added;
    if (!ext4_write_inode(dir_ino, dir_inode)) return EXT4_E_INODE_WRITE;
    return 0;
}

/* Создаёт файл name в каталоге path или перезаписывает уже существующий.
 * Возвращает 0 или отрицательный код EXT4_E_*. */
int ext4_create_file(const char* path, const char* name, uint16_t mode,
                     const void* data, uint32_t data_size) {
    if (name == NULL || name[0] == '\0') return EXT4_E_BAD_NAME;
    if (data_size > EXT4_BLOCK_SIZE) return EXT4_E_TOO_BIG;

    uint32_t dir_ino = ext4_find_inode_by_path(path);
    if (dir_ino == 0) return EXT4_E_NO_PARENT;

    Ext4Inode dir_inode;
    uint32_t dir_block;
    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    int err = ext4_load_dir(dir_ino, &dir_inode, &dir_block, dir_buf);
    if (err != 0) return err;

    uint32_t len = data_length(data, data_size);

    uint32_t existing = ext4_dir_lookup(dir_buf, name);
    if (existing != 0) return overwrite_file(existing, mode, data, len);

    return create_new_file(dir_ino, &dir_inode, dir_block, dir_buf, name, mode, data, len);
}

/* Создаёт каталог name в каталоге path. Возвращает 0 или код EXT4_E_*. */
int ext4_create_dir(const char* path, const char* name) {
    if (name == NULL || name[0] == '\0') return EXT4_E_BAD_NAME;

    int name_len = 0;
    while (name[name_len] != '\0') name_len++;

    uint32_t dir_ino = ext4_find_inode_by_path(path);
    if (dir_ino == 0) return EXT4_E_NO_PARENT;

    Ext4Inode dir_inode;
    uint32_t dir_block;
    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    int err = ext4_load_dir(dir_ino, &dir_inode, &dir_block, dir_buf);
    if (err != 0) return err;

    /* Проверяем, не занято ли уже такое имя и хватит ли места в блоке */
    if (ext4_dir_lookup(dir_buf, name) != 0) return EXT4_E_BAD_NAME;
    if (!ext4_dir_has_room(dir_buf, name_len)) return EXT4_E_DIR_FULL;

    uint32_t ino = ext4_alloc_inode();
    if (ino == 0) return EXT4_E_NO_INODE;

    uint32_t block = ext4_alloc_block();
    if (block == 0) return EXT4_E_NO_BLOCK;

    /* Форматируем блок нового каталога с записями "." и ".." */
    ext4_dir_format(block, ino, dir_ino, NULL, NULL, NULL, 0);

    /* Заполняем и сохраняем инод нового каталога */
    Ext4Inode inode;
    ext4_zero(&inode, sizeof(inode));
    inode.i_mode        = EXT4_MODE_DIR;
    inode.i_size_lo     = EXT4_BLOCK_SIZE;
    inode.i_links_count = 2;
    inode.i_blocks_lo   = 2;
    ext4_set_extent(&inode, block, 1);
    if (!ext4_write_inode(ino, &inode)) return EXT4_E_INODE_WRITE;

    /* Добавляем запись в родительский каталог и обновляем его инод */
    uint32_t added = ext4_dir_add_entry(dir_buf, name, ino, EXT4_FT_DIR);
    if (!ext4_write_block(dir_block, dir_buf)) return EXT4_E_DISK_WRITE;

    dir_inode.i_size_lo += added;
    dir_inode.i_links_count++;
    if (!ext4_write_inode(dir_ino, &dir_inode)) return EXT4_E_INODE_WRITE;

    return 0;
}

static void bitmap_clear_bit(uint32_t bitmap_block, uint32_t bit_index) {
    uint8_t bm[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(bitmap_block, bm)) return;
    bm[bit_index / 8] &= (uint8_t)~(1 << (bit_index % 8));
    ext4_write_block(bitmap_block, bm);
}

static bool dir_name_match(const Ext4DirectoryEntry* de, const char* name) {
    int n = 0;
    while (n < de->name_len && name[n] != '\0') {
        if (de->name[n] != name[n]) return false;
        n++;
    }
    return de->name_len == n && name[n] == '\0';
}

int ext4_remove(const char* path) {
    if (!path || path[0] != '/' || path[1] == '\0') return EXT4_E_BAD_NAME;

    char dir_path[300], name[256];
    ext4_split_path(path, dir_path, name);
    if (name[0] == '\0' || (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))) {
        return EXT4_E_BAD_NAME;
    }

    uint32_t dir_ino = ext4_find_inode_by_path(dir_path);
    if (dir_ino == 0) return EXT4_E_NO_PARENT;

    Ext4Inode dir_inode;
    uint32_t dir_block;
    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    int err = ext4_load_dir(dir_ino, &dir_inode, &dir_block, dir_buf);
    if (err != 0) return err;

    uint32_t target_ino = ext4_dir_lookup(dir_buf, name);
    if (target_ino == 0) return EXT4_E_NOENT;

    Ext4Inode target;
    if (!ext4_read_inode(target_ino, &target)) return EXT4_E_FILE_INODE;

    bool is_dir = (target.i_mode & EXT4_S_IFMT) == EXT4_S_IFDIR;
    if (is_dir) {
        uint32_t tblk = ext4_inode_first_block(&target);
        if (tblk != 0) {
            uint8_t tbuf[EXT4_BLOCK_SIZE];
            if (!ext4_read_block(tblk, tbuf)) return EXT4_E_IO;
            uint32_t off = 0;
            int entries = 0;
            while (off < EXT4_BLOCK_SIZE) {
                const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(tbuf + off);
                if (de->rec_len == 0 || de->inode == 0) break;
                entries++;
                off += de->rec_len;
            }
            if (entries > 2) return -21;
        }
    }

    uint32_t off = 0, prev_off = 0;
    bool removed = false;
    while (off < EXT4_BLOCK_SIZE) {
        Ext4DirectoryEntry* de = (Ext4DirectoryEntry*)(dir_buf + off);
        if (de->rec_len == 0 || de->inode == 0) break;
        if (de->inode == target_ino && dir_name_match(de, name)) {
            if (off > 0) {
                Ext4DirectoryEntry* prev = (Ext4DirectoryEntry*)(dir_buf + prev_off);
                prev->rec_len = (uint16_t)(prev->rec_len + de->rec_len);
                ext4_zero(de, EXT4_DIRENT_HEADER);
                removed = true;
            }
            break;
        }
        prev_off = off;
        off += de->rec_len;
    }
    if (!removed) return EXT4_E_NOENT;
    if (!ext4_write_block(dir_block, dir_buf)) return EXT4_E_DISK_WRITE;

    if (is_dir && dir_inode.i_links_count > 0) {
        dir_inode.i_links_count--;
        ext4_write_inode(dir_ino, &dir_inode);
    }

    const struct ext4_extent_header* hdr = (const struct ext4_extent_header*)target.i_block;
    if (hdr->eh_magic == EXT4_EXT_MAGIC && hdr->eh_entries > 0) {
        const struct ext4_extent* exts =
            (const struct ext4_extent*)(target.i_block + sizeof(struct ext4_extent_header));
        for (uint16_t e = 0; e < hdr->eh_entries; e++) {
            for (uint16_t b = 0; b < exts[e].e_ee_len; b++) {
                uint32_t blk = exts[e].e_ee_start_lo + b;
                if (blk >= EXT4_ALLOC_START_BLOCK) bitmap_clear_bit(EXT4_BLOCK_BITMAP_BLOCK, blk);
            }
        }
    }

    ext4_zero(&target, sizeof(target));
    ext4_write_inode(target_ino, &target);
    if (target_ino > 0) bitmap_clear_bit(EXT4_INODE_BITMAP_BLOCK, target_ino - 1);

    return 0;
}
