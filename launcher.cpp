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
#include <libgen.h>

const char *TARGET_PACKAGE_JSON = "resources/app/package.json";
const char *TARGET_PACKAGE_JSON_ALT = "/opt/QQ/resources/app/package.json";
const char *TARGET_NAPCAT_JS = "resources/app/loadNapCat.js";

// 注入的 JS 内容
const char *NAPCAT_JS_CONTENT =
    "const path = require('path');"
    "const CurrentPath = process.env.NAPCAT_BOOTMAIN || path.dirname(__filename);"
    "(async () => {"
    "    await import('file://' + path.join(CurrentPath, './napcat/napcat.mjs'));"
    "})();";

// package.json 替换内容
const char *ORIGINAL_MAIN = "\"main\": \"./application.asar/app_launcher/index.js\"";
// NEW_MAIN 由 get_modified_packagejson 动态生成
static char g_new_main[PATH_MAX + 128] = {0};

// 缓存修改后的 package.json
static char *g_modified_package_json = nullptr;

// 标记 loadNapCat.js 是否已生成
static bool g_loadnapcat_generated = false;

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
 * 计算 from_dir 到 to_dir 的相对路径 - 改进版本
 */
static int get_relative_path(const char *from_dir, const char *to_dir, char *out, size_t out_size)
{
    char from[PATH_MAX], to[PATH_MAX];

    // 获取绝对路径
    if (!realpath(from_dir, from) || !realpath(to_dir, to))
        return -1;

    // 确保路径以 '/' 结尾，便于处理
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);

    if (from[from_len - 1] != '/')
    {
        strcat(from, "/");
        from_len++;
    }
    if (to[to_len - 1] != '/')
    {
        strcat(to, "/");
        to_len++;
    }

    // 找到公共前缀
    size_t common_len = 0;
    while (common_len < from_len && common_len < to_len && from[common_len] == to[common_len])
    {
        common_len++;
    }

    // 回退到最后一个完整的目录分隔符
    while (common_len > 0 && from[common_len - 1] != '/')
    {
        common_len--;
    }

    // 计算需要向上跳转的层数
    size_t up_levels = 0;
    for (size_t i = common_len; i < from_len; i++)
    {
        if (from[i] == '/')
        {
            up_levels++;
        }
    }

    // 构建相对路径
    char result[PATH_MAX] = {0};

    // 添加 "../" 部分
    for (size_t i = 0; i < up_levels; i++)
    {
        strcat(result, "../");
    }

    // 添加目标路径的剩余部分
    if (common_len < to_len)
    {
        strcat(result, to + common_len);
        // 移除末尾的 '/'
        size_t result_len = strlen(result);
        if (result_len > 0 && result[result_len - 1] == '/')
        {
            result[result_len - 1] = '\0';
        }
    }

    // 如果结果为空，表示是同一目录
    if (strlen(result) == 0)
    {
        strcpy(result, ".");
    }

    // 复制结果
    strncpy(out, result, out_size - 1);
    out[out_size - 1] = '\0';

    return 0;
}

/**
 * 读取并修改 package.json 内容 - 改进版本
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

    // 计算 package.json 所在目录和当前工作目录的相对路径
    char pkg_dir[PATH_MAX], cwd[PATH_MAX], relpath[PATH_MAX];

    // 获取 package.json 所在的目录
    strncpy(pkg_dir, TARGET_PACKAGE_JSON_ALT, PATH_MAX - 1);
    pkg_dir[PATH_MAX - 1] = '\0';

    // 获取目录部分（去掉文件名）
    char *last_slash = strrchr(pkg_dir, '/');
    if (last_slash)
    {
        *last_slash = '\0';
    }

    // 获取当前工作目录
    if (!getcwd(cwd, sizeof(cwd)))
    {
        free(buffer);
        return nullptr;
    }

    // 计算相对路径
    if (get_relative_path(pkg_dir, cwd, relpath, sizeof(relpath)) != 0)
    {
        free(buffer);
        return nullptr;
    }

    // 构造新的 main 字段
    snprintf(g_new_main, sizeof(g_new_main),
             "\"main\": \"%s/loadNapCat.js\"", relpath);

    size_t prefix_size = main_pos - buffer;
    size_t new_main_len = strlen(g_new_main);
    size_t suffix_size = strlen(main_pos + strlen(ORIGINAL_MAIN));
    size_t new_size = prefix_size + new_main_len + suffix_size;

    char *modified = (char *)malloc(new_size + 1);
    if (!modified)
    {
        free(buffer);
        return nullptr;
    }

    memcpy(modified, buffer, prefix_size);
    memcpy(modified + prefix_size, g_new_main, new_main_len);
    memcpy(modified + prefix_size + new_main_len,
           main_pos + strlen(ORIGINAL_MAIN), suffix_size);
    modified[new_size] = 0;

    free(buffer);
    g_modified_package_json = modified;

    printf("[launcher] Generated relative path: %s\n", relpath);
    printf("[launcher] New main field: %s\n", g_new_main);

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
 * 启动时生成 loadNapCat.js 到当前目录
 */
__attribute__((constructor))
static void generate_loadnapcat_js()
{
    if (g_loadnapcat_generated)
        return;
    FILE *fp = fopen("loadNapCat.js", "w");
    if (fp)
    {
        fwrite(NAPCAT_JS_CONTENT, 1, strlen(NAPCAT_JS_CONTENT), fp);
        fclose(fp);
        g_loadnapcat_generated = true;
        printf("[launcher] loadNapCat.js generated in current directory\n");
    }
    else
    {
        printf("[launcher] failed to generate loadNapCat.js in current directory\n");
    }
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
