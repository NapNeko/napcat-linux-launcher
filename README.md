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
- `LD_PRELOAD` 环境变量支持（大多数 Linux 发行版默认支持）

### 安装步骤

1. 从 下载适合您系统架构的 `libnapcat_launcher.so` 或 `libnapcat_launcher_arm64.so`
2. 将下载好的文件放在适当的位置

### 构建说明
```
# 编译 x64 版本
g++ -shared -fPIC launcher.cpp -o libnapcat_launcher.so -ldl

# 编译 ARM64 版本（需要交叉编译工具）
aarch64-linux-gnu-g++ -shared -fPIC -ldl -o libnapcat_launcher_arm64.so launcher.cpp
```
### 使用步骤

```bash
Xvfb :1 -screen 0 1x1x8 +extension GLX +render > /dev/null 2>&1 &
export DISPLAY=:1
LD_PRELOAD=./libnapcat_launcher.so qq
LD_PRELOAD=./libnapcat_launcher.so qq>log.txt
LD_PRELOAD=./libnapcat_launcher.so strace -e trace=file -f -s 200 -o strace.log qq
```