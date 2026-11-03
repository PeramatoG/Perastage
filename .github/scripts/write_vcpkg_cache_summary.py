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
    restored = str(sum(map(int, restored_matches))) if restored_matches else "unknown"
    completed = "All requested installations completed successfully" in text
    built_value = str(built) if built else "0" if reused == "yes" or completed else "unknown"
    return reused, restored, built_value


def remote_activity(path: Path | None) -> list[str]:
    """Report provider timing and publication evidence without copying raw NuGet output."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace") if path else ""
    except OSError:
        text = ""
    restores = re.findall(r"Restored\s+(\d+)\s+package\(s\) from NuGet in ([\d.]+\s*(?:ms|us|s|min))", text)
    publications = re.findall(r"Uploaded binaries to (\d+) NuGet feed", text)
    missing = len(re.findall(r"NotFound https://nuget\.pkg\.github\.com/", text))
    denied = bool(re.search(r"\b(?:401|403|Unauthorized|Forbidden)\b", text, re.I))
    return [
        f"- NuGet restore reports (packages, duration): {restores or 'not reported'}",
        f"- NuGet package URL misses: {missing}",
        f"- Remote authorization errors observed: {'yes' if denied else 'no (does not prove access)'}",
        f"- NuGet publication reports: {len(publications)} successful upload reports; "
        "independent read-back still required",
    ]


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
    parser.add_argument("--matched-key", default="")
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
    parser.add_argument("--install-elapsed-seconds", default="not reported")
    parser.add_argument("--publication-verification", default="not applicable")
    parser.add_argument("--save-key", default="")
    parser.add_argument("--environment", default=os.environ.get("VCPKG_CACHE_ENVIRONMENT", "not reported"))
    args = parser.parse_args()

    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        print("GITHUB_STEP_SUMMARY is not set; skipping vcpkg cache summary.")
        return 0

    exact_hit = args.compiled_hit.lower() == "true"
    cache_restore = "exact hit" if exact_hit else "partial/fallback hit" if args.matched_key else "miss"
    save_outcome = normalized(args.compiled_save_outcome)
    save_attempted = "yes" if save_outcome in {"success", "failure", "cancelled"} else "no"
    saved_status = save_outcome
    installed_reused, binary_restored, source_built = parse_install_activity(args.install_log)
    effective_reuse = "no" if args.sdk_guard_result == "invalidated" or source_built not in {"0", "unknown"} else installed_reused
    repaired_publication = publication_status(
        args.install_outcome, save_outcome, exact_hit, source_built, args.save_key or args.primary_key
    )
    if args.schema == "v5":
        saved_status = save_outcome + " (see cache action log for upload confirmation or an immutable-key race)"
        repaired_publication = "save action result: " + save_outcome

    lines = [
        "## vcpkg cache summary",
        "",
        f"- Platform: {args.platform}",
        f"- Runner architecture: {os.environ.get('RUNNER_ARCH', 'not reported')}",
        f"- Environment identity: `{args.environment}`",
        f"- vcpkg target triplet: {args.triplet}",
        f"- vcpkg baseline: {args.baseline}",
        f"- Cache schema version: {args.schema}",
        f"- Downloads cache hit: {normalized(args.downloads_hit)}",
        f"- Compiled vcpkg cache hit: {normalized(args.compiled_hit)}",
        f"- GitHub Actions binary cache result: {cache_restore}",
        f"- Matched binary cache key: `{normalized(args.matched_key)}`",
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
        f"- Completed snapshot key: `{normalized(args.save_key)}`",
        f"- Repaired snapshot publication: {repaired_publication}",
        f"- Remote cache requested mode: {args.remote_mode}",
        f"- Remote cache enabled: {args.remote_enabled}",
        "- Remote source: PerastageGitHubPackages",
        f"- Remote authentication/setup result: {args.remote_setup_result}",
        f"- Fallback to local-only: {args.fallback_local_only}",
        f"- Permitted to publish remotely: {args.publish_permitted}",
        f"- vcpkg install outcome: {args.install_outcome}",
        f"- vcpkg install elapsed time: {normalized(args.install_elapsed_seconds)} seconds",
        f"- Remote publication verification: {args.publication_verification}",
        *remote_activity(args.install_log),
        "",
    ]
    Path(summary_path).open("a", encoding="utf-8").write("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
