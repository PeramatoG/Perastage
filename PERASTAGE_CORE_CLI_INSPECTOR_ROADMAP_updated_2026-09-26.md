# Perastage Core, CLI, and Inspector roadmap

Current status: CLI-220 and GUI-300 are merged. This sanitation pass closes
their inspection-contract follow-up; GUI-310 is the next active implementation
block.

## Completed blocks

- [x] **CLI-220** — Inspection CLI automation contract and complete reports.
  Evidence: merged PR #2398 (`e6057a6`) and the development CLI contract in
  `docs/developer/cli.md`.
- [x] **GUI-300** — Native read-only MVR/GDTF Inspector workspace.
  Evidence: merged PR #2399 (`2c53d18`, `fa8c71b`) and the enforced
  `tests/check_gui_inspector_boundary.sh` boundary.

## Next block

- [ ] **GUI-310** — MVR-specific navigation. Not started.

Later GUI-320 and GUI-330 work remains incomplete and outside this sanitation
pass.
