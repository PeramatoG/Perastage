import datetime as dt
import importlib.util
from pathlib import Path
import unittest
from unittest import mock


SCRIPT = Path(__file__).parents[2] / ".github/scripts/awaiting_feedback_triage.py"
SPEC = importlib.util.spec_from_file_location("awaiting_feedback_triage", SCRIPT)
triage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(triage)

NOW = dt.datetime(2026, 7, 27, tzinfo=dt.timezone.utc)


def timestamp(days_ago):
    return (NOW - dt.timedelta(days=days_ago)).isoformat().replace("+00:00", "Z")


def issue(*, days_ago=31, labels=None, state="open", pull_request=False):
    value = {
        "number": 7,
        "state": state,
        "created_at": timestamp(days_ago),
        "updated_at": timestamp(days_ago),
        "labels": [{"name": name} for name in (labels or [triage.TARGET_LABEL])],
    }
    if pull_request:
        value["pull_request"] = {"url": "example"}
    return value


def comment(days_ago, body="human response", author_type="User"):
    return {
        "created_at": timestamp(days_ago),
        "body": body,
        "user": {"type": author_type},
    }


class DecisionTests(unittest.TestCase):
    def test_issue_without_target_label_is_ignored(self):
        self.assertEqual(triage.decide(issue(labels=["bug"]), [], NOW).action, "ignore")

    def test_pull_request_is_ignored(self):
        self.assertEqual(triage.decide(issue(pull_request=True), [], NOW).action, "ignore")

    def test_issue_below_30_days_is_ignored(self):
        self.assertEqual(triage.decide(issue(days_ago=29), [], NOW).action, "ignore")

    def test_first_warning_is_posted_after_30_days(self):
        self.assertEqual(triage.decide(issue(days_ago=30), [], NOW).action, "warn")

    def test_duplicate_warning_is_not_posted(self):
        comments = [comment(5, triage.WARNING_COMMENT, "Bot")]
        self.assertEqual(triage.decide(issue(), comments, NOW).action, "ignore")

    def test_closing_occurs_14_days_after_latest_warning(self):
        comments = [comment(14, triage.WARNING_COMMENT, "Bot")]
        self.assertEqual(triage.decide(issue(days_ago=60), comments, NOW).action, "close")

    def test_human_comment_after_warning_prevents_closure(self):
        comments = [comment(20, triage.WARNING_COMMENT, "Bot"), comment(10)]
        self.assertEqual(triage.decide(issue(days_ago=60), comments, NOW).action, "ignore")

    def test_issue_edit_after_warning_prevents_closure(self):
        value = issue(days_ago=60)
        value["updated_at"] = timestamp(10)
        comments = [comment(20, triage.WARNING_COMMENT, "Bot")]
        self.assertEqual(triage.decide(value, comments, NOW).action, "ignore")

    def test_renewed_inactivity_can_produce_new_warning(self):
        comments = [comment(50, triage.WARNING_COMMENT, "Bot"), comment(30)]
        self.assertEqual(triage.decide(issue(days_ago=90), comments, NOW).action, "warn")

    def test_removing_target_label_prevents_closure(self):
        comments = [comment(14, triage.WARNING_COMMENT, "Bot")]
        self.assertEqual(triage.decide(issue(labels=["bug"]), comments, NOW).action, "ignore")

    def test_closed_issue_is_ignored(self):
        self.assertEqual(triage.decide(issue(state="closed"), [], NOW).action, "ignore")

    def test_automation_comments_are_not_human_activity(self):
        comments = [comment(35, "unrelated automation", "Bot")]
        self.assertEqual(triage.decide(issue(days_ago=40), comments, NOW).action, "warn")


class ApiTests(unittest.TestCase):
    def test_pagination_collects_multiple_pages(self):
        api = triage.GitHubApi("owner/repository", "token")
        api.request = mock.Mock(side_effect=[
            ([{"number": 1}], '<https://api.github.com/repos/owner/repository/issues?page=2>; rel="next"'),
            ([{"number": 2}], None),
        ])
        self.assertEqual([item["number"] for item in api.paginated("/issues?page=1")], [1, 2])
        self.assertEqual(api.request.call_count, 2)

    def test_dry_run_performs_no_writes(self):
        api = mock.Mock()
        api.issues.return_value = [issue()]
        api.comments.return_value = []
        triage.run(api, True, NOW)
        api.current_issue.assert_not_called()
        api.comment.assert_not_called()
        api.remove_label.assert_not_called()
        api.close_not_planned.assert_not_called()

    def test_closure_rechecks_label_and_uses_not_planned(self):
        api = mock.Mock()
        api.issues.return_value = [issue(days_ago=60)]
        api.comments.return_value = [comment(14, triage.WARNING_COMMENT, "Bot")]
        api.current_issue.return_value = issue(days_ago=60)
        triage.run(api, False, NOW)
        api.comment.assert_called_once_with(7, triage.CLOSE_COMMENT)
        api.remove_label.assert_called_once_with(7)
        api.close_not_planned.assert_called_once_with(7)

    def test_label_removal_during_run_prevents_writes(self):
        api = mock.Mock()
        api.issues.return_value = [issue(days_ago=60)]
        api.comments.return_value = [comment(14, triage.WARNING_COMMENT, "Bot")]
        api.current_issue.return_value = issue(labels=["bug"])
        triage.run(api, False, NOW)
        api.comment.assert_not_called()
        api.remove_label.assert_not_called()
        api.close_not_planned.assert_not_called()

    def test_close_payload_uses_not_planned(self):
        api = triage.GitHubApi("owner/repository", "token")
        api.request = mock.Mock()
        api.close_not_planned(7)
        api.request.assert_called_once_with(
            "PATCH", "/issues/7", {"state": "closed", "state_reason": "not_planned"}
        )


if __name__ == "__main__":
    unittest.main()
