# Quality Gates

## Always Consider

1. Format/lint/type checks.
2. Unit or integration tests.
3. Build/package checks.
4. Domain smoke tests.
5. Security-sensitive-data checks.

## Registered Tools

See `.harness/tools/registry.json`.

## Completion Rule

Do not mark a change complete until required gates pass, or until skipped gates are explicitly listed with a reason.
