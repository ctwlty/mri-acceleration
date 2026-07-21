# Change Template

Create one folder per non-trivial task:

```text
.harness/changes/YYYY-MM-DD-short-name/
├── requirement.md
├── plan.md
├── decision-log.md
├── affected-files.md
├── review.md
├── test-report.md
└── handoff.md
```

## decision-log.md

```markdown
# Decision Log

## Pending Decisions

| Date | Gate | Question | Options | Recommendation | Status |
| --- | --- | --- | --- | --- | --- |

## Confirmed Decisions

| Date | Gate | Decision | Rationale | Confirmed By |
| --- | --- | --- | --- | --- |

## Assumptions Used

| Date | Assumption | Why Safe | Reversal |
| --- | --- | --- | --- |
```
