#!/usr/bin/env python3
"""Select a safe sccache backend from the GitHub event and resolved source."""
from __future__ import annotations

import argparse
import os
from pathlib import Path


def select_scope(event: str, github_ref: str, source_sha: str, trusted_main_sha: str, requested_ref: str) -> dict[str, str]:
    if event == "pull_request" and github_ref.startswith("refs/pull/") and github_ref.endswith("/merge"):
        return {
            "sccache_gha_enabled": "on",
            "sccache_backend": "GitHub Actions Cache",
            "sccache_cache_scope": "gha-pr-merge-ref",
            "sccache_scope_reason": "GitHub isolates writes to the pull request merge ref",
            "cache_warming": "false",
        }
    trusted_request = requested_ref in {"", "main", "refs/heads/main"}
    trusted_push = event == "push" and github_ref == "refs/heads/main" and requested_ref == ""
    trusted_dispatch = event == "workflow_dispatch" and github_ref == "refs/heads/main" and trusted_request
    if (trusted_push or trusted_dispatch) and source_sha == trusted_main_sha:
        reason = ("push resolved exactly to the current main commit" if trusted_push
                  else "manual run resolved exactly to the current main commit")
        return {
            "sccache_gha_enabled": "on",
            "sccache_backend": "GitHub Actions Cache",
            "sccache_cache_scope": "gha-main",
            "sccache_scope_reason": reason,
            "cache_warming": "true" if trusted_push else "false",
        }
    return {
        "sccache_gha_enabled": "off",
        "sccache_backend": "Local disk",
        "sccache_cache_scope": "disk-ephemeral",
        "sccache_scope_reason": "event or source cannot safely use a persistent GHA cache scope",
        "cache_warming": "false",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", required=True)
    parser.add_argument("--github-ref", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--trusted-main-sha", required=True)
    parser.add_argument("--requested-ref", default="")
    args = parser.parse_args()
    values = select_scope(args.event, args.github_ref, args.source_sha, args.trusted_main_sha, args.requested_ref)
    output_path = os.environ.get("GITHUB_OUTPUT")
    if output_path:
        with Path(output_path).open("a", encoding="utf-8") as output:
            for name, value in values.items():
                output.write(f"{name}={value}\n")
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with Path(summary_path).open("a", encoding="utf-8") as summary:
            summary.write(f"sccache backend: {values['sccache_backend']}\n\n")
            summary.write(f"sccache cache scope: {values['sccache_cache_scope']}\n\n")
            summary.write(f"sccache scope reason: {values['sccache_scope_reason']}\n")
            if values["cache_warming"] == "true":
                summary.write("\nCache-warming run: configure and build all Debug targets; skip CTest execution and test-result uploads.\n")
            else:
                summary.write("\nNormal test run: configure, build all Debug targets, and execute CTest.\n")
    for name, value in values.items():
        print(f"{name}={value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
