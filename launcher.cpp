#define _GNU_SOURCE
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

const char *TARGET_PACKAGE_JSON = "resources/app/package.json";
const char *TARGET_NAPCAT_JS = "resources/app/loadNapCat.js";

const char *NAPCAT_JS_CONTENT =
    "const path = require('path');"
    "const CurrentPath = process.env.NAPCAT_BOOTMAIN || path.dirname(__filename);"
    "(async () => {"
    "    await import('file://' + path.join(CurrentPath, './napcat.mjs'));"
    "})();";

const char *ORIGINAL_MAIN = "\"main\": \"./application.asar/app_launcher/index.js\"";
const char *NEW_MAIN = "\"main\": \"loadNapCat.js\"";

static char *g_modified_package_json = nullptr;

/**
 * 检查文件路径是否与目标路径匹配
 */
static bool path_matches(const char *path, const char *target)
{
    if (!path || !target)
        return false;

    size_t path_len = strlen(path);
    size_t target_len = strlen(target);

    if (path_len < target_len)
        return false;

    return strcmp(path + path_len - target_len, target) == 0;
}

/**
 * 读取并修改package.json内容
 */
static char *get_modified_packagejson()
{
    if (g_modified_package_json)
    {
        return g_modified_package_json;
    }

    FILE *fp = fopen(TARGET_PACKAGE_JSON, "r");
    if (!fp)
    {
        return nullptr;
    }

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
    {
        return -1;
    }

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
    if (path_matches(pathname, TARGET_PACKAGE_JSON))
    {
        char *content = get_modified_packagejson();
        if (!content)
        {
            errno = ENOENT;
            return -1;
        }
        return create_memfd_with_content(content);
    }
    else if (path_matches(pathname, TARGET_NAPCAT_JS))
    {
        return create_memfd_with_content(NAPCAT_JS_CONTENT);
    }

    return -1;
}

/**
 * Hook系统open函数
 */
extern "C" int open(const char *pathname, int flags, ...)
{
    static int (*real_open)(const char *, int, ...) = nullptr;
    if (!real_open)
    {
        real_open = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, "open");
        if (!real_open)
            return -1;
    }

    int target_fd = handle_target_file(pathname);
    if (target_fd >= 0)
    {
        return target_fd;
    }

    va_list args;
    va_start(args, flags);
    int result;
    if (flags & O_CREAT)
    {
        int mode = va_arg(args, int);
        result = real_open(pathname, flags, mode);
    }
    else
    {
        result = real_open(pathname, flags);
    }
    va_end(args);

    return result;
}

/**
 * Hook系统openat函数
 */
extern "C" int openat(int dirfd, const char *pathname, int flags, ...)
{
    static int (*real_openat)(int, const char *, int, ...) = nullptr;
    if (!real_openat)
    {
        real_openat = (int (*)(int, const char *, int, ...))dlsym(RTLD_NEXT, "openat");
        if (!real_openat)
            return -1;
    }

    int target_fd = handle_target_file(pathname);
    if (target_fd >= 0)
    {
        return target_fd;
    }

    va_list args;
    va_start(args, flags);
    int result;
    if (flags & O_CREAT)
    {
        int mode = va_arg(args, int);
        result = real_openat(dirfd, pathname, flags, mode);
    }
    else
    {
        result = real_openat(dirfd, pathname, flags);
    }
    va_end(args);

    return result;
}

/**
 * Hook系统fopen函数
 */
extern "C" FILE *fopen(const char *pathname, const char *mode)
{
    static FILE *(*real_fopen)(const char *, const char *) = nullptr;
    if (!real_fopen)
    {
        real_fopen = (FILE * (*)(const char *, const char *)) dlsym(RTLD_NEXT, "fopen");
        if (!real_fopen)
            return nullptr;
    }

    int target_fd = handle_target_file(pathname);
    if (target_fd >= 0)
    {
        FILE *fp = fdopen(target_fd, mode);
        if (!fp)
        {
            close(target_fd);
        }
        return fp;
    }

    return real_fopen(pathname, mode);
}

__attribute__((constructor)) static void launcher_init()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)))
    {
        printf("NapCat启动器初始化，当前工作目录: %s\n", cwd);
    }
    else
    {
        printf("NapCat启动器初始化\n");
    }
}

__attribute__((destructor)) static void launcher_cleanup()
{
    if (g_modified_package_json)
    {
        free(g_modified_package_json);
        g_modified_package_json = nullptr;
    }
}