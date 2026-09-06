#!/usr/bin/env python3
"""Write a concise GitHub Actions summary for vcpkg cache behavior."""
from __future__ import annotations

import argparse
import os
import re
from pathlib import Path


def normalized(value: str | None) -> str:
    return value if value else "not reported"


def parse_install_activity(path: Path | None) -> tuple[str, str, str]:
    if path is None:
        return "unknown", "unknown", "unknown"
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return "unknown", "unknown", "unknown"
    restored_matches = re.findall(r"Restored\s+(\d+)\s+package\(s\)", text, re.IGNORECASE)
    built = len(set(re.findall(r"^Building\s+([^\s]+)", text, re.IGNORECASE | re.MULTILINE)))
    reused = "yes" if "All requested installations are currently installed" in text else "no" if built else "unknown"
    restored = restored_matches[-1] if restored_matches else "unknown"
    built_value = str(built) if built else "0" if reused == "yes" else "unknown"
    return reused, restored, built_value


def publication_status(install_outcome: str, save_outcome: str, exact_hit: bool, source_built: str,
                       primary_key: str) -> str:
    if install_outcome == "failure":
        return "no, dependency installation failed"
    if save_outcome == "success":
        return f"yes, saved under `{primary_key}`"
    if source_built not in {"0", "unknown"} and exact_hit:
        return "no, rebuilt packages cannot replace an immutable exact cache"
    if exact_hit:
        return "not needed, repaired primary cache was an exact hit"
    return "unknown"


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
    parser.add_argument("--install-log", type=Path)
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
    installed_reused, binary_restored, source_built = parse_install_activity(args.install_log)
    effective_reuse = "no" if args.sdk_guard_result == "invalidated" or source_built not in {"0", "unknown"} else installed_reused
    repaired_publication = publication_status(
        args.install_outcome, save_outcome, exact_hit, source_built, args.primary_key
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
        f"- Installed packages reused without rebuild: {installed_reused}",
        f"- Binary packages restored: {binary_restored}",
        f"- Source packages rebuilt: {source_built}",
        f"- macOS SDK guard result: {args.sdk_guard_result}",
        f"- macOS SDK guard reason: {args.sdk_guard_reason}",
        f"- macOS SDK invalidation source: {args.sdk_invalidation_source}",
        f"- Effective compiled cache primary key: `{args.primary_key}`",
        f"- Explicit compiled cache save attempted: {save_attempted}",
        f"- Explicit compiled cache save result: {saved_status}",
        f"- Repaired snapshot publication: {repaired_publication}",
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
