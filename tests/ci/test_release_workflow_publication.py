import subprocess
from pathlib import Path

WORKFLOWS = Path(__file__).resolve().parents[2] / ".github" / "workflows"


def run(cmd, cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def test_fresh_checkout_can_create_annotated_tag_after_workflow_identity(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    repo.mkdir()
    run(["git", "init"], repo)
    run(["git", "config", "user.name", "Initial Author"], repo)
    run(["git", "config", "user.email", "author@example.invalid"], repo)
    (repo / "VERSION").write_text("1.5.0\n")
    run(["git", "add", "VERSION"], repo)
    run(["git", "commit", "-m", "initial"], repo)
    run(["git", "config", "--unset", "user.name"], repo)
    run(["git", "config", "--unset", "user.email"], repo)
    run(["git", "config", "user.name", "github-actions[bot]"], repo)
    run(["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"], repo)
    run(["git", "tag", "-a", "v1.5.0", "HEAD", "-m", "Perastage v1.5.0"], repo)
    target = run(["git", "rev-parse", "refs/tags/v1.5.0^{commit}"], repo).stdout.strip()
    head = run(["git", "rev-parse", "HEAD"], repo).stdout.strip()
    assert target == head


def test_workflows_encode_safe_publication_and_recovery_guards() -> None:
    minor = (WORKFLOWS / "minor-draft-release.yml").read_text()
    recover = (WORKFLOWS / "recover-minor-release.yml").read_text()
    assert "git push --atomic origin" in minor
    assert "git push origin \"$RELEASE_SHA\":main" not in minor
    assert "Unsupported publication state" in minor
    assert "refs/tags/${NEW_TAG}^{commit}" in minor
    assert recover.count("refs/heads/main") == 0
    assert "git fetch origin \"$RELEASE_SHA\"" in recover
    assert "gh run download \"$SOURCE_RUN_ID\" --name Perastage-validated-release-assets" in recover
    assert recover.find("if [ \"$DRY_RUN\" = true ]; then") < recover.find("git tag -a \"$TAG\"")


def test_atomic_branch_and_tag_push_succeeds_against_local_bare_remote(tmp_path: Path) -> None:
    remote = tmp_path / "remote.git"
    run(["git", "init", "--bare", str(remote)], tmp_path)
    repo = tmp_path / "repo"
    run(["git", "clone", str(remote), str(repo)], tmp_path)
    run(["git", "config", "user.name", "Initial Author"], repo)
    run(["git", "config", "user.email", "author@example.invalid"], repo)
    (repo / "VERSION").write_text("1.4.157\n")
    run(["git", "add", "VERSION"], repo)
    run(["git", "commit", "-m", "base"], repo)
    run(["git", "push", "origin", "HEAD:refs/heads/main"], repo)
    base_sha = run(["git", "rev-parse", "HEAD"], repo).stdout.strip()
    (repo / "VERSION").write_text("1.5.0\n")
    run(["git", "commit", "-am", "release"], repo)
    release_sha = run(["git", "rev-parse", "HEAD"], repo).stdout.strip()
    run(["git", "config", "user.name", "github-actions[bot]"], repo)
    run(["git", "config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"], repo)
    assert run(["git", "rev-parse", "origin/main"], repo).stdout.strip() == base_sha
    run(["git", "tag", "-a", "v1.5.0", release_sha, "-m", "Perastage v1.5.0"], repo)
    run(["git", "push", "--atomic", "origin", f"{release_sha}:refs/heads/main", "refs/tags/v1.5.0:refs/tags/v1.5.0"], repo)
    assert run(["git", "ls-remote", str(remote), "refs/heads/main"], tmp_path).stdout.startswith(release_sha)
    assert run(["git", "ls-remote", str(remote), "refs/tags/v1.5.0^{}"], tmp_path).stdout.startswith(release_sha)
