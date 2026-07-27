#!/usr/bin/env python3
"""Conservatively maintain issues explicitly awaiting reporter feedback."""

import argparse
import dataclasses
import datetime as dt
import hashlib
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

TARGET_LABEL = "status: awaiting-feedback"
WARNING_MARKER = "<!-- perastage-awaiting-feedback-warning:v1 -->"
CLOSE_MARKER = "<!-- perastage-awaiting-feedback-close:v1 -->"
WARNING_REVISION_PREFIX = "<!-- perastage-awaiting-feedback-warning-revision:v1:"
CLOSE_REVISION_PREFIX = "<!-- perastage-awaiting-feedback-revision:v1:"
WARNING_DAYS = 30
CLOSE_DAYS = 14

WARNING_COMMENT_TEXT = f"""{WARNING_MARKER}
Hi! This issue is currently waiting for the additional information requested above. If you are still able to reproduce the problem, any new details or test results would be very helpful. If there is no further information within the next 14 days, the issue may be closed for now, but it can still be reviewed again when the missing information becomes available. Thank you!"""

CLOSE_COMMENT_TEXT = f"""{CLOSE_MARKER}
Hi! Since we have not received the additional information needed to continue the investigation, I am closing this issue for now. This does not mean the report was unimportant. Please comment again or open a new issue with the requested details if the problem persists in a current Perastage release. Thank you for taking the time to report it."""


@dataclasses.dataclass(frozen=True)
class Decision:
    action: str
    reason: str


def parse_time(value):
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def is_automation_comment(comment):
    body = comment.get("body") or ""
    author_type = (comment.get("user") or {}).get("type")
    return WARNING_MARKER in body or CLOSE_MARKER in body or author_type == "Bot"


