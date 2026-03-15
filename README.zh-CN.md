| [English (en-US)](https://github.com/KNSoft/ClawSandbox/blob/main/README.md) | **简体中文 (zh-CN)** |
| --- | --- |

&nbsp;

# ClawSandbox

![PR Welcome](https://img.shields.io/badge/PR-welcome-0688CB.svg) [![GitHub License](https://img.shields.io/github/license/KNSoft/KNSoft.SlimDetours)](https://github.com/KNSoft/KNSoft.SlimDetours/blob/main/LICENSE)

![ClawSandbox](./Source/Assets/ClawSandbox.ico)

ClawSandbox 是一个**内核级安全沙箱**，防止 OpenClaw 意外地破坏重要文件。

官方网站：https://knsoft.org/ClawSandbox/

[官方下载（Windows x64）](https://knsoft.org/ClawSandbox/ClawSandbox_v1.0.0.7z) | [GitHub 发布](https://github.com/KNSoft/ClawSandbox/releases)

## 控制规则

- 允许读取所有文件
- 只允许写入：
  - 临时文件目录
  - OpenClaw 相关目录（如路径中包含 `openclaw`）

## 如何使用

- 运行 `ClawSandbox.exe` ，此时会自动安装驱动程序服务，点击启动沙箱
- 运行 OpenClaw ，此时 OpenClaw 将受到上述文件访问控制
- 退出 ClawSandbox 时，自动安装的服务也将从系统中删除

**务必在 ClawSandbox 运行后再运行 OpenClaw**

## 协议

**发布的版本仅供个人学习与技术交流使用，由于没有受信任的EV签名，可能被安防软件拦截。如果你可以提供，请让我们知道**

**如需用于其它用途，应自行编译和发布，本项目按MIT开源协议许可**
