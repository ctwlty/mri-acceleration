# Plan

## Scope

- Rewrite Git history to remove the single oversized blob path.
- Keep the current ignore rules so future日志不会重新进入仓库。

## Verification

- Scan all reachable blobs and confirm no blob over 200MB remains.
- Re-run push after rewrite.

## Risk

- History rewrite changes commit IDs.
- Force push will be required after local verification.
