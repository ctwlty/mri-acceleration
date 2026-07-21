# Decision Log

## 2026-07-20

- Implemented DRY_RUN before real SDK writes.
- Kept `Run HOLD` intact.
- Used a whitelist instead of parsing parameter display text.
- Added only partial field mappings for currently important protocols; unknown protocols are marked `pending`.
- DRY_RUN files are written under the application directory in `dry_run_params/`.
