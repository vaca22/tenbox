# TenBox 架构文档

## 1. 项目概述

TenBox 是一个跨平台的虚拟机监控器（Virtual Machine Monitor, VMM），用于在个人计算机上安全地运行 AI 代理。每个代理运行在隔离的虚拟机中，只能访问用户明确授权的文件，从而保护隐私和数据安全。

### 1.1 核心特性

- **跨平台虚拟化后端**：Windows 使用 WHVP（Windows Hypervisor Platform），macOS Apple Silicon 使用 Hypervisor Framework
- **原生 GUI 管理器**：Windows 使用 Win32，macOS 使用 SwiftUI/AppKit
- **Linux 启动支持**：支持标准的 `vmlinuz` / `Image` 内核和 `initramfs`
- **VirtIO MMIO 设备**：块设备、网络、GPU、输入、串口、音频和文件系统
- **磁盘镜像支持**：qcow2 和 raw 格式，支持 zlib 和 zstd 压缩
- **GPU 显示**：virtio-gpu 配合 SPICE 协议，可调整大小的显示窗口
- **音频输出**：virtio-snd 通过 WASAPI（Windows）和 CoreAudio（macOS）流式传输
- **共享文件夹**：virtiofs（virtio-fs），每个 VM 可配置，支持只读模式
- **剪贴板共享**：通过 SPICE vdagent 协议实现双向主机 ↔ 客户机剪贴板
- **客户机代理**：qemu-guest-agent 集成，用于 VM 生命周期管理
- **NAT 网络**：内置 DHCP 服务器、TCP/UDP NAT 代理、通过 lwIP 的 ICMP 中继
- **端口转发**：将客户机 TCP 服务暴露到主机端口
- **多 VM 管理**：创建、编辑、启动、停止、重启和删除 VM；配置持久化为 `vm.json`
- **平台特定机器模型**：共享 VMM 核心，支持 x86_64 和 aarch64 客户机

## 2. 系统架构

### 2.1 双进程设计

TenBox 采用双进程架构，将用户界面和虚拟机运行时分离：

