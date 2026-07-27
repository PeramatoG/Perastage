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


def closing_comment(days_ago, value=None):
    return comment(days_ago, triage.close_comment(value or issue(days_ago=60)), "Bot")


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

    def test_warning_and_issue_timestamps_may_differ(self):
        value = issue(days_ago=60)
        value["updated_at"] = (NOW - dt.timedelta(days=14, seconds=-7)).isoformat()
        comments = [comment(14, triage.WARNING_COMMENT, "Bot")]
        self.assertEqual(triage.decide(value, comments, NOW).action, "close")

    def test_open_issue_with_close_marker_resumes_finalization(self):
        value = issue(days_ago=60)
        comments = [closing_comment(1, value)]
        self.assertEqual(triage.decide(value, comments, NOW).action, "finalize_close")

    def test_human_activity_after_close_marker_prevents_finalization(self):
        value = issue(days_ago=60)
        comments = [closing_comment(2, value), comment(1)]
        self.assertEqual(triage.decide(value, comments, NOW).action, "ignore")


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
        api.close_not_planned.assert_not_called()

    def test_human_comment_after_initial_decision_prevents_warning(self):
        api = mock.Mock()
        initial = issue()
        current = issue()
        current["updated_at"] = timestamp(0)
        api.issues.return_value = [initial]
        api.comments.side_effect = [[], [comment(0)]]
        api.current_issue.return_value = current
        triage.run(api, False, NOW)
        api.comment.assert_not_called()

    def test_issue_edit_after_initial_decision_prevents_warning(self):
        api = mock.Mock()
        initial = issue()
        current = issue()
        current["updated_at"] = timestamp(0)
        api.issues.return_value = [initial]
        api.comments.side_effect = [[], []]
        api.current_issue.return_value = current
        triage.run(api, False, NOW)
        api.comment.assert_not_called()

    def test_label_removal_during_run_prevents_writes(self):
        api = mock.Mock()
        api.issues.return_value = [issue(days_ago=60)]
        api.comments.side_effect = [
            [comment(14, triage.WARNING_COMMENT, "Bot")],
            [comment(14, triage.WARNING_COMMENT, "Bot")],
        ]
        api.current_issue.return_value = issue(labels=["bug"])
        triage.run(api, False, NOW)
        api.comment.assert_not_called()
        api.close_not_planned.assert_not_called()

    def test_close_payload_atomically_removes_target_and_uses_not_planned(self):
        api = triage.GitHubApi("owner/repository", "token")
        api.request = mock.Mock()
        value = issue(labels=[triage.TARGET_LABEL, "bug", "platform:linux"])
        api.close_not_planned(value)
        api.request.assert_called_once_with(
            "PATCH",
            "/issues/7",
            {
                "state": "closed",
                "state_reason": "not_planned",
                "labels": ["bug", "platform:linux"],
            },
        )

    def test_successful_close_rechecks_after_marker_and_posts_once(self):
        api = mock.Mock()
        value = issue(days_ago=60)
        warning = [comment(14, triage.WARNING_COMMENT, "Bot")]
        closing = warning + [closing_comment(0, value)]
        api.issues.return_value = [value]
        api.current_issue.return_value = value
        api.comments.side_effect = [warning, warning, closing]
        triage.run(api, False, NOW)
        api.comment.assert_called_once_with(7, triage.close_comment(value))
        api.close_not_planned.assert_called_once_with(value)

    def test_partial_close_recovers_without_duplicate_comment(self):
        value = issue(days_ago=60)
        closing = [closing_comment(1, value)]
        api = mock.Mock()
        api.issues.return_value = [value]
        api.current_issue.return_value = value
        api.comments.side_effect = [closing, closing]
        triage.run(api, False, NOW)
        api.comment.assert_not_called()
        api.close_not_planned.assert_called_once_with(value)

    def test_human_activity_after_partial_close_prevents_recovery(self):
        value = issue(days_ago=60)
        comments = [closing_comment(2, value), comment(1)]
        api = mock.Mock()
        api.issues.return_value = [value]
        api.comments.return_value = comments
        triage.run(api, False, NOW)
        api.comment.assert_not_called()
        api.close_not_planned.assert_not_called()

    def test_issue_edit_after_partial_close_prevents_recovery(self):
        original = issue(days_ago=60)
        edited = dict(original, title="Edited after pending close")
        comments = [closing_comment(2, original)]
        api = mock.Mock()
        api.issues.return_value = [edited]
        api.comments.return_value = comments
        triage.run(api, False, NOW)
        api.comment.assert_not_called()
        api.close_not_planned.assert_not_called()

    def test_failure_after_posting_close_comment_recovers_without_duplicate(self):
        value = issue(days_ago=60)
        warning = [comment(14, triage.WARNING_COMMENT, "Bot")]
        closing = warning + [closing_comment(0, value)]
        first_api = mock.Mock()
        first_api.issues.return_value = [value]
        first_api.current_issue.return_value = value
        first_api.comments.side_effect = [warning, warning, closing]
        first_api.close_not_planned.side_effect = RuntimeError("final update failed")
        with self.assertRaises(RuntimeError):
            triage.run(first_api, False, NOW)
        first_api.comment.assert_called_once_with(7, triage.close_comment(value))

        recovery_api = mock.Mock()
        recovery_api.issues.return_value = [value]
        recovery_api.current_issue.return_value = value
        recovery_api.comments.side_effect = [closing, closing]
        triage.run(recovery_api, False, NOW)
        recovery_api.comment.assert_not_called()
        recovery_api.close_not_planned.assert_called_once_with(value)

    def test_failed_atomic_final_update_recovers_on_next_run(self):
        value = issue(days_ago=60)
        closing = [closing_comment(1, value)]
        for failure_stage in ("label update", "issue closure"):
            with self.subTest(failure_stage=failure_stage):
                failing_api = mock.Mock()
                failing_api.issues.return_value = [value]
                failing_api.current_issue.return_value = value
                failing_api.comments.side_effect = [closing, closing]
                failing_api.close_not_planned.side_effect = RuntimeError(failure_stage)
                with self.assertRaises(RuntimeError):
                    triage.run(failing_api, False, NOW)

                recovery_api = mock.Mock()
                recovery_api.issues.return_value = [value]
                recovery_api.current_issue.return_value = value
                recovery_api.comments.side_effect = [closing, closing]
                triage.run(recovery_api, False, NOW)
                recovery_api.comment.assert_not_called()
                recovery_api.close_not_planned.assert_called_once_with(value)


if __name__ == "__main__":
    unittest.main()
