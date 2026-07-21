# Handoff

## Completed

- Replaced the top middle “实验链” pill row with an “操作链” node row.
- Each node now shows step number, node title, and template-specific content.
- Added direction arrows and scroll behavior to avoid overlap.
- Synced changes to `/Users/martin/Project/nuclear_system`.
- Verified build and offscreen startup in both source copies.

## Run

```bash
cd /Users/martin/Project/nuclear_system/client
cmake --build build -j
./build/scenario_nmr_client
```

## Follow-Up

Perform a live visual check and close any old running client before launching the rebuilt binary.
