# DownloadAssistant

DownloadAssistant 是一个基于 **Qt** 开发的 Windows 下载工具，使用 **MSVC** 编译，通过 `.pro` 文件进行构建。目前实现了对 **SMB** 协议的下载并支持断点续传。

## 功能特性

- 支持 SMB 下载，提供断点续传能力
- 图形界面可创建、启动、暂停和停止下载任务
- 可自定义下载目录
- 支持多任务并发下载
- 实时显示下载进度、速度和剩余时间
- 任务状态持久化保存
- 支持地址书签，快速填充常用 SMB 目录
- **日志系统**：记录线程ID、文件名和行号，便于调试
- **配置文件**：保存在 exe 同级目录，便于部署


## 项目结构

```
DownloadAssistant/
├── src/                           # 源代码目录
│   ├── main.cpp                   # 主程序入口
│   ├── mainwindow.h               # 主窗口头文件
│   ├── mainwindow.cpp             # 主窗口实现
│   ├── mainwindow.ui              # 主窗口 UI 文件
│   ├── downloadmanager.h          # 下载管理器头文件
│   ├── downloadmanager.cpp        # 下载管理器实现
│   ├── downloadtask.h             # 下载任务头文件
│   ├── downloadtask.cpp           # 下载任务实现
│   ├── smbdownloader.h            # SMB 下载器头文件
│   ├── smbdownloader.cpp          # SMB 下载器实现
│   ├── logger.h                   # 日志系统头文件
│   └── logger.cpp                 # 日志系统实现
├── DownloadAssistant.pro          # Qt 项目文件
├── README.md                      # 项目说明
└── LICENSE                        # 许可证文件
```

## 构建方式

确保安装了 Qt 工具链后，执行以下命令即可完成构建：

```bash
qmake
make       # Windows 环境可使用 nmake
```

`qmake` 在生成 Makefile 时会自动调用 `uic`，从 `mainwindow.ui` 生成 `ui_mainwindow.h`，
因此该文件无需加入版本控制。

## 文件结构

```
DownloadAssistant/
├── DownloadAssistant.exe          # 主程序
├── config.ini                     # 配置文件（自动生成）
├── logs/                          # 日志目录（自动生成）
│   └── downloadassistant.log      # 日志文件
└── ...
```

## 日志功能

程序内置日志系统，记录以下信息：
- 时间戳
- 日志级别（DEBUG/INFO/WARNING/ERROR）
- 线程ID
- 文件名和行号
- 详细消息

日志文件位置：`exe同级目录/logs/downloadassistant.log`

## 配置文件

配置文件位置：`exe同级目录/config.ini`

包含以下设置：
- 默认保存路径
- 最大并发下载数
- 任务列表

## 开发计划

- [ ] 添加下载速度限制功能
- [ ] 支持下载队列管理
- [x] 添加系统托盘功能
- [ ] 支持代理服务器配置
- [ ] 添加下载历史记录
- [ ] 支持批量导入下载任务

## 构建指南 / Build Instructions

当前版本仅支持 Windows，并直接使用 Qt 的文件接口访问 UNC 路径，因此无需 `libsmb2` 库，相关文件已移除。

确保安装 Qt 工具链后，在项目根目录执行：

```bash
qmake
make       # Windows 环境可使用 nmake
```


## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

---

项目仍处于早期阶段，后续将持续完善并新增更多协议和功能。欢迎提出建议或贡献代码！
