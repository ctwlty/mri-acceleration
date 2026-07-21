# Test Report

## Baseline

- `git rev-list --objects --all | git cat-file --batch-check='%(objectname) %(objecttype) %(objectsize) %(rest)' | awk '$2=="blob" && $3 > 200000000 {print}'`

## Result

- Found one blob over 200MB:
  - `56c8f572557d721c83d268ec3702b0c5ca2842d4`
  - `testDLL/bin/x64/Debug/Log/20260410/error_2026.4.10.log`