```
┌──────────────────────────────────────────────────────────────────┐
│  tenbox-manager.exe / TenBox.app                                 │
│                                                                  │
│  原生管理器 UI                                                    │
│  ├─ Windows: Win32 管理器 (`src/manager/`)                       │
│  ├─ macOS: SwiftUI/AppKit 管理器 (`src/manager-macos/`)          │
│  ├─ VM 列表、创建/编辑流程、显示、控制台                           │
│  ├─ 剪贴板桥接、共享文件夹、音频播放                               │
│  └─ 设置、镜像源、下载、更新检查                                   │
│                  │ IPC 协议 v1                                   │
│                  │ Windows: Named Pipe                          │
│                  │ macOS: Unix domain socket                     │
│                  ▼                                               │
│  tenbox-vm-runtime  [每个运行的 VM 一个进程]                      │
│  ├─ 主机后端: WHVP (Windows) / HVF (macOS)                       │
│  ├─ 客户机机器模型: x86_64 / aarch64                              │
│  ├─ 地址空间 (PIO / MMIO)                                         │
│  ├─ 平台设备和机器粘合层 (x86_64 / aarch64)                       │
│  ├─ VirtIO MMIO: blk · net · gpu · input · serial · snd · fs    │
│  ├─ vdagent 处理器 (剪贴板)                                       │
│  ├─ guest_agent 处理器                                            │
│  └─ 网络后端: lwIP · DHCP · NAT · 端口转发                        │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 进程间通信（IPC）

- **Windows**：使用命名管道（Named Pipe）进行进程间通信
- **macOS**：使用 Unix 域套接字（Unix domain socket）进行进程间通信
- **协议版本**：IPC 协议 v1，定义在 `src/ipc/` 目录中

### 2.3 进程生命周期

1. **管理器进程**：用户启动管理器应用程序
2. **运行时进程**：管理器为每个 VM 启动一个独立的运行时进程
3. **进程监控**：运行时进程监控父进程（管理器）的退出，自动停止 VM

## 3. 核心组件

### 3.1 源代码目录结构

```
src/
├── common/              # 共享类型: VmSpec, PortForward, SharedFolder
├── core/                # VM 引擎核心
│   ├── arch/            # 架构特定实现
│   │   ├── x86_64/      # x86_64 机器模型、Linux 启动协议、ACPI
│   │   └── aarch64/     # arm64 机器模型、启动流程、FDT
│   ├── device/          # 虚拟设备
│   │   ├── serial/      # UART 16550
│   │   ├── timer/       # i8254 PIT
│   │   ├── rtc/         # CMOS RTC
│   │   ├── irq/         # I/O APIC & i8259 PIC
│   │   ├── acpi/        # ACPI PM 寄存器
│   │   ├── pci/         # PCI 主机桥
│   │   └── virtio/      # VirtIO MMIO + blk/net/gpu/input/serial/snd/fs
│   ├── disk/            # 磁盘镜像处理（qcow2, raw）
│   ├── guest_agent/     # qemu-guest-agent 协议处理器
│   ├── net/             # lwIP NAT 后端
│   ├── vdagent/         # SPICE vdagent（剪贴板协议）
│   └── vmm/             # VM 编排、地址空间和虚拟化接口
│       ├── hypervisor_vm.h    # 抽象 HypervisorVm 接口
│       ├── hypervisor_vcpu.h  # 抽象 HypervisorVCpu 接口
│       ├── address_space.h    # 地址空间管理
│       ├── machine_model.h    # 机器模型抽象
│       └── vm.h               # VM 主类
├── platform/            # 操作系统特定实现
│   ├── windows/
│   │   ├── hypervisor/  # WHVP（Windows Hypervisor Platform）
│   │   ├── console/     # StdConsolePort（Win32 控制台 I/O）
│   │   └── ipc/         # 命名管道传输、共享帧缓冲区
│   └── macos/
│       ├── hypervisor/  # HVF（Hypervisor Framework）
│       ├── console/     # POSIX 控制台 I/O
│       └── ipc/         # Unix 套接字传输
├── ipc/                 # 共享 IPC 协议和传输抽象
├── manager/             # Windows GUI 管理器应用程序
│   ├── ui/              # Win32 GUI: shell、显示、对话框、标签页、WASAPI 音频
│   │   └── audio/       # WASAPI 音频播放器
│   └── (业务逻辑)        # i18n、VM 表单、设置、HTTP 下载、更新检查器
├── manager-macos/       # macOS 管理器（SwiftUI/AppKit + Obj-C++ 桥接）
│   ├── Views/           # SwiftUI 屏幕和显示视图
│   ├── Bridge/          # Swift <-> C++/Obj-C++ IPC 桥接
│   ├── Audio/           # CoreAudio 播放
│   ├── Clipboard/       # 主机剪贴板集成
│   ├── Input/           # 输入处理（键盘捕获）
│   ├── Services/        # 服务层（镜像源、LLM 代理等）
│   └── Resources/       # App bundle 资源、entitlements、着色器
└── runtime/             # VM 运行时进程入口点和 CLI
```

### 3.2 核心模块详解

#### 3.2.1 VMM 核心 (`src/core/vmm/`)

- **`vm.h/cpp`**：VM 主类，协调所有组件
- **`hypervisor_vm.h`**：抽象虚拟化后端接口
- **`hypervisor_vcpu.h`**：抽象 vCPU 接口
- **`address_space.h`**：管理客户机地址空间（PIO/MMIO）
- **`machine_model.h`**：机器模型抽象，支持 x86_64 和 aarch64

#### 3.2.2 架构支持 (`src/core/arch/`)

- **x86_64**：
  - Linux 启动协议实现
  - ACPI 表生成
  - 平台设备初始化
- **aarch64**：
  - 设备树（FDT）生成
  - ARM 启动流程
  - 平台设备初始化

#### 3.2.3 虚拟设备 (`src/core/device/`)

- **VirtIO MMIO 设备**：
  - `virtio_blk`：块设备（磁盘）
  - `virtio_net`：网络设备
  - `virtio_gpu`：GPU 显示（SPICE 协议）
  - `virtio_input`：输入设备（键盘、鼠标）
  - `virtio_serial`：串口设备
  - `virtio_snd`：音频设备
  - `virtio_fs`：文件系统（共享文件夹）
- **平台设备**：
  - UART 16550：串口
  - i8254 PIT：定时器
  - CMOS RTC：实时时钟
  - I/O APIC & i8259 PIC：中断控制器
  - ACPI：电源管理
  - PCI：PCI 主机桥

#### 3.2.4 网络后端 (`src/core/net/`)

- **lwIP 集成**：轻量级 TCP/IP 栈
- **NAT 实现**：网络地址转换
- **DHCP 服务器**：为客户机分配 IP 地址
- **端口转发**：将主机端口映射到客户机端口
- **ICMP 中继**：ICMP 数据包转发

#### 3.2.5 客户机代理 (`src/core/guest_agent/`)

- **qemu-guest-agent 协议**：实现客户机代理协议
- **VM 生命周期管理**：支持客户机发起的操作

#### 3.2.6 vdagent (`src/core/vdagent/`)

- **SPICE vdagent 协议**：实现剪贴板共享协议
- **双向剪贴板同步**：主机 ↔ 客户机

### 3.3 平台特定实现

#### 3.3.1 Windows (`src/platform/windows/`)

- **Hypervisor**：WHVP（Windows Hypervisor Platform）实现
- **Console**：Win32 控制台 I/O
- **IPC**：命名管道传输

#### 3.3.2 macOS (`src/platform/macos/`)

- **Hypervisor**：Hypervisor Framework 实现
- **Console**：POSIX 控制台 I/O
- **IPC**：Unix 域套接字传输

### 3.4 管理器实现

#### 3.4.1 Windows 管理器 (`src/manager/`)

- **UI 框架**：Win32 API
- **功能模块**：
  - VM 列表管理
  - VM 创建/编辑对话框
  - 显示窗口（virtio-gpu）
  - 控制台视图
  - 音频播放（WASAPI）
  - 剪贴板桥接
  - 设置管理
  - 镜像源和下载
  - 更新检查（WinSparkle）

#### 3.4.2 macOS 管理器 (`src/manager-macos/`)

- **UI 框架**：SwiftUI/AppKit
- **功能模块**：
  - **Views/**：SwiftUI 视图（VM 列表、详情、显示、控制台等）
  - **Bridge/**：Swift 与 C++ 的桥接层
  - **Audio/**：CoreAudio 播放
  - **Clipboard/**：主机剪贴板集成
  - **Input/**：输入处理（键盘捕获）
  - **Services/**：服务层（镜像源、LLM 代理）
  - **Resources/**：App bundle 资源

### 3.5 运行时 (`src/runtime/`)

- **入口点**：`main.cpp`
- **CLI 参数解析**：支持命令行参数配置 VM
- **进程管理**：监控父进程退出
- **IPC 服务**：与管理器通信

## 4. 数据流

### 4.1 VM 启动流程

1. 用户通过管理器 UI 创建或选择 VM
2. 管理器启动 `tenbox-vm-runtime` 进程
3. 运行时进程初始化：
   - 创建 Hypervisor VM 和 vCPU
   - 设置地址空间
   - 初始化机器模型（x86_64 或 aarch64）
   - 注册虚拟设备
   - 加载内核和 initramfs
   - 配置磁盘镜像
4. 启动 vCPU 线程
5. 客户机开始执行

### 4.2 显示数据流

1. 客户机通过 virtio-gpu 发送帧缓冲区更新
2. 运行时进程接收更新
3. 通过 IPC 发送到管理器进程
4. 管理器进程在 UI 窗口中渲染

### 4.3 音频数据流

1. 客户机通过 virtio-snd 发送音频数据
2. 运行时进程接收音频流
3. 通过 IPC 发送到管理器进程
4. 管理器进程使用 WASAPI（Windows）或 CoreAudio（macOS）播放

### 4.4 网络数据流

1. 客户机通过 virtio-net 发送网络数据包
2. 运行时进程的 lwIP 栈处理数据包
3. NAT 转换后转发到主机网络
4. 端口转发规则应用到入站连接

### 4.5 剪贴板数据流

1. 主机或客户机剪贴板变化
2. 通过 vdagent 协议同步
3. 管理器进程处理剪贴板事件
4. 同步到另一端

## 5. 构建系统

### 5.1 CMake 构建

- **主 CMakeLists.txt**：项目根配置
- **子目录**：
  - `src/core/CMakeLists.txt`：核心库
  - `src/platform/CMakeLists.txt`：平台特定代码
  - `src/ipc/CMakeLists.txt`：IPC 库
  - `src/runtime/CMakeLists.txt`：运行时可执行文件
  - `src/manager/CMakeLists.txt`：Windows 管理器（仅 Windows）
  - `tests/CMakeLists.txt`：测试

### 5.2 依赖管理

使用 CMake `FetchContent` 自动获取依赖：

- **zlib**：qcow2 zlib 压缩集群解压
- **zstd**：qcow2 zstd 压缩集群解压
- **lwIP**：轻量级 TCP/IP 栈
- **libuv**：跨平台事件循环
- **nlohmann/json**：JSON 序列化
- **WinSparkle**（仅 Windows）：自动更新框架
- **Sparkle**（仅 macOS）：自动更新框架（通过 SwiftPM）

### 5.3 macOS 构建

macOS 管理器使用 Xcode/SwiftPM 单独构建：
- **Package.swift**：Swift 包管理
- **build-macos.sh**：构建脚本，生成 App bundle 和更新 ZIP

## 6. 脚本和工具

### 6.1 镜像构建脚本 (`scripts/`)

- **`x86_64/`**：x86_64 镜像构建脚本
  - `get-kernel.sh`：获取内核
  - `make-initramfs.sh`：构建 initramfs
  - `make-rootfs-*.sh`：构建根文件系统
- **`arm64/`**：arm64 镜像构建脚本（类似结构）
- **`docker/`**：Docker 构建包装器
- **`rootfs-scripts/`**：根文件系统设置脚本（共享）
- **`rootfs-services/`**：systemd 服务单元（共享）
- **`rootfs-configs/`**：根文件系统配置（共享）

### 6.2 工具脚本

- **`mkcpio.py`**：CPIO 归档生成器
- **`image_manager.py`**：镜像源管理助手
- **`build-macos.sh`**：构建 macOS App bundle 和更新 ZIP
- **`make-dmg.sh`**：创建签名的 macOS DMG
- **`publish.py`**：发布工具

## 7. 网站 (`website/`)

- **框架**：Vue 3 + Vite
- **国际化**：vue-i18n（支持中文和英文）
- **功能**：项目网站，展示 TenBox 功能和下载链接

## 8. 网络架构

### 8.1 NAT 网络配置

当启用 NAT 时，TenBox 提供用户模式网络：

| 地址 | 角色 |
|------|------|
| `10.0.2.2` | 网关（主机） |
| `10.0.2.15` | 客户机 IP（通过 DHCP） |
| `8.8.8.8` | DNS 服务器 |

### 8.2 网络功能

- **出站 TCP**：通过 lwIP TCP 栈代理到主机套接字
- **出站 UDP**：由主机网络层直接中继（DNS、NTP 等）
- **ICMP**：在主机 OS 和权限支持的情况下通过原始套接字中继
- **端口转发**：每个 VM 可配置；例如，主机端口 2222 → 客户机端口 22

## 9. 客户机默认配置

由 `scripts/*/make-rootfs-chromium.sh` 构建的客户机默认设置：

| 设置 | 默认值 | 覆盖 |
|------|--------|------|
| Root 密码 | `tenbox` | `ROOT_PASSWORD` 环境变量 |
| 用户账户 | `openclaw` / `openclaw` | `USER_NAME` / `USER_PASSWORD` 环境变量 |
| 主机名 | `tenbox-vm` | — |
| 桌面环境 | XFCE 4 (LightDM) | — |
| 磁盘大小 | 20 GB qcow2 | `ROOTFS_SIZE` 变量 |
| 发行版 | Debian Bookworm | — |
| 预安装软件 | Chrome、Node.js 22、SPICE vdagent、qemu-guest-agent | — |

## 10. 安全考虑

### 10.1 进程隔离

- 每个 VM 运行在独立的运行时进程中
- 管理器进程和运行时进程通过 IPC 通信
- 运行时进程监控父进程退出，自动清理

### 10.2 文件访问控制

- 共享文件夹通过 virtiofs 实现
- 支持只读模式
- 用户明确授权才能访问主机文件

### 10.3 网络隔离

- 默认使用 NAT 网络
- 端口转发需要明确配置
- 客户机网络与主机网络隔离

## 11. 扩展性

### 11.1 架构支持

- 当前支持 x86_64 和 aarch64 客户机
- 架构特定代码隔离在 `src/core/arch/` 中
- 易于添加新的架构支持

### 11.2 设备支持

- VirtIO 设备框架易于扩展
- 新设备可以通过实现 VirtIO 设备接口添加

### 11.3 平台支持

- 平台特定代码隔离在 `src/platform/` 中
- 抽象接口允许添加新平台支持

## 12. 测试

- **测试目录**：`tests/`
- **CMake 集成**：通过 `tests/CMakeLists.txt` 配置

## 13. 版本管理

- **版本文件**：`VERSION` 文件包含版本字符串
- **版本头文件**：构建时生成 `version.h`
- **自动更新**：Windows 使用 WinSparkle，macOS 使用 Sparkle

## 14. 许可证

- **许可证**：GPL v3
- **许可证文件**：`LICENSE`

---

*本文档基于 TenBox 项目源代码结构生成，最后更新：2026-03-20*
