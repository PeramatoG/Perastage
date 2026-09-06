#!/usr/bin/env python3
"""Write a concise GitHub Actions summary for vcpkg cache behavior."""
from __future__ import annotations

import argparse
import os
from pathlib import Path


def normalized(value: str | None) -> str:
    return value if value else "not reported"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--triplet", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--schema", default="v3")
    parser.add_argument("--primary-key", required=True)
    parser.add_argument("--downloads-hit", required=True)
    parser.add_argument("--compiled-hit", required=True)
    parser.add_argument("--compiled-save-outcome", required=True)
    parser.add_argument("--sdk-guard-result", default="not applicable")
    parser.add_argument("--sdk-guard-reason", default="not applicable")
    parser.add_argument("--sdk-invalidation-source", default="not applicable")
    parser.add_argument("--remote-mode", default="disabled")
    parser.add_argument("--remote-enabled", default="false")
    parser.add_argument("--remote-setup-result", default="not requested")
    parser.add_argument("--fallback-local-only", default="true")
    parser.add_argument("--publish-permitted", default="no")
    parser.add_argument("--install-outcome", default="not reported")
    parser.add_argument("--publication-verification", default="not applicable")
    args = parser.parse_args()

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        print("GITHUB_STEP_SUMMARY is not set; skipping vcpkg cache summary.")
        return 0

    exact_hit = args.compiled_hit.lower() == "true"
    save_outcome = normalized(args.compiled_save_outcome)
    save_attempted = "no, exact compiled cache was restored" if exact_hit else "yes, after vcpkg install succeeded"
    saved_status = "skipped because an exact cache already existed" if exact_hit else save_outcome
    effective_reuse = "no, restored cache was invalidated" if args.sdk_guard_result == "invalidated" else (
        "yes" if exact_hit else "no exact compiled cache restored"
    )

    lines = [
        "## vcpkg cache summary",
        "",
        f"- Platform: {args.platform}",
        f"- Runner architecture: {os.environ.get('RUNNER_ARCH', 'not reported')}",
        f"- vcpkg target triplet: {args.triplet}",
        f"- vcpkg baseline: {args.baseline}",
        f"- Cache schema version: {args.schema}",
        f"- Downloads cache hit: {normalized(args.downloads_hit)}",
        f"- Compiled vcpkg cache hit: {normalized(args.compiled_hit)}",
        f"- Effective compiled cache reuse: {effective_reuse}",
        f"- macOS SDK guard result: {args.sdk_guard_result}",
        f"- macOS SDK guard reason: {args.sdk_guard_reason}",
        f"- macOS SDK invalidation source: {args.sdk_invalidation_source}",
        f"- Effective compiled cache primary key: `{args.primary_key}`",
        f"- Explicit compiled cache save attempted: {save_attempted}",
        f"- Explicit compiled cache save result: {saved_status}",
        f"- Remote cache requested mode: {args.remote_mode}",
        f"- Remote cache enabled: {args.remote_enabled}",
        "- Remote source: PerastageGitHubPackages",
        f"- Remote authentication/setup result: {args.remote_setup_result}",
        f"- Fallback to local-only: {args.fallback_local_only}",
        f"- Permitted to publish remotely: {args.publish_permitted}",
        f"- vcpkg install outcome: {args.install_outcome}",
        f"- Remote publication verification: {args.publication_verification}",
        "",
    ]
    Path(summary_path).open("a", encoding="utf-8").write("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
