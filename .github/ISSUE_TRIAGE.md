# Issue triage policy

This policy keeps issue decisions consistent without treating inactivity as evidence that a problem is resolved.

## Labels

Label categories are complementary:

- `bug` identifies a reported defect.
- `platform:*` identifies operating-system-specific scope.
- `area:*` identifies the affected subsystem. Add narrowly named areas as the project grows rather than maintaining a fixed exhaustive list.
- `type:*` identifies a special technical category, such as a crash.
- `status:*` records the current workflow state.

Apply the useful categories together, but keep status labels mutually exclusive: an issue must have no more than one `status:*` label at a time.

## Confirmed bugs

A confirmed or reproducible bug remains open even if the original reporter stops responding. Silence is not evidence that the bug has disappeared. Inactivity automation must not process a confirmed bug unless a maintainer explicitly changes its status to `status: awaiting-feedback` because progress is genuinely blocked.

## Awaiting feedback

Use `status: awaiting-feedback` only when investigation is blocked by missing information, a requested test, or reporter confirmation. Never apply it merely because an issue is old. When the requested information arrives and the issue is reviewed, remove the label or replace it with the single appropriate status.

The awaiting-feedback workflow warns after 30 days without human activity. If another 14 days pass after that warning without human activity, it removes `status: awaiting-feedback` and closes the issue as not planned. The issue can be revisited when the missing information becomes available.

## Closing issues

### Duplicates

Link a duplicate to its canonical issue with GitHub's duplicate relationship where possible, leave a short and friendly explanation, and close it with the **duplicate** state reason rather than completed.

### Completed

Close as **completed** when the reported problem has been corrected. When release availability is part of the user-facing resolution, wait until the fix is available in an official release. Reference the fixing pull request or release when useful. Thank reporters warmly, especially when they supplied logs, dumps, test files, or development-build testing.

### Not planned

Use **not planned** for deliberate scope decisions or an awaiting-feedback issue that completed the warning and timeout process above. Do not use it instead of investigating a confirmed bug, platform limitation, or upstream issue that still needs tracking.

## Milestones and assignment

Apply a milestone only when work is genuinely planned for that release. Do not assign an issue mechanically when no owner has accepted the work.

## Tone

Keep comments human, appreciative, and concise. Avoid cold boilerplate when someone has spent time testing or providing diagnostics.
