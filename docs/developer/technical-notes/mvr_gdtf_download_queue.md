# MVR GDTF download queue lifecycle

`gui/gdtf_download_queue_dialog.{h,cpp}` owns widgets, row presentation,
summary/progress formatting, cancellation requests and terminal acknowledgement.
The importer owns catalog loading, matching, source selection, reuse and fallback
assignment. No matching, conflict policy or serialization rules change.

`mvr::preferences::kAutoCloseGdtfDownloadWindowKey` uses the existing user
preference store (`user_config.json`). Only `1` enables automatic dismissal;
missing and invalid values keep the queue visible. The Preferences container
commits the page's staged edits using its existing save service. Credentials
remain a separate concern. Headless workflows do not consume this GUI policy.

The synchronous importer enters one standard wxWidgets modal dialog before
starting the queue. A dialog-owned one-shot event starts the existing workflow;
completion enables OK and either awaits acknowledgement or ends that same modal
boundary. There is no second completion dialog, manual wait loop, or new yield
checkpoint. While active, OK is ignored and close/Escape requests cancellation
without destroying the stack-owned dialog. After work returns, cancellation is
terminal and the normal completion preference applies. Exceptions unwind through
the modal boundary and are rethrown to the caller. Progress callbacks remain
synchronous and are not posted to a window after its lifetime ends.

## Existing limitations and future shared workflow

Network requests still run synchronously, and existing import yield checkpoints
remain. Cancellation responsiveness during an individual blocking HTTP transfer
is limited by that existing design. This change does not add worker-thread UI
access or additional yield-based waiting. The modal boundary disables its parent
while the queue runs and prevents premature resumption of conflict assignment.

A later Core service could own an immutable queue request, per-item resolution
results, transferred/total bytes, cancellation token and structured completion
and diagnostic events. The existing GDTF Share client already exposes structured
transfer progress and result categories; those values can feed GUI, CLI and
future MCP/AI adapters. Importer-specific catalog/matching orchestration still
uses wx strings, logging and fixture assignment and is not yet such a service.
GUI labels, colors, dialog acknowledgement and automatic window closing must
remain in GUI adapters. No new CLI command or Core GUI dependency is introduced.
