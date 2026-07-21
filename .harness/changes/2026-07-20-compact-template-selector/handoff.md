# Handoff

## Completed

- Replaced three large left-side selection lists with compact scene/object dropdowns.
- Added keyword template search.
- Reduced template list height and left splitter width.
- Synced changes to `/Users/martin/Project/nuclear_system`.
- Verified build and offscreen startup in both source copies.

## Run

```bash
cd /Users/martin/Project/nuclear_system/client
cmake --build build -j
./build/scenario_nmr_client
```

## Many-Scenario Strategy

Use `场景/对象` as coarse filters, `搜索/标签/最近/收藏` as fast retrieval, and move toward model-backed filtering when the catalog grows beyond the current static list.
