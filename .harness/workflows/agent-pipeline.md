# Agent Pipeline

## Skill Orchestration

Use this sequence for non-trivial development work:

```text
harness-engineering
  -> product-delivery-planner
  -> harness decision gate approval when required
  -> saas-delivery
  -> quality-acceptance
  -> harness-engineering handoff
```

## Steps

1. Understand: use product-delivery-planner when scope or acceptance criteria are not already precise.
2. Decide: identify mandatory human decision gates and record options in `decision-log.md`.
3. Plan: define implementation steps, gates, risks, and rollback.
4. Execute: use saas-delivery patterns and make scoped edits inside approved scope.
5. Review: inspect behavior, security, style, and domain rules.
6. Verify: use quality-acceptance or registered gates and save evidence.
7. Handoff: summarize completed work, decisions made, and residual risks.
