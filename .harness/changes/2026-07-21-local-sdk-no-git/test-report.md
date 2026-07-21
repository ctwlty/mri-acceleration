# Test Report

## Checks

- `git check-ignore -v client/build/scenario_nmr_client eggcontrollerV2/Iface/mriRely/mridll.dll testDLL/bin/x64/Debug/mridll.dll`
- `git ls-files | rg '(^|/)(build|bin|obj|Log|log_data)/|\\.(dll|lib|exe|pdb|fid|nii|gz|log|raw|o|d|a)$|\\.DS_Store$'`

## Result

- Ignore rules matched the expected paths.
- No tracked ignored artifacts were reported by the filtered `git ls-files` check.
