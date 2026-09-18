#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workflow_header="$repo_root/gui/gdtf_share_download_workflow.h"
workflow_source="$repo_root/gui/gdtf_share_download_workflow.cpp"
menu_source="$repo_root/gui/mainwindow_menu.cpp"
window_source="$repo_root/gui/mainwindow.cpp"

if rg -q '#include[[:space:]]+[<"]mainwindow\.h[>"]|\bMainWindow\b' \
    "$workflow_header" "$workflow_source"; then
  echo "GDTF Share download workflow must not depend on MainWindow." >&2
  exit 1
fi

"${PERASTAGE_TEST_PYTHON:-python3}" - \
    "$menu_source" "$window_source" <<'PY'
import re
import sys

menu_text, window_text = (open(path, encoding="utf-8").read() for path in sys.argv[1:])
start = menu_text.find("void MainWindow::OnDownloadGdtf")
end = menu_text.find("// Opens the dictionary editor dialog.", start)
if start < 0 or end < 0:
    raise SystemExit("Missing MainWindow::OnDownloadGdtf handler")
body = menu_text[start:end]
if "RunGdtfShareDownloadWorkflow(this, configManager, callbacks);" not in body:
    raise SystemExit("OnDownloadGdtf must delegate to the GDTF Share GUI workflow")
for forbidden in (
    "GdtfShareClient", "GdtfCatalogService", "GdtfSearchDialog",
    "GdtfLoginDialog", "wxFileDialog", "CredentialStore",
    "GdtfCatalogRefreshResult", "DownloadWithExpiredSessionRetry",
):
    if forbidden in body:
        raise SystemExit(f"OnDownloadGdtf still owns orchestration through {forbidden}")
if not re.search(
    r"EVT_MENU\s*\(\s*ID_Tools_DownloadGdtf\s*,\s*MainWindow::OnDownloadGdtf\s*\)",
    window_text,
):
    raise SystemExit("Download GDTF menu command no longer routes to OnDownloadGdtf")
print("OK: MainWindow delegates GDTF Share download workflow across a narrow boundary.")
PY
