/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   29/01/22
*
* File:  lifo_storage_writer.c
* Descr:
*******************************************************************************/

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_log.h>
#include "lifo_storage_writer.h"
#include "can5_storagedriv.h"
#include "can5_storage_fsfat.h"
#include "can5_utils.h"

static const char *TAG = "LIFO_STORAGE_WRITER";

#define CAN5_LIFO_FILESIZE_MAX  4194000     // 4 mb
#define CAN5_FILE_BUF_SIZE      2048

static const char new_line =    '\n';
#define NEWLINE_SIZE            1

/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/

static can5_err_t __peek_or_pop(const char *tag, uint8_t *buf, size_t *buf_len, bool truncate_file);
static can5_err_t __get_fd_read(const char *path, FILE **file, const char *modes, char *out_fullpath); // modes here because for pop-delete
static can5_err_t __get_fd_write(const char *path, FILE **file, size_t buf_len, char *out_fullpath);
static can5_err_t __get_and_create_new_filename(const char *path, char *out_filename);
static can5_err_t __get_existing_filename(const char *path, char *out_filename);
static can5_err_t __get_numeric_filename(const char *path, uint32_t *out_filename);
static can5_err_t __tag_to_path(const char *tag, char *path);

// save some stack space as these operations are bounded by mutex
static char path_p[CAN5_LIFO_FILENAME_MAX] = {0 };
static char fullpath_p[CAN5_LIFO_FILENAME_MAX];
/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

#define CLOSE_FILE(file) if (fclose(file)) return CAN5_STORAGE_ERR_FILESYSTEM

can5_err_t can5_fsfat_lifo_push(const char *tag, const uint8_t *buf, size_t buf_len)
{
    FILE *file;
    can5_err_t ret;
    size_t pos;

    CLEAR_ARRAY(path_p);

    VERIFY_SUCCESS(__tag_to_path(tag, path_p));

    ret = __get_fd_write(path_p, &file, buf_len, NULL);

    if (ret == CAN5_STORAGE_ERR_TAG_NOT_FOUND) {
        if (mkdir(path_p, S_IRWXU) == -1) {
            return CAN5_STORAGE_ERR_FILESYSTEM;
        }
        VERIFY_SUCCESS(__get_fd_write(path_p, &file, buf_len, NULL));
    }
    else {
        VERIFY_SUCCESS(ret);
    }

    if (fseek(file, 0, SEEK_END)) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    pos = ftell(file);


    if (fseek(file, pos, SEEK_SET)) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (fwrite(buf, sizeof(uint8_t), buf_len, file) != buf_len) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (fwrite(&new_line, sizeof(uint8_t), NEWLINE_SIZE, file) != NEWLINE_SIZE) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    CLOSE_FILE(file);

    return CAN5_SUCCESS;
}


can5_err_t can5_fsfat_lifo_pop(const char *tag, uint8_t *buf, size_t *buf_len)
{
    return __peek_or_pop(tag, buf, buf_len, true);
}

can5_err_t can5_fsfat_lifo_peek(const char *tag, uint8_t *buf, size_t *buf_len)
{
    return __peek_or_pop(tag, buf, buf_len, false);
}

