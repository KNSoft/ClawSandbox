# ClawSandbox

![ClawSandbox](./Source/Assets/ClawSandbox.ico)

ClawSandbox is a **kernel-level safety sandbox** that prevents OpenClaw from accidentally damaging important files.

Official site: https://knsoft.org/ClawSandbox/

[Official download (Windows x64)](https://knsoft.org/ClawSandbox/ClawSandbox_v1.0.0.7z) | [GitHub releases](https://github.com/KNSoft/ClawSandbox/releases)

## Control Rules

- Reading any file is allowed.
- Writing is only allowed in:
  - temporary file directories
  - OpenClaw-related directories, such as paths containing `openclaw`

## How to Use

- Run `ClawSandbox.exe`. It will automatically install the driver service, then click to start the sandbox.
- Run OpenClaw. OpenClaw will then be subject to the file access controls described above.
- When you exit ClawSandbox, the automatically installed service will also be removed from the system.

**Make sure OpenClaw is started only after ClawSandbox is already running.**

**Released builds are provided for personal learning only. Because they do not have a trusted EV signature, they may be blocked by security software. If you can provide one, please let us know.**

**For any other use, you should build and publish it yourself. This project is licensed under the MIT License.**
