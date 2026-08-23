#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
main_source="$repo_root/main.cpp"
splash_source="$repo_root/gui/mainwindow_startup_splash.cpp"

if rg -q 'mainWindow->(Show|Maximize)\(' "$main_source"; then
  echo "MyApp::OnInit must keep MainWindow hidden until final startup composition." >&2
  exit 1
fi

publish_body="$(sed -n '/void MainWindow::PublishInitialMainWindow()/,/void MainWindow::CompleteStartupSplashInitialization()/p' "$splash_source")"
for required_call in 'auiManager->Update()' 'Maximize(true)' 'Show(true)'; do
  if [[ "$publish_body" != *"$required_call"* ]]; then
    echo "Startup publication is missing required final action: $required_call" >&2
    exit 1
  fi
done

echo "OK: MainWindow remains hidden until authoritative startup publication."
