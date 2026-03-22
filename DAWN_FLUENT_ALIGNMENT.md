# Dawn Fluent Component Alignment (Current Snapshot)

This checklist maps the FluentUI component guidance in `dawn.md` to the current project state.

## Implemented

- `FluWindow` + `FluNavigationView`: main shell and top-level navigation are live in `ui/qml/main_fluent.qml`.
- `FluAppBar`: app bar integrated in `ui/qml/main_fluent.qml`.
- `FluInfoBar`: event/status toast feedback used via `FluWindow.showSuccess/showError` in pages (for example `ContentCenterPage.qml`, `InstancesPage.qml`, `SettingsPage.qml`).
- `FluContentDialog`: confirm flow implemented for cache cleanup in `ui/qml/pages/SettingsPage.qml`.
- `FluProgressBar` / `FluProgressRing`: live progress surfaces are wired in queue/install/repair pages:
  - `ui/qml/pages/DownloadQueuePage.qml`
  - `ui/qml/pages/ContentCenterPage.qml`
  - `ui/qml/pages/LogsRepairPage.qml`
- `FluCarousel`: homepage recommendation/workspace carousel is integrated in `ui/qml/pages/HomePage.qml`.
- `FluFlipView`: homepage vertical workspace preview is integrated in `ui/qml/pages/HomePage.qml`.
- `FluDatePicker` / `FluTimePicker`: scheduled backup policy controls are integrated in `ui/qml/pages/SettingsPage.qml`.
- Welcome/onboarding media panel: real playback is integrated in `ui/qml/pages/FirstLaunchWizardPage.qml` via `QtMultimedia` (`MediaPlayer` + `VideoOutput`) with interactive controls, limited to onboarding only.
- `FluTreeView`: dependency tree implemented in `ui/qml/pages/ContentCenterPage.qml`.
- `FluTableView`: structured tables implemented across pages:
  - `ui/qml/pages/ContentCenterPage.qml`
  - `ui/qml/pages/DownloadQueuePage.qml`
  - `ui/qml/pages/HomePage.qml`
  - `ui/qml/pages/InstancesPage.qml`
  - `ui/qml/pages/LogsRepairPage.qml`
- `FluPivot`: instance workbench tabbed surface in `ui/qml/pages/InstancesPage.qml`.
- `FluPagination`: integrated across large data tables:
  - `ui/qml/pages/ContentCenterPage.qml`
  - `ui/qml/pages/DownloadQueuePage.qml`
  - `ui/qml/pages/HomePage.qml`
  - `ui/qml/pages/InstancesPage.qml`
  - `ui/qml/pages/LogsRepairPage.qml`
- `FluTooltip`: explicit domain hints integrated in settings and instance workbench batch actions.
- `FluScrollBar`: explicit page-level scrollbars (`ScrollBar.vertical: FluScrollBar {}`) are wired across wizard/settings/content/queue/instances/logs pages.
- `FluToggleSwitch`, `FluSpinBox`, `FluTextBox`, `FluComboBox`, `FluButton`, `FluFilledButton`, `FluFrame`, `FluDivider`: used across core pages/components.

## Partially Implemented / Needs Expansion

- Motion/animation semantics in `dawn.md` are only partially codified; major workflows are functional, but timing/transition polish is not yet standardized by a dedicated animation token system.

## Not Yet Implemented (From `dawn.md` Suggested Usage)

- `FluMediaPlayer`: upstream component is still unavailable in the current integrated `zhuzichu520/FluentUI` snapshot, so Dawn uses a non-fake `QtMultimedia` implementation for the onboarding video panel until FluentUI exposes the control on mainline.

## Notes

- Current QML pages no longer use raw `Rectangle` containers for page-level semantic blocks; these were migrated to Fluent semantic containers (`FluFrame`, tables, trees, panels).
- `EventCenter` has been unified to `EventCenterPanel` to avoid duplicate filter/list implementations.
- `InstancesPage` workbench tabs now render real per-instance filesystem/runtime data (`mods`, `resourcepacks`, `shaderpacks`, `saves`, `logs`, runtime and advanced settings summary) via `AppViewModel.activeInstanceAssets`.
- `InstancesPage` asset tables now expose real actions: open path, remove asset, and enable/disable content packs through persisted filesystem operations.
- `InstancesPage` asset management now includes `FluContentDialog` delete confirmation and batch actions (`Enable All`, `Disable All`, `Delete Disabled`, `Clear All`) backed by real filesystem commands.
- Asset operations from `InstancesPage` (`toggle/open/delete/batch`) now emit Event Center records with source type `instance_asset` for action-level auditing.
