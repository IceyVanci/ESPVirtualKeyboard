# build-kit · 通用构建套件

本目录是 **ESPEZUpdater 的独立构建套件**：与运行时网页工具解耦，
由**使用升级工具的项目**内置（vendor）后，在**项目侧**运行，构建产物全部落在**项目目录**，
绝不写入工具仓。机制详见 `docs\04_构建套件与项目集成机制.md`。

## 内容

| 文件 | 说明 |
|---|---|
| `build_common.ps1` | 通用构建驱动（读项目 `build.json`，产物写入 `-OutDir`） |
| `empty_creds.ps1` | L1 空凭据改写（被 `build_common.ps1` 调用） |
| `templates\build.example.json` | 构建档模板 |
| `templates\profile.example.json` | UI 档模板 |

## 项目侧标准布局（推荐：内置套件副本 + 内置运行时工具）

```
<项目>\firmware-updater\
├─ build-kit\                 # ← 复制本目录
├─ runtime\                   # ← 复制 ESPEZUpdater 运行时工具（server.ps1 / public\ / esptool.exe / flash_cli.bat / *.bat）
├─ build.json                 # 项目构建档（含 runtimeToolDir: "runtime"）
├─ profile.json               # 项目 UI 档
└─ dist\                       # ← 构建产物（本套件生成，勿手工编辑）
   ├─ firmware\<变体>\{bootloader,partitions,boot_app0,app}.bin
   │                    + manifest.json + profile.json + VERSION.txt
   ├─ firmware\profile.json
   ├─ firmware\verify.log
   └─ <zip>
```

> 也可把 `build-kit\` 与 `runtime\` 放在项目任意位置，用 `-Build` 指向 `build.json` 即可。

## 用法

```powershell
# 在项目目录下（与 build.json 同级，或指定路径）
# ① 仅固件产物：dist\firmware\... + dist\firmware\verify.log + dist\固件包_v<版本>.zip
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-kit\build_common.ps1 -Build .\build.json

# ② 自包含分发：另将运行时工具复制进 dist\，产出 dist\固件升级工具_v<版本>.zip
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-kit\build_common.ps1 -Build .\build.json -Bundle

# 自定义产物目录 / 调试
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-kit\build_common.ps1 -Build .\build.json -OutDir .\dist -Bundle -KeepTmp
```

## 参数

| 参数 | 说明 |
|---|---|
| `-Build <path>` | **必填**，项目侧 `build.json`；相对路径按其所在目录解析 |
| `-OutDir <dir>` | 产物目录，缺省 `<build.json 目录>\dist` |
| `-Bundle` | 自包含模式：复制运行时工具到 `-OutDir` 并打包完整升级工具 zip |
| `-SkipZip` | 不打包 zip（调试） |
| `-KeepTmp` | 保留 `build_tmp\`（调试） |

## 构建档关键字段

见 `templates\build.example.json`。要点：

- `sketchDir` 指向**只读**项目源码根目录；
- `fqbn` 必须与该项目的 Arduino IDE 菜单一致（尤其 `CDCOnBoot=cdc`、`FlashMode=dio`）；
- `credentialMacros` 列全所有含真实凭据的宏（含多行 PEM）；
- `genericNeedles` / `allowValues` 按项目填好；
- `profileSource` 指向项目 UI 档（相对 `build.json`）；
- `runtimeToolDir`：`-Bundle` 时运行时工具来源目录（相对 `build.json`，缺省尝试 `<kitDir>\runtime`）。

## 空凭据保障链（fail-closed）

L1 源层（`empty_creds.ps1`）→ L2 源码层（断言副本宏全空 + 扫描敏感特征/内网 IP）→ L3 产物层（四件套
ASCII + UTF-16LE 字节扫描）。任一层失败即中止，不产出可分发产物；通过后写 `firmware\verify.log`。
