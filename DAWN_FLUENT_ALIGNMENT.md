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
- `FluTreeView`: dependency tree implemented in `ui/qml/pages/ContentCenterPage.qml`.
- `FluTableView`: structured tables implemented across pages:
  - `ui/qml/pages/ContentCenterPage.qml`
  - `ui/qml/pages/DownloadQueuePage.qml`
  - `ui/qml/pages/HomePage.qml`
  - `ui/qml/pages/InstancesPage.qml`
  - `ui/qml/pages/LogsRepairPage.qml`
- `FluPivot`: instance workbench tabbed surface in `ui/qml/pages/InstancesPage.qml`.
- `FluToggleSwitch`, `FluSpinBox`, `FluTextBox`, `FluComboBox`, `FluButton`, `FluFilledButton`, `FluFrame`, `FluDivider`: used across core pages/components.

## Partially Implemented / Needs Expansion

- `FluPagination`: integrated for high-volume tables in:
  - `ui/qml/pages/ContentCenterPage.qml` (search results + versions)
  - `ui/qml/pages/LogsRepairPage.qml` (repair execution logs)
  Still pending broader rollout to all large tables.
- `FluTooltip`: explicit hints added for advanced settings in `ui/qml/pages/SettingsPage.qml`; further contextual hints can be added across runtime and content expert flows.
- `FluScrollBar`: used internally by Fluent table/tree controls, but no explicit page-level scrollbar strategy doc yet.

## Not Yet Implemented (From `dawn.md` Suggested Usage)

- `FluMediaPlayer`: not available in the current integrated `zhuzichu520/FluentUI` codebase snapshot (no `FluMediaPlayer.qml` in `external/FluentUI/src/Qt6/imports/FluentUI/Controls`), so onboarding media panel remains pending until upstream component availability.

## Notes

- Current QML pages no longer use raw `Rectangle` containers for page-level semantic blocks; these were migrated to Fluent semantic containers (`FluFrame`, tables, trees, panels).
- `EventCenter` has been unified to `EventCenterPanel` to avoid duplicate filter/list implementations.
- `InstancesPage` workbench tabs now render real per-instance filesystem/runtime data (`mods`, `resourcepacks`, `shaderpacks`, `saves`, `logs`, runtime and advanced settings summary) via `AppViewModel.activeInstanceAssets`.
- `InstancesPage` asset tables now expose real actions: open path, remove asset, and enable/disable content packs through persisted filesystem operations.
- `InstancesPage` asset management now includes `FluContentDialog` delete confirmation and batch actions (`Enable All`, `Disable All`, `Delete Disabled`, `Clear All`) backed by real filesystem commands.
- Asset operations from `InstancesPage` (`toggle/open/delete/batch`) now emit Event Center records with source type `instance_asset` for action-level auditing.
