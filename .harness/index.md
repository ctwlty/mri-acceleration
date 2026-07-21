# Harness Index

## Project Purpose

面向科研与教学的场景化核磁共振 Windows 上位机方案与原型。

## Load Order

1. Read this file.
2. Read relevant rules in `.harness/rules/`.
3. Read relevant knowledge files in `.harness/knowledge/`.
4. Read `.harness/tools/registry.json` before running checks.
5. For active work, use `.harness/changes/<change-id>/`.
6. For material decisions, use `.harness/changes/<change-id>/decision-log.md` and `.harness/decisions/`.

## Profile

- Detected profile: `generic`
- Installed: `2026-07-18`

## Critical Paths

- Scene template workflow
- Device connection and precheck
- Sequence selection and scan control
- Result preview and analysis indicators

## Required Gates

- Readable product documentation
- Local-open prototype HTML
- Prototype PNG renders correctly

## Decision Gates

- Product scope, architecture, data, security/auth, public API, cost/vendor, deployment/runtime, destructive operations, and scope expansion require explicit human approval before execution.
- Safe low-risk assumptions must be recorded in the active `decision-log.md`.

## Domain Invariants

- Preserve the scientific workflow ordering.
- Keep template-driven interaction above raw DLL complexity.
- Keep the current scope limited to research and teaching.
