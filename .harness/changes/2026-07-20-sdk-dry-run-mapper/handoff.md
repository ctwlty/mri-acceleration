# Handoff

## Completed

- Added ProtocolMapper with field whitelist statuses.
- Added DRY_RUN `.dryrun.par` generation.
- Added DeviceBridge diagnostic signal and DRY_RUN action.
- Added left-side `DRY_RUN` button.
- Added right-side `SDK 诊断 / 字段白名单` panel.
- Synced changes to `/Users/martin/Project/nuclear_system`.
- Verified build and offscreen startup in both source copies.

## Run

```bash
cd /Users/martin/Project/nuclear_system/client
cmake --build build -j
./build/scenario_nmr_client
```

Click `DRY_RUN` after selecting a template. The generated file is written under:

```text
/Users/martin/Project/nuclear_system/client/build/dry_run_params/
```

## Next Step

Compare generated DRY_RUN files with the known working `par0423.par` and the `testDLL` flow before any real SDK parameter write or `Run()` enablement.
