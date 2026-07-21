# Scenario NMR Client

Qt/C++ desktop client skeleton for the scenario-based nuclear magnetic resonance workflow.

## Structure

- `src/app/SceneTemplate.h`: template data model
- `src/app/SceneCatalog.*`: sample scene templates
- `src/app/DeviceBridge.*`: device/session state adapter
- `src/app/MainWindow.*`: polished three-column shell
- `resources/app.qss`: visual theme

## Intent

- Keep the UI calm, dense, and readable.
- Put template-driven interaction above raw DLL complexity.
- Use a stable shell that can later host real DLL calls.

## Build

Requires Qt 6 Widgets.

## Test

1. Click `加载 SDK` and select a real `mridll.dll`.
2. If no DLL is selected, the app falls back to Demo mode.
3. Use `一键建链`, `校准向导`, `开始采集` to verify the control flow.

## Notes

- The app reads `init.ini` and `par0423.par` from the demo resource path.
- Real SDK mode follows the same flow as the working `eggcontrollerV2` demo: `Init -> ConfigFile -> SetOutputPath -> SetParameterFile -> Run -> ScanStatus/ScanCompleted -> Abort -> CloseSys`.
