#include <sys/stat.h>
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

// 拦截 stat
extern "C" int stat(const char *pathname, struct stat *buf) {
    printf("[launcher] stat: %s\n", pathname);
    static int (*real_stat)(const char *, struct stat *) = nullptr;
    if (!real_stat)
        real_stat = (int (*)(const char *, struct stat *))dlsym(RTLD_NEXT, "stat");
    return real_stat(pathname, buf);
}

// 拦截 lstat
extern "C" int lstat(const char *pathname, struct stat *buf) {
    printf("[launcher] lstat: %s\n", pathname);
    static int (*real_lstat)(const char *, struct stat *) = nullptr;
    if (!real_lstat)
        real_lstat = (int (*)(const char *, struct stat *))dlsym(RTLD_NEXT, "lstat");
    return real_lstat(pathname, buf);
}

// 拦截 fstat
extern "C" int fstat(int fd, struct stat *buf) {
    printf("[launcher] fstat: %d\n", fd);
    static int (*real_fstat)(int, struct stat *) = nullptr;
    if (!real_fstat)
        real_fstat = (int (*)(int, struct stat *))dlsym(RTLD_NEXT, "fstat");
    return real_fstat(fd, buf);
}

// 拦截 __xstat
extern "C" int __xstat(int ver, const char *pathname, struct stat *buf) {
    printf("[launcher] __xstat: %s\n", pathname);
    static int (*real___xstat)(int, const char *, struct stat *) = nullptr;
    if (!real___xstat)
        real___xstat = (int (*)(int, const char *, struct stat *))dlsym(RTLD_NEXT, "__xstat");
    return real___xstat(ver, pathname, buf);
}

// 拦截 __lxstat
extern "C" int __lxstat(int ver, const char *pathname, struct stat *buf) {
    printf("[launcher] __lxstat: %s\n", pathname);
    static int (*real___lxstat)(int, const char *, struct stat *) = nullptr;
    if (!real___lxstat)
        real___lxstat = (int (*)(int, const char *, struct stat *))dlsym(RTLD_NEXT, "__lxstat");
    return real___lxstat(ver, pathname, buf);
}

extern "C" int __xstat64(int ver, const char *pathname, struct stat64 *buf) {
    printf("[launcher] __xstat64: %s\n", pathname);
    static int (*real___xstat64)(int, const char *, struct stat64 *) = nullptr;
    if (!real___xstat64)
        real___xstat64 = (int (*)(int, const char *, struct stat64 *))dlsym(RTLD_NEXT, "__xstat64");
    return real___xstat64(ver, pathname, buf);
}

extern "C" int __lxstat64(int ver, const char *pathname, struct stat64 *buf) {
    printf("[launcher] __lxstat64: %s\n", pathname);
    static int (*real___lxstat64)(int, const char *, struct stat64 *) = nullptr;
    if (!real___lxstat64)
        real___lxstat64 = (int (*)(int, const char *, struct stat64 *))dlsym(RTLD_NEXT, "__lxstat64");
    return real___lxstat64(ver, pathname, buf);
}
extern "C" int __xstat64(int ver, const char *pathname, struct stat64 *buf) {
    printf("[launcher] __xstat64: %s\n", pathname);
    static int (*real___xstat64)(int, const char *, struct stat64 *) = nullptr;
    if (!real___xstat64)
        real___xstat64 = (int (*)(int, const char *, struct stat64 *))dlsym(RTLD_NEXT, "__xstat64");
    return real___xstat64(ver, pathname, buf);
}

extern "C" int __fxstat(int ver, int fd, struct stat *buf) {
    printf("[launcher] __fxstat: %d\n", fd);
    static int (*real___fxstat)(int, int, struct stat *) = nullptr;
    if (!real___fxstat)
        real___fxstat = (int (*)(int, int, struct stat *))dlsym(RTLD_NEXT, "__fxstat");
    return real___fxstat(ver, fd, buf);
}

extern "C" int __fxstat64(int ver, int fd, struct stat64 *buf) {
    printf("[launcher] __fxstat64: %d\n", fd);
    static int (*real___fxstat64)(int, int, struct stat64 *) = nullptr;
    if (!real___fxstat64)
        real___fxstat64 = (int (*)(int, int, struct stat64 *))dlsym(RTLD_NEXT, "__fxstat64");
    return real___fxstat64(ver, fd, buf);
}