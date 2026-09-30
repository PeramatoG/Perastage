#!/usr/bin/env bash
set -euo pipefail

appimage="${1:?Usage: $0 <AppImage> <log-file>}"
log_file="${2:?Usage: $0 <AppImage> <log-file>}"
mkdir -p "$(dirname "$log_file")"
x11_log="${log_file}.x11"

display_number=99
export DISPLAY=":$display_number"
Xvfb "$DISPLAY" -screen 0 1280x800x24 >"${log_file}.xvfb" 2>&1 &
xvfb_pid=$!
app_pid=""

cleanup() {
  if [ -n "$app_pid" ] && kill -0 "$app_pid" 2>/dev/null; then
    kill -TERM "$app_pid" 2>/dev/null || true
    for _ in $(seq 1 10); do
      kill -0 "$app_pid" 2>/dev/null || break
      sleep 1
    done
    kill -KILL "$app_pid" 2>/dev/null || true
  fi
  kill "$xvfb_pid" 2>/dev/null || true
  wait "$xvfb_pid" 2>/dev/null || true
}
trap cleanup EXIT

write_x11_diagnostics() {
  (
    set +e
    echo "AppImage launcher PID: $app_pid"
    echo "---- Process list ----"
    ps -ef --forest
    echo "---- Visible X11 windows ----"
    visible_windows="$(xdotool search --onlyvisible --name '.*' 2>/dev/null)"
    if [ -z "$visible_windows" ]; then
      echo "No visible X11 windows found."
    fi
    while read -r window_id; do
      [ -n "$window_id" ] || continue
      title="$(xdotool getwindowname "$window_id" 2>/dev/null)"
      printf 'Window ID: %s\nTitle: %s\n' "$window_id" "$title"
      xwininfo -id "$window_id" | sed -n '/Map State:/p'
      if command -v xprop >/dev/null 2>&1; then
        xprop -id "$window_id" _NET_WM_PID WM_CLASS WM_NAME _NET_WM_NAME
      fi
      echo
    done <<< "$visible_windows"
    echo "---- X11 root window tree ----"
    xwininfo -root -tree
  ) >> "$x11_log" 2>&1 || true
}

for _ in $(seq 1 50); do
  xdpyinfo >/dev/null 2>&1 && break
  sleep 0.1
done
xdpyinfo >/dev/null 2>&1 || { echo "Xvfb did not become ready" >&2; exit 1; }

APPIMAGE_EXTRACT_AND_RUN=1 GDK_BACKEND=x11 QT_QPA_PLATFORM=xcb \
  "$appimage" >"$log_file" 2>&1 &
app_pid=$!
echo "AppImage launcher PID: $app_pid" > "$x11_log"

for _ in $(seq 1 30); do
  if ! kill -0 "$app_pid" 2>/dev/null; then
    wait "$app_pid" || status=$?
    echo "AppImage exited before showing its main window (status ${status:-0})." >&2
    write_x11_diagnostics
    cat "$log_file" >&2
    cat "$x11_log" >&2
    exit 1
  fi

  while read -r window_id; do
    [ -n "$window_id" ] || continue
    title="$(xdotool getwindowname "$window_id" 2>/dev/null || true)"
    state="$(xwininfo -id "$window_id" 2>/dev/null | sed -n 's/.*Map State: //p')"
    if [[ "$title" =~ [Pp]erastage ]] && [ "$state" = IsViewable ]; then
      echo "Visible Perastage window detected: id=$window_id title=$title"
      xdotool windowclose "$window_id"
      for _ in $(seq 1 10); do
        kill -0 "$app_pid" 2>/dev/null || break
        sleep 1
      done
      if kill -0 "$app_pid" 2>/dev/null; then
        echo "Application did not exit after its window was closed; sending SIGTERM." >&2
        kill -TERM "$app_pid"
      fi
      wait "$app_pid" || true
      app_pid=""
      exit 0
    fi
  done < <(xdotool search --onlyvisible --name '[Pp]erastage' 2>/dev/null || true)
  sleep 1
done

echo "No visible Perastage top-level window appeared within 30 seconds." >&2
write_x11_diagnostics
cat "$log_file" >&2
cat "$x11_log" >&2
exit 1
