#include "ext4_priv.h"

/* Каталог занимает один блок. Записи идут подряд, последняя растянута
 * до конца блока (её rec_len включает свободное место). */

static uint32_t entry_len(int name_len) {
    return (EXT4_DIRENT_HEADER + name_len + 3) & ~3u;   /* кратно 4 */
}

static bool name_equals(const Ext4DirectoryEntry* de, const char* name) {
    int n = 0;
    while (n < de->name_len && name[n] != '\0') {
        if (de->name[n] != name[n]) return false;
        n++;
    }
    return de->name_len == n && name[n] == '\0';
}

/* Смещение последней живой записи в блоке */
static uint32_t last_entry_offset(const uint8_t* buf) {
    uint32_t offset = 0, last = 0;
    while (offset < EXT4_BLOCK_SIZE) {
        const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(buf + offset);
        if (de->rec_len == 0 || de->inode == 0) break;
        last = offset;
        offset += de->rec_len;
    }
    return last;
}

/* Номер инода по имени или 0, если такой записи нет */
uint32_t ext4_dir_lookup(const uint8_t* dir_buf, const char* name) {
    uint32_t offset = 0;
    while (offset < EXT4_BLOCK_SIZE) {
        const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(dir_buf + offset);
        if (de->rec_len == 0 || de->inode == 0) break;
        if (name_equals(de, name)) return de->inode;
        offset += de->rec_len;
    }
    return 0;
}

/* Поместится ли ещё одна запись с таким именем */
bool ext4_dir_has_room(const uint8_t* dir_buf, int name_len) {
    if (name_len > 255) return false;

    uint32_t last = last_entry_offset(dir_buf);
    const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(dir_buf + last);
    uint32_t free_from = last + entry_len(de->name_len);
    return free_from + entry_len(name_len) <= EXT4_BLOCK_SIZE;
}

/* Дописывает запись в конец блока. Место надо проверить заранее через
 * ext4_dir_has_room(). Возвращает длину новой записи. */
uint32_t ext4_dir_add_entry(uint8_t* dir_buf, const char* name, uint32_t ino, uint8_t type) {
    int name_len = 0;
    while (name[name_len] != '\0') name_len++;

    uint32_t last = last_entry_offset(dir_buf);
    Ext4DirectoryEntry* tail = (Ext4DirectoryEntry*)(dir_buf + last);
    uint32_t new_offset = last + entry_len(tail->name_len);

    /* Прежняя последняя запись отдаёт хвост блока новой */
    tail->rec_len = (uint16_t)entry_len(tail->name_len);

    Ext4DirectoryEntry* de = (Ext4DirectoryEntry*)(dir_buf + new_offset);
    de->inode     = ino;
    de->name_len  = (uint8_t)name_len;
    de->file_type = type;
    for (int i = 0; i < name_len; i++) de->name[i] = name[i];
    de->rec_len   = (uint16_t)(EXT4_BLOCK_SIZE - new_offset);

    return entry_len(name_len);
}

/* Кладёт запись в буфер по смещению, возвращает смещение за ней */
static uint32_t put_entry(uint8_t* buf, uint32_t offset, uint32_t ino, uint8_t type,
                          const char* name, int name_len, uint32_t rec_len) {
    Ext4DirectoryEntry* de = (Ext4DirectoryEntry*)(buf + offset);
    de->inode     = ino;
    de->rec_len   = (uint16_t)rec_len;
    de->name_len  = (uint8_t)name_len;
    de->file_type = type;
    for (int i = 0; i < name_len; i++) de->name[i] = name[i];
    return offset + rec_len;
}

/* Собирает блок нового каталога с "." и ".." и записывает на диск.
 * Остальные записи берутся из массивов names/inodes/types. */
void ext4_dir_format(uint32_t block, uint32_t self_ino, uint32_t parent_ino,
                     const char* const names[], const uint32_t inodes[],
                     const uint8_t types[], int count) {
    uint8_t buf[EXT4_BLOCK_SIZE];
    ext4_zero(buf, sizeof(buf));

    uint32_t offset = 0;
    offset = put_entry(buf, offset, self_ino,   EXT4_FT_DIR, ".",  1, entry_len(1));
    offset = put_entry(buf, offset, parent_ino, EXT4_FT_DIR, "..", 2, entry_len(2));

    for (int i = 0; i < count; i++) {
        int len = 0;
        while (names[i][len] != '\0') len++;

        /* последняя запись забирает всё оставшееся место */
        uint32_t rec_len = (i == count - 1) ? EXT4_BLOCK_SIZE - offset : entry_len(len);
        offset = put_entry(buf, offset, inodes[i], types[i], names[i], len, rec_len);
    }

    ext4_write_block(block, buf);
}

/* ls: печатает содержимое каталога, по строке на запись.
 * Возвращает число напечатанных строк. */
int ext4_ls(const char* path, int x, int y) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) {
        printf("ls: No such file or directory", x, y, 255, 255, 255, 255);
        return 1;
    }

    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) {
        printf("ls: failed to read inode", x, y, 255, 255, 255, 255);
        return 1;
    }

    /* ls для обычного файла просто печатает его путь */
    if ((inode.i_mode & EXT4_S_IFMT) != EXT4_S_IFDIR) {
        printf(path, x, y, 255, 255, 255, 255);
        return 1;
    }

    uint32_t block = ext4_inode_first_block(&inode);
    if (block == 0) return 0;

    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    ext4_read_block(block, dir_buf);

    const int line_height = 20;
    int lines = 0;
    uint32_t offset = 0;

    while (offset < EXT4_BLOCK_SIZE) {
        const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(dir_buf + offset);
        if (de->rec_len == 0 || de->inode == 0) break;

        char line[258];
        int n = 0;
        while (n < de->name_len) { line[n] = de->name[n]; n++; }
        if (de->file_type == EXT4_FT_DIR) line[n++] = '/';
        line[n] = '\0';

        printf(line, x, y + lines * line_height, 255, 255, 255, 255);
        lines++;
        offset += de->rec_len;
    }
    return lines;
}

int ext4_list_dir(const char* path, Ext4DirItem* out, uint32_t max_items) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) return EXT4_E_NOENT;

    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) return EXT4_E_IO;
    if ((inode.i_mode & EXT4_S_IFMT) != EXT4_S_IFDIR) return -20;

    uint32_t block = ext4_inode_first_block(&inode);
    if (block == 0) return 0;

    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    if (!ext4_read_block(block, dir_buf)) return EXT4_E_IO;

    uint32_t count = 0;
    uint32_t offset = 0;
    while (offset < EXT4_BLOCK_SIZE && count < max_items) {
        const Ext4DirectoryEntry* de = (const Ext4DirectoryEntry*)(dir_buf + offset);
        if (de->rec_len == 0 || de->inode == 0) break;

        uint8_t n = de->name_len;
        for (uint8_t i = 0; i < n; i++) out[count].name[i] = de->name[i];
        out[count].name[n] = '\0';
        out[count].file_type = de->file_type;
        count++;

        offset += de->rec_len;
    }
    return (int)count;
}
