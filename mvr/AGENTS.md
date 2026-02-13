# AGENTS.md (mvr)

## Scope
These rules apply to everything under `mvr/`.

## Rules
1. **Primary hotspot**
   - `mvrexporter.cpp` should evolve through responsibility extraction.

2. **Recommended separation when extending exporter logic**
   - Keep decoupled:
     - data normalization/preparation,
     - XML serialization,
     - package/file writing.

3. **Change strategy**
   - Refactor opportunistically around touched paths; avoid massive one-PR changes.
