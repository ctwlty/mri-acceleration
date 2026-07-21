# Handoff

## Completed

- Rebuilt the left panel as a clear three-step selector: primary scene, target object, recommended template.
- Moved selection content into a scroll area so it cannot overlap with bottom control buttons.
- Updated list styling for clearer hierarchy.
- Synced source changes to `/Users/martin/Project/nuclear_system`.
- Verified build and offscreen startup in both source copies.

## Run

```bash
cd /Users/martin/Project/nuclear_system/client
cmake --build build -j
./build/scenario_nmr_client
```

## Follow-Up

- Do a real visual check in the Qt window after closing any old running instance.
- Later SDK work should add a separate parameter-mapping layer instead of reading values directly from UI text.
