# UI Growth Checklist

Use this checklist before implementing any new UI feature in Peraviz.

## 1) User value and visibility tier

- [ ] Is this feature for end users (User tier) or for diagnostics (Debug tier)?
- [ ] If it is not essential for the core workflow, should it be Advanced instead of User?
- [ ] Does the default startup remain clean when this feature is disabled?

## 2) Module placement

- [ ] Can this feature be added to an existing module without overloading its responsibility?
- [ ] If not, should a new module be introduced (Panel, Window, Controller, or Policy)?
- [ ] Is script ownership explicit (single primary responsibility per script)?
- [ ] Are names consistent with architecture conventions (`*_panel`, `*_window`, `*_controller`, `*_policy`)?

## 3) Main layout impact

- [ ] Does this feature change the main viewer layout or persistent visible controls?
- [ ] If yes, is the change justified by core user workflow value?
- [ ] Could the same function be delivered as opt-in Advanced/Debug UI instead?
- [ ] Has the team confirmed that scene readability remains the top priority?

## 4) Clean screen by default gate (mandatory)

- [ ] After implementing the feature, default startup still shows only User-tier essentials.
- [ ] Advanced and Debug surfaces remain hidden until explicitly enabled.
- [ ] No new overlay/panel/window appears by default unless required for base flow.

## 5) Integration quality checks

- [ ] Architecture docs are updated when module boundaries or visibility decisions change.
- [ ] The new feature respects existing module boundaries and does not create unnecessary coupling.
- [ ] Any follow-up extraction/refactor is recorded if the touched module is growing too large.