def issue_revision(issue):
    snapshot = {
        "title": issue.get("title"),
        "body": issue.get("body"),
        "labels": sorted(label.get("name") for label in issue.get("labels", [])),
        "assignees": sorted(user.get("login") for user in issue.get("assignees", [])),
        "milestone": (issue.get("milestone") or {}).get("number"),
    }
    encoded = json.dumps(snapshot, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def close_comment(issue):
    marker = f"{CLOSE_REVISION_PREFIX}{issue_revision(issue)} -->"
    return f"{CLOSE_COMMENT_TEXT}\n{marker}"


def warning_comment(issue):
    marker = f"{WARNING_REVISION_PREFIX}{issue_revision(issue)} -->"
    return f"{WARNING_COMMENT_TEXT}\n{marker}"


def marked_revision(comment, prefix):
    body = comment.get("body") or ""
    start = body.find(prefix)
    if start == -1:
        return None
    start += len(prefix)
    end = body.find(" -->", start)
    return body[start:end] if end != -1 else None


def decide(issue, comments, now):
    """Return the safe action for one issue from issue and comment snapshots."""
    labels = {label.get("name") for label in issue.get("labels", [])}
    if issue.get("state") != "open":
        return Decision("ignore", "issue is not open")
    if "pull_request" in issue:
        return Decision("ignore", "item is a pull request")
    if TARGET_LABEL not in labels:
        return Decision("ignore", "target label is absent")
    human_times = [parse_time(issue["created_at"])]
    human_times.extend(
        parse_time(comment["created_at"])
        for comment in comments
        if not is_automation_comment(comment)
    )
    warning_comments = [
        comment
        for comment in comments
        if WARNING_MARKER in (comment.get("body") or "")
    ]
    latest_human = max(human_times)
    latest_warning_comment = max(
        warning_comments, key=lambda comment: parse_time(comment["created_at"]), default=None
    )
    latest_warning = (
        parse_time(latest_warning_comment["created_at"])
        if latest_warning_comment
        else None
    )
    close_comments = [
        comment
        for comment in comments
        if CLOSE_MARKER in (comment.get("body") or "")
    ]
    latest_close_comment = max(
        close_comments, key=lambda comment: parse_time(comment["created_at"]), default=None
    )
    latest_close = (
        parse_time(latest_close_comment["created_at"])
        if latest_close_comment
        else None
    )

    if latest_close and latest_close > latest_human:
        if marked_revision(latest_close_comment, CLOSE_REVISION_PREFIX) != issue_revision(issue):
            return Decision("ignore", "issue changed after the closing comment")
        return Decision("finalize_close", "closing comment awaits final state update")

    if latest_warning and latest_warning > latest_human:
        warning_revision = marked_revision(latest_warning_comment, WARNING_REVISION_PREFIX)
        if warning_revision != issue_revision(issue):
            renewed_at = max(latest_human, parse_time(issue["updated_at"]))
            if now - renewed_at >= dt.timedelta(days=WARNING_DAYS):
                return Decision("warn", "30 days passed after the issue changed")
            return Decision("ignore", "issue changed after the warning")
        if now - latest_warning >= dt.timedelta(days=CLOSE_DAYS):
            return Decision("close", "14 days passed after the latest warning")
        return Decision("ignore", "warning grace period is active")
    if now - latest_human >= dt.timedelta(days=WARNING_DAYS):
        return Decision("warn", "30 days passed without human activity")
    return Decision("ignore", "inactivity period has not elapsed")


class GitHubApi:
    def __init__(self, repository, token):
        self.base = f"https://api.github.com/repos/{repository}"
        self.token = token

    def request(self, method, path, payload=None):
        data = json.dumps(payload).encode() if payload is not None else None
        request = urllib.request.Request(
            self.base + path,
            data=data,
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "User-Agent": "perastage-awaiting-feedback",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                body = response.read()
                return (json.loads(body) if body else None), response.headers.get("Link")
        except urllib.error.HTTPError as error:
            detail = error.read().decode(errors="replace")[:500]
            raise RuntimeError(f"GitHub API {method} {path} failed with HTTP {error.code}: {detail}") from error
        except urllib.error.URLError as error:
            raise RuntimeError(f"GitHub API {method} {path} failed: {error.reason}") from error

    def paginated(self, path):
        items = []
        next_path = path
        while next_path:
            page, link = self.request("GET", next_path)
            if not isinstance(page, list):
                raise RuntimeError(f"GitHub API returned a non-list page for {path}")
            items.extend(page)
            next_url = next_link(link)
            next_path = next_url.removeprefix(self.base) if next_url else None
        return items

    def issues(self):
        label = urllib.parse.quote(TARGET_LABEL)
        return self.paginated(f"/issues?state=open&labels={label}&per_page=100")

    def comments(self, number):
        return self.paginated(f"/issues/{number}/comments?per_page=100")

    def current_issue(self, number):
        return self.request("GET", f"/issues/{number}")[0]

    def comment(self, number, body):
        self.request("POST", f"/issues/{number}/comments", {"body": body})

    def close_not_planned(self, issue):
        labels = [
            label["name"]
            for label in issue.get("labels", [])
            if label.get("name") != TARGET_LABEL
        ]
        self.request(
            "PATCH",
            f"/issues/{issue['number']}",
            {"state": "closed", "state_reason": "not_planned", "labels": labels},
        )


def next_link(header):
    if not header:
        return None
    for part in header.split(","):
        segments = [segment.strip() for segment in part.split(";")]
        if len(segments) > 1 and 'rel="next"' in segments[1:]:
            return segments[0].strip("<>")
    return None


def still_eligible(issue):
    labels = {label.get("name") for label in issue.get("labels", [])}
    return issue.get("state") == "open" and "pull_request" not in issue and TARGET_LABEL in labels


def fresh_decision(api, number, now):
    issue = api.current_issue(number)
    comments = api.comments(number)
    return issue, comments, decide(issue, comments, now)


def finalize_close(api, number, now):
    issue, _comments, decision = fresh_decision(api, number, now)
    if decision.action != "finalize_close" or not still_eligible(issue):
        print(f"Issue #{number}: final close skipped after activity recheck")
        return
    api.close_not_planned(issue)


def run(api, dry_run, now):
    for issue in api.issues():
        number = issue["number"]
        decision = decide(issue, api.comments(number), now)
        print(f"Issue #{number}: {decision.action} ({decision.reason})")
        if decision.action == "ignore" or dry_run:
            continue
        current, _comments, fresh = fresh_decision(api, number, now)
        revision_changed = current.get("updated_at") != issue.get("updated_at")
        if revision_changed or fresh.action != decision.action or not still_eligible(current):
            print(f"Issue #{number}: skipped after complete decision recheck")
            continue
        if decision.action == "warn":
            api.comment(number, warning_comment(current))
        elif decision.action == "close":
            api.comment(number, close_comment(current))
            finalize_close(api, number, now)
        elif decision.action == "finalize_close":
            api.close_not_planned(current)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    repository = os.environ.get("GITHUB_REPOSITORY")
    token = os.environ.get("GITHUB_TOKEN")
    if not repository or not token:
        parser.error("GITHUB_REPOSITORY and GITHUB_TOKEN are required")
    try:
        run(GitHubApi(repository, token), args.dry_run, dt.datetime.now(dt.timezone.utc))
    except RuntimeError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
