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

- Writable copy build: passed.
- Project directory build: passed.
- Offscreen startup: passed, manually interrupted after initialization.

## Notes

Qt emitted a `Sans Serif` font alias warning during offscreen startup. It does not block the client.
