# Dawn

Dawn is a Windows-first, instance-centric Minecraft launcher shell built with C++20, CMake, Qt 6, QML, and FluentUIbi.

## Build

### Headless core build

This mode does not require Qt or FluentUIbi and is the default.

```powershell
cmake -S . -B build -DDAWN_BUILD_TESTS=ON -DDAWN_ENABLE_QT=OFF
cmake --build build
ctest --test-dir build
```

### Qt/QML build

Enable this when Qt 6 is installed.

```powershell
cmake -S . -B build -DDAWN_ENABLE_QT=ON -DQt6_DIR="C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6"
cmake --build build
```

If the `external/FluentUIbi` submodule is available, set `-DDAWN_USE_FLUENTUIBI=ON`. When the submodule is absent, Dawn falls back to a local Qt Quick shell so the project still builds.

## Runtime modes

* `DAWN_ENABLE_QT=OFF` builds a headless executable that demonstrates the core services.
* `DAWN_ENABLE_QT=ON` builds the QML launcher shell.
* `DAWN_ENABLE_SQLITE=ON` enables the SQLite skeleton when `SQLite3` is available on the system.

## Architecture

* `app/` contains the executable entry point.
* `core/` contains domain models, interfaces, services, repositories, providers, and the launch pipeline.
* `infra/` contains JSON, filesystem, and other low-level utilities.
* `ui/` contains QML pages and optional Qt-facing view models.

The code is layered so that the core library can be tested without Qt.

## Stage Notes

### P0

* Project skeleton and build system.
* Core models, interfaces, JSON serialization, and file-backed instance storage.
* Headless service entry point.

### P1

* Qt/QML shell.
* View model adapters.
* Launch preflight and task queue wiring.

### P2

* Modrinth protocol adapter.
* Loader installer abstractions.
* Install plan composition.

### P3

* FluentUIbi integration toggles.
* Expanded diagnostics, update simulation, drag-and-drop install, and deeper loader support.

## Capability Matrix

| Area | Status | Notes |
| --- | --- | --- |
| Core instance storage | implemented | JSON-backed instance manifests and workbench model are wired in. |
| Settings | implemented | Global settings model and JSON persistence are available. |
| Accounts | protocol-adapter | Microsoft and offline caches exist; token state helpers and OAuth payloads are wired, transport defaults to fake. |
| Java | scaffolded | Runtime discovery and profile stubs are present. |
| Minecraft versions | scaffolded | Version classification and lookup are local stubs. |
| Loaders | scaffolded | Fabric, Quilt, Forge, NeoForge, and OptiFine profiles are represented. |
| Download queue | scaffolded | Task queue and update simulation are present, not network-backed. |
| Diagnostics | scaffolded | Log rule matching returns human-readable categories. |
| Backups | scaffolded | Snapshot metadata and restore plans are local stubs. |
| Modrinth integration | protocol-adapter | Search/version URL builders and JSON parsing are wired; transport defaults to fake. |
| Microsoft auth | protocol-adapter | Device-code and token payloads are wired; transport defaults to fake. |
| FluentUIbi | planned | The shell is compatible with the submodule but does not depend on it. |

## Known Gaps

* Network transport is fake by default; a concrete HTTP backend is still pending.
* Real Minecraft runtime orchestration and actual file download pipelines are intentionally stubbed.
* FluentUIbi integration is behind a CMake switch and falls back when the submodule is not present.
