# DLL-Injector

Windows DLL injector with a CLI, an optional MFC GUI, a local launcher, and a Windows service.

Built on [BlackBone](https://github.com/DarthTon/Blackbone). **For learning and for processes you are authorized to modify.** You are responsible for how you use it.

## Features

- **LoadLibrary** (`STANDART`) and **manual map** (`MANUAL`)
- Inject by PID, by launching a process, or by watching for a process / window name
- Configurable delay after a target is found and before the DLL is mapped
- `inject-launcher.bat` for a console session
- Windows service that starts with the machine and reads `inject-service.ini`

## Layout

| Path | Purpose |
| --- | --- |
| `InjectorCLI/` | Console injector and Windows service host |
| `Down'sInjector/` | MFC GUI (manual map into the next focused window) |
| `Payload/` | Local DLLs to inject (not committed) |
| `inject-launcher.bat` | Console watcher; edit process / DLL names here |
| `inject-service.ini.example` | Template for the Windows service |
| `install-service.bat` / `uninstall-service.bat` | Register or remove the service (UAC prompt) |
| `pack-release.bat` | Build a portable zip under `dist/` |
| `External/include/argparse/` | Vendored CLI parser |
| `Blackbone/` | Git submodule |

## Requirements

- Windows 10/11 **x64**
- Visual Studio 2022 with MSVC **v143** and the Windows 10/11 SDK
- For the GUI project: MFC
- [BlackBone](https://github.com/DarthTon/Blackbone) sources in `Blackbone/`

## Build

```bat
git clone <this-repo>
cd DLL-Injector
git submodule update --init --recursive
```

If the recorded BlackBone commit is missing upstream, clone current BlackBone into `Blackbone/` instead:

```bat
git clone https://github.com/DarthTon/Blackbone Blackbone
```

1. Open `Down'sInjector.sln` in Visual Studio 2022.
2. Set **Release | x64**.
3. Build **InjectorCLI** (builds BlackBone first).

Output:

```
build\x64\Release\InjectorCLI.exe
```

Set BlackBone’s C++ language standard to **ISO C++17** if you see:

```
'blackbone::_UNICODE_STRING_T<uint32_t>::type': expected an expression instead of a type
```

This repo already uses C++17 and toolset v143 for the x64 Debug/Release BlackBone configs used by the solution.

## Payloads

1. Copy your DLLs into `Payload/` (subfolders are fine).
2. Point the launcher or the service at those names, relative to `Payload\`, not `C:\...`.

See [Payload/README.md](Payload/README.md).

## Console launcher

Edit the block at the top of `inject-launcher.bat`:

```bat
set "INJECT_DELAY=30"
set "PROC1=target.exe"
set "DLL1=payload.dll"
set "MAP1=STANDART"
```

`DLL1` / `DLL2` are paths under `Payload\`. Mapping is `STANDART` (LoadLibrary) or `MANUAL`.

Double-click `inject-launcher.bat`. It locates `InjectorCLI.exe` next to the script or under `build\x64\Release\`, then watches for the process names. Close the window to stop.

`inject-launcher-hidden.vbs` starts the same launcher with no console.

## Windows service

The service does **not** copy files into Windows. The folder you install from must stay on disk.

```bat
copy inject-service.ini.example inject-service.ini
```

Edit `inject-service.ini` (process names, DLLs, mapping, delay). Then double-click `install-service.bat` and approve UAC.

After ini changes:

```bat
net stop InjectorCLI
net start InjectorCLI
```

Remove with `uninstall-service.bat`. Logs default to `inject-service.log` next to the ini.

`inject-service.ini` is gitignored so machine-specific targets are not committed.

## CLI

```text
InjectorCLI.exe [OPTIONS] <dll_path>
```

| Option | Meaning |
| --- | --- |
| `-m, --mapping STANDART\|MANUAL` | LoadLibrary or manual map |
| `--pid N` | Inject into an existing PID |
| `--process_path PATH` | Start that executable and inject |
| `--steal_handle_job` | Handle-steal start; use with `--process_path` and `MANUAL` |
| `--auto_inject_process NAME` | Stay running; inject when `NAME` appears |
| `--auto_inject_window TITLE` | Stay running; inject when a new window title matches |
| `--inject_delay SECONDS` | Wait after a match, then inject |
| `-x` | Wipe PE headers (manual map) |
| `-i` | Manual imports (manual map) |
| `-t` | No new threads / hijack (manual map) |
| `--service install\|uninstall` | Windows service (Administrator) |
| `--service --config PATH` | SCM entrypoint; used by the service |

Examples:

```bat
InjectorCLI.exe -m STANDART --auto_inject_process target.exe --inject_delay 30 Payload\payload.dll

InjectorCLI.exe -m MANUAL --pid 1234 Payload\payload.dll

InjectorCLI.exe -m MANUAL --steal_handle_job --process_path "C:\app\app.exe" Payload\payload.dll
```

`--auto_inject` from older README text is not a flag. Use `--auto_inject_process`.

## GUI

Build **Down'sInjector** (Release x64). It waits until you focus another window, then manual-maps the selected DLL.

## Portable zip

```bat
pack-release.bat
```

Creates:

- `dist\InjectorCLI\` — runnable folder
- `dist\InjectorCLI-Release.zip`

Unzip on another 64-bit Windows machine, keep the folder together, and either run `inject-launcher.bat` or `install-service.bat`.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| `InjectorCLI.exe was not found` | Build Release x64, or place the exe next to the launcher |
| Target found, then LoadLibrary error 0 | Increase `INJECT_DELAY` / `InjectDelay`; try `MANUAL` |
| Service starts then stops | `inject-service.log`; missing payload DLL or empty targets |
| Service fails after moving the folder | Re-run `install-service.bat` from the new location |
| argparse.hpp missing | `External\include` is on the include path; do not require vcpkg |

## Credits

- [BlackBone](https://github.com/DarthTon/Blackbone) by DarthTon
- [argparse](https://github.com/p-ranav/argparse) (header vendored under `External/include`)
