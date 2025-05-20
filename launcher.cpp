#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <linux/stat.h>
#include <limits.h>

const char *TARGET_PACKAGE_JSON = "resources/app/package.json";
const char *TARGET_PACKAGE_JSON_ALT = "/opt/QQ/resources/app/package.json";
const char *TARGET_NAPCAT_JS = "resources/app/loadNapCat.js";

// 注入的 JS 内容
const char *NAPCAT_JS_CONTENT =
    "const path = require('path');"
    "const CurrentPath = process.env.NAPCAT_BOOTMAIN || path.dirname(__filename);"
    "(async () => {"
    "    await import('file://' + path.join(CurrentPath, './napcat.mjs'));"
    "})();";

// package.json 替换内容
const char *ORIGINAL_MAIN = "\"main\": \"./application.asar/app_launcher/index.js\"";
const char *NEW_MAIN = "\"main\": \"loadNapCat.js\"";

// 缓存修改后的 package.json
static char *g_modified_package_json = nullptr;

/**
 * 检查文件路径是否与目标路径匹配（支持绝对路径和相对路径）
 */
static bool path_matches(const char *path, const char *target)
{
    if (!path || !target)
        return false;

    size_t path_len = strlen(path);
    size_t target_len = strlen(target);

    if (path_len < target_len)
        return false;

    // 完全相等或以 target 结尾
    return strcmp(path, target) == 0 || strcmp(path + path_len - target_len, target) == 0;
}

/**
 * 读取并修改 package.json 内容
 */
static char *get_modified_packagejson()
{
    if (g_modified_package_json)
        return g_modified_package_json;

    // 使用真实的 fopen 防止递归
    static FILE *(*real_fopen)(const char *, const char *) = nullptr;
    if (!real_fopen)
    {
        real_fopen = (FILE * (*)(const char *, const char *)) dlsym(RTLD_NEXT, "fopen");
        if (!real_fopen)
            return nullptr;
    }

    FILE *fp = real_fopen(TARGET_PACKAGE_JSON_ALT, "r");
    if (!fp)
        return nullptr;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer)
    {
        fclose(fp);
        return nullptr;
    }

    size_t bytes_read = fread(buffer, 1, file_size, fp);
    buffer[bytes_read] = 0;
    fclose(fp);

    char *main_pos = strstr(buffer, ORIGINAL_MAIN);
    if (!main_pos)
    {
        g_modified_package_json = buffer;
        return buffer;
    }

    size_t prefix_size = main_pos - buffer;
    size_t suffix_size = strlen(main_pos + strlen(ORIGINAL_MAIN));
    size_t new_size = prefix_size + strlen(NEW_MAIN) + suffix_size;

    char *modified = (char *)malloc(new_size + 1);
    if (!modified)
    {
        free(buffer);
        return nullptr;
    }

    memcpy(modified, buffer, prefix_size);
    memcpy(modified + prefix_size, NEW_MAIN, strlen(NEW_MAIN));
    memcpy(modified + prefix_size + strlen(NEW_MAIN),
           main_pos + strlen(ORIGINAL_MAIN), suffix_size);
    modified[new_size] = 0;

    free(buffer);
    g_modified_package_json = modified;

    return g_modified_package_json;
}

/**
 * 创建内存文件并写入内容
 */
static int create_memfd_with_content(const char *content)
{
    if (!content)
        return -1;

    int fd = syscall(SYS_memfd_create, "napcat_memfd", 0);
    if (fd < 0)
        return -1;

    size_t content_len = strlen(content);
    ssize_t written = write(fd, content, content_len);

    if (written != (ssize_t)content_len)
    {
        close(fd);
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) == -1)
    {
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * 处理目标文件的访问请求
 */
static int handle_target_file(const char *pathname)
{
    if (path_matches(pathname, TARGET_PACKAGE_JSON) || path_matches(pathname, TARGET_PACKAGE_JSON_ALT))
    {
        printf("[launcher] intercepted package.json: %s, replacing content\n", pathname);
        char *content = get_modified_packagejson();
        printf("[launcher] package.json content: %s\n", content ? content : "NULL");
        if (!content)
        {
            errno = ENOENT;
            return -1;
        }
        return create_memfd_with_content(content);
    }
    else if (path_matches(pathname, TARGET_NAPCAT_JS))
    {
        printf("[launcher] intercepted loadNapCat.js: %s, replacing content\n", pathname);
        printf("[launcher] loadNapCat.js content: %s\n", NAPCAT_JS_CONTENT);
        return create_memfd_with_content(NAPCAT_JS_CONTENT);
    }

    return -1;
}

/**
 * Hook 系统 open64 函数
 */
extern "C" int open64(const char *pathname, int flags, ...)
{
    printf("[launcher] open64: %s\n", pathname);

    static int (*real_open64)(const char *, int, ...) = nullptr;
    if (!real_open64)
    {
        real_open64 = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open64");
        if (!real_open64)
            return -1;
    }

    int target_fd = handle_target_file(pathname);
    if (target_fd >= 0)
        return target_fd;

    va_list args;
    va_start(args, flags);
    int result;
    if (flags & O_CREAT)
    {
        int mode = va_arg(args, int);
        result = real_open64(pathname, flags, mode);
    }
    else
    {
        result = real_open64(pathname, flags);
    }
    va_end(args);

    return result;
}

/**
 * Hook 系统 fopen64 函数
 */
extern "C" FILE *fopen64(const char *pathname, const char *mode)
{
    printf("[launcher] fopen64: %s\n", pathname);

    int target_fd = handle_target_file(pathname);
    if (target_fd >= 0)
    {
        FILE *fp = fdopen(target_fd, mode);
        if (!fp)
            close(target_fd);
        return fp;
    }

    static FILE *(*real_fopen64)(const char *, const char *) = nullptr;
    if (!real_fopen64)
    {
        real_fopen64 = (FILE * (*)(const char *, const char *)) dlsym(RTLD_NEXT, "fopen64");
        if (!real_fopen64)
            return nullptr;
    }
    return real_fopen64(pathname, mode);
}