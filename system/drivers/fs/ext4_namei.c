#include "ext4_priv.h"

/* Копирует не больше n символов (и не больше max) и ставит '\0' */
static void copy_limited(char* dst, const char* src, int n, int max) {
    int i = 0;
    while (i < n && i < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Делит путь на каталог и имя: "/a/b.txt" -> "/a" и "b.txt".
 * Без слэша или со слэшем только в начале каталогом считается "/". */
void ext4_split_path(const char* full_path, char* dir_out, char* name_out) {
    if (!full_path || full_path[0] == '\0') {
        dir_out[0] = '/';
        dir_out[1] = '\0';
        name_out[0] = '\0';
        return;
    }

    int len = 0;
    while (full_path[len] != '\0') len++;

    int slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (full_path[i] == '/') { slash = i; break; }
    }

    int name_start = slash + 1;
    if (slash <= 0) {
        dir_out[0] = '/';
        dir_out[1] = '\0';
    } else {
        copy_limited(dir_out, full_path, slash, 299);
    }
    copy_limited(name_out, full_path + name_start, len - name_start, 255);
}

/* Читает инод каталога и его блок. Возвращает 0 или код ошибки
 * в тех же числах, что и ext4_create_file(). */
int ext4_load_dir(uint32_t dir_ino, Ext4Inode* inode, uint32_t* block, uint8_t* buf) {
    if (!ext4_read_inode(dir_ino, inode)) return EXT4_E_PARENT_INODE;

    *block = ext4_inode_first_block(inode);
    if (*block == 0) return EXT4_E_PARENT_BLOCK;

    if (!ext4_read_block(*block, buf)) return EXT4_E_DIR_READ;
    return 0;
}

/* Идём по пути вглубь рекурсивно: сначала находим родителя, потом имя в нём.
 * Пути у нас короткие, глубина небольшая. 0 — не найдено. */
uint32_t ext4_find_inode_by_path(const char* path) {
    if (!path || path[0] != '/') return 0;
    if (path[1] == '\0') return EXT4_ROOT_INO;

    char dir_path[300];
    char name[256];
    ext4_split_path(path, dir_path, name);

    uint32_t parent = EXT4_ROOT_INO;
    if (dir_path[0] != '/' || dir_path[1] != '\0') {
        parent = ext4_find_inode_by_path(dir_path);
        if (parent == 0) return 0;
    }

    Ext4Inode inode;
    uint32_t block;
    uint8_t dir_buf[EXT4_BLOCK_SIZE];
    if (ext4_load_dir(parent, &inode, &block, dir_buf) != 0) return 0;

    return ext4_dir_lookup(dir_buf, name);
}

bool ext4_is_dir(const char* path) {
    uint32_t ino = ext4_find_inode_by_path(path);
    if (ino == 0) return false;

    Ext4Inode inode;
    if (!ext4_read_inode(ino, &inode)) return false;

    return (inode.i_mode & EXT4_S_IFMT) == EXT4_S_IFDIR;
}
