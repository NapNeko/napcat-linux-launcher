# napcat-linux-launcher

NapCat Linux 启动器是一个用于NapCat应用程序启动过程的工具，它可以通过动态劫持文件访问来重定向应用程序的入口点。

## 功能特点

- 通过预加载机制劫持应用程序的文件访问
- 动态修改 package.json 中的 main 入口点指向
- 无需修改原始应用程序文件
- 支持 x64 和 ARM64 架构

## 使用方法

### 预备条件

- Linux 操作系统
- 目标应用为基于 Electron 的应用程序
- `LD_PRELOAD` 环境变量支持（大多数 Linux 发行版默认支持）

### 安装步骤

1. 从 下载适合您系统架构的 `libnapcat_launcher.so` 或 `libnapcat_launcher_arm64.so`
2. 将下载好的文件放在适当的位置

### 使用步骤

```bash
LD_PRELOAD=./libnapcat_launcher.so ./your-electron-app
```

```bash
NAPCAT_BOOTMAIN=/path/napcat LD_PRELOAD=./libnapcat_launcher.so ./your-electron-app
```