can5_err_t can5_fsfat_lifo_remove_tag(const char *tag)
{
    char dirname[CAN5_LIFO_FILENAME_MAX + 1] = {0 };
    char filename[CAN5_LIFO_FILENAME_MAX + 1] = {0 };
    struct stat st;
    struct dirent *entry;
    DIR *dir;

    VERIFY_SUCCESS(__tag_to_path(tag, dirname));

    if (stat(dirname, &st) == -1) {
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    if (!S_ISDIR(st.st_mode)) {
        // dictionary does not use directories
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    if ((dir = opendir(dirname)) == NULL) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        memset(filename, 0 , CAN5_LIFO_FILENAME_MAX + 1);
        strcpy(filename, dirname);
        strcat(filename, "/");
        strcat(filename, entry->d_name);

        unlink(filename);
    }

    if (rmdir(dirname) == -1) {
        return CAN5_STORAGE_ERR_DIR_NOT_EMPTY;
    }

    return CAN5_SUCCESS;
}

/* ---------------------------------------------------------------------
 * Private definition
 -----------------------------------------------------------------------*/


static can5_err_t __peek_or_pop(const char *tag, uint8_t *buf, size_t *buf_len, bool truncate_file)
{
    FILE *file;
    size_t file_size, read_len, copy_idx, out_len;
    char *b;


    CLEAR_ARRAY(path_p);
    CLEAR_ARRAY(fullpath_p);
    CLEAR_STRUCT(file);

    VERIFY_SUCCESS(__tag_to_path(tag, path_p));

    VERIFY_SUCCESS(__get_fd_read(path_p, &file, "r+", fullpath_p));

    if (fseek(file, 0, SEEK_END)) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    file_size = ftell(file);

    read_len = file_size > CAN5_FILE_BUF_SIZE ? CAN5_FILE_BUF_SIZE : file_size;

    if (read_len == 0) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_EMPTY;
    }

    b = NULL;
    VERIFY_ALLOC_SAFERETURN(b, read_len, {
        CLOSE_FILE(file);
    });

    if (fseek(file, file_size - read_len, SEEK_SET)) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (fread(b, read_len, 1, file) != 1) {
        CLOSE_FILE(file);
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    CLOSE_FILE(file);

    for (copy_idx = read_len - 1; copy_idx > 0 ; copy_idx-- ) {
        if (b[copy_idx - 1] == '\n') {
            break;
        }
    }

    for (out_len = 0; copy_idx < read_len - 1; copy_idx ++) {
        buf[out_len++] = b[copy_idx];
    }

    free(b);

    buf[out_len] = '\0';

    if (buf_len) {
        *buf_len = out_len;
    }

    if (!truncate_file) {
        return CAN5_SUCCESS;
    }

    size_t new_size = file_size - out_len - 1;


    if (truncate(fullpath_p, (off_t)new_size)) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (new_size <= 0) {
        char *pfile = &fullpath_p[strlen(fullpath_p) - 2];
        // dont delete if its file 1
        if (strcmp("/1", pfile)) {
            if (unlink(fullpath_p)) {
                return CAN5_STORAGE_ERR_FILESYSTEM;
            }
        }
    }

    return CAN5_SUCCESS;
}

static can5_err_t __get_fd_read(const char *path, FILE **file, const char *modes, char *out_fullpath)
{
    char filename[CAN5_LIFO_FILENAME_MAX] = { 0 };
    size_t pos;

    strcat(filename, path);
    strcat(filename, "/");

    pos = strlen(filename);


    VERIFY_SUCCESS(__get_existing_filename(path, &filename[pos]));

    *file = NULL;

    *file = fopen(filename, modes);

    if (!file) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (out_fullpath) {
        strcpy(out_fullpath, filename);
    }

    return CAN5_SUCCESS;
}

static can5_err_t __get_fd_write(const char *path, FILE **file, size_t buf_len, char *out_fullpath)
{
    char filename[CAN5_LIFO_FILENAME_MAX] = { 0 };
    can5_err_t ret;
    size_t pos;
    struct stat st;

    strcat(filename, path);
    strcat(filename, "/");

    pos = strlen(filename);

    ret = __get_existing_filename(path, &filename[pos]);

    if (ret == CAN5_STORAGE_ERR_FILE_NOT_FOUND) {
        VERIFY_SUCCESS(__get_and_create_new_filename(path, &filename[pos]));
    } else {
        VERIFY_SUCCESS(ret);
    }

    if (stat(filename, &st)) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (st.st_size + buf_len > CAN5_LIFO_FILESIZE_MAX) {
        memset(&filename[pos], 0, CAN5_LIFO_FILENAME_MAX - pos);
        VERIFY_SUCCESS(__get_and_create_new_filename(path, &filename[pos]));
    }

    *file = NULL;
    *file = fopen(filename, "r+");

    if (!file) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    if (out_fullpath) {
        strcpy(out_fullpath, filename);
    }

    return CAN5_SUCCESS;
}


static can5_err_t __get_existing_filename(const char *path, char *out_filename)
{
    uint32_t curr_file_i;

    VERIFY_SUCCESS(__get_numeric_filename(path, &curr_file_i));

    if (!curr_file_i) {
        return CAN5_STORAGE_ERR_FILE_NOT_FOUND;
    }

    snprintf(out_filename, CAN5_LIFO_FILENAME_MAX, "%u", curr_file_i);

    return CAN5_SUCCESS;
}


static can5_err_t __get_and_create_new_filename(const char *path, char *out_filename)
{
    uint32_t curr_file_i;
    char fullpath[CAN5_LIFO_FILENAME_MAX + 10];
    FILE *file;
    VERIFY_SUCCESS(__get_numeric_filename(path, &curr_file_i));

    curr_file_i += 1;

    snprintf(out_filename, CAN5_LIFO_FILENAME_MAX, "%u", curr_file_i);
    snprintf(fullpath, CAN5_LIFO_FILENAME_MAX + 10, "%s/%u", path, curr_file_i);

    file = fopen(fullpath, "w");
    if (!file) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }
    fclose(file);

    return CAN5_SUCCESS;
}

