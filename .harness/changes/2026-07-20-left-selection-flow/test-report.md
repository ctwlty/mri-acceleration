# Test Report

## Commands

```bash
cd /Users/martin/Documents/核磁共震/client
cmake --build build -j
QT_QPA_PLATFORM=offscreen ./build/scenario_nmr_client
```

```bash
cd /Users/martin/Project/nuclear_system/client
cmake --build build -j
QT_QPA_PLATFORM=offscreen ./build/scenario_nmr_client
```

## Result

- Local writable copy build: passed.
- Project directory build: passed.
- Offscreen startup: passed, manually interrupted after initialization.

## Notes

Qt emitted a font alias performance warning for `Sans Serif`; this does not block startup or UI initialization.
