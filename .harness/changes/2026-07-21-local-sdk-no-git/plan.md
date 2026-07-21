# Plan

## Change Scope

- Add ignore rules for SDK/runtime/build artifacts.
- Add local SDK setup note.
- Remove tracked SDK/build artifacts from the Git index only.

## Verification

- `git ls-files` should no longer list ignored SDK/build/runtime artifacts.
- `git check-ignore -v` should report the expected ignore rules.
- Existing local files should remain on disk.

## Risk

- Accidental deletion if `git rm` is run without `--cached`.
- Future contributors may re-add SDK files unless README and `.gitignore` stay aligned.