can5_err_t can5_fsfat_lifo_remove_old_data(const char *tag)
{

    uint32_t curr_file_i;

    CLEAR_ARRAY(path_p);
    CLEAR_ARRAY(fullpath_p);

    char *path = &path_p[0];

    VERIFY_SUCCESS(__tag_to_path(tag, path_p));

    VERIFY_SUCCESS(__get_numeric_filename(path_p, &curr_file_i));

    if (!curr_file_i) {
        return CAN5_STORAGE_ERR_FILE_NOT_FOUND;
    }

    if (curr_file_i <= CONFIG_CAN5_KEEP_CACHE_FILE_NUM) {
        // leave two sensor data block
        return CAN5_SUCCESS;
    }

    for (uint32_t i = 1; i <= curr_file_i - CONFIG_CAN5_KEEP_CACHE_FILE_NUM; i++)
    {
        CLEAR_ARRAY(fullpath_p);
        strcpy(fullpath_p, path);
        strcat(fullpath_p, "/");
        size_t pos = strlen(fullpath_p);
        snprintf(&fullpath_p[pos], CAN5_LIFO_FILENAME_MAX, "%u", i);

        ESP_LOGI(TAG, "Deleting %s.", fullpath_p);
        unlink(fullpath_p);
    }

    uint32_t new_i = 1;

    for(uint32_t i = curr_file_i - CONFIG_CAN5_KEEP_CACHE_FILE_NUM + 1; i <= curr_file_i; i++) {
        char fullpath_new[CAN5_LIFO_FILENAME_MAX];
        CLEAR_ARRAY(fullpath_new);
        CLEAR_ARRAY(fullpath_p);

        if (i <= 0 ) {
            continue;
        }

        strcpy(fullpath_p, path);
        strcat(fullpath_p, "/");
        size_t pos = strlen(fullpath_p);
        snprintf(&fullpath_p[pos], CAN5_LIFO_FILENAME_MAX, "%u", i);

        strcpy(fullpath_new, path);
        strcat(fullpath_new, "/");
        pos = strlen(fullpath_new);
        snprintf(&fullpath_new[pos], CAN5_LIFO_FILENAME_MAX, "%u", new_i);

        ESP_LOGI(TAG, "Renaming %s to %s", fullpath_p, fullpath_new);
        rename(fullpath_p, fullpath_new);

        new_i++;
    }

    return CAN5_SUCCESS;
}

static can5_err_t __get_numeric_filename(const char *path, uint32_t *out_filename)
{
    DIR *dir;
    uint32_t curr_file_i;

    struct dirent *dirent;
    dir = opendir(path);
    if (!dir) {
        return CAN5_STORAGE_ERR_TAG_NOT_FOUND;
    }

    // no filename with 0
    curr_file_i = 0;
    while ((dirent = readdir(dir)))
    {
        char *end = NULL;
        uint32_t file_i = 0;

        if (dirent->d_type == DT_REG) {
            file_i = strtoul(dirent->d_name, &end, 10);
            if (end == dirent->d_name) {
                // not a numeric value
                continue;
            }

            if (file_i > curr_file_i) {
                curr_file_i = file_i;
            }
        }
    }

    if (closedir(dir)) {
        return CAN5_STORAGE_ERR_FILESYSTEM;
    }

    *out_filename = curr_file_i;

    return CAN5_SUCCESS;
}


static can5_err_t __tag_to_path(const char *tag, char *path)
{
    if (strlen(tag) > CAN5_STORAGE_TAG_LEN_MAX) {
        return CAN5_STORAGE_ERR_TAG_TOO_LONG;
    }

    strcpy(path, CAN5_STORAGE_FSFAT_STACK_DIR);

    strcat(path, tag);

    return CAN5_SUCCESS;
}
