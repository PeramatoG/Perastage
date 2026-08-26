#!/usr/bin/env python3
from pathlib import Path
import csv
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / '.github/scripts/summarize_ctest_failures.py'

with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    junit = base / 'ctest.junit.xml'
    log = base / 'ctest.log'
    last = base / 'LastTestsFailed.log'
    out = base / 'summary.csv'
    junit.write_text('''<testsuite tests="3" failures="2" errors="1">
<testcase name="NormalFailed"><failure message="***Failed">Expected A actual B</failure></testcase>
<testcase name="Aborted"><error message="Subprocess aborted***Exception">Assertion failed in child</error></testcase>
<testcase name="TimedOut"><failure message="Timeout">Test timeout reached</failure><system-out>[Protocol] BEGIN Earlier
[Protocol] END Earlier
[Protocol] BEGIN IdleStop</system-out></testcase>
</testsuite>''')
    log.write_text('''    Start 4: SegfaultTest
4/5 Test #4: SegfaultTest ...***SEGFAULT  0.01 sec
Segmentation fault
    Start 6: BlockTimeout
[Protocol] BEGIN CompletedBlock
[Protocol] END CompletedBlock
[Protocol] BEGIN IdleStop
6/6 Test #6: BlockTimeout ...***Timeout  30.00 sec
    Start 5: CancelledTest
Test command: app
Cancellation requested by runner
''')
    last.write_text('''1:NormalFailed
2:Aborted
3:TimedOut
4:SegfaultTest
5:LaunchFailure
6:BlockTimeout
''')
    subprocess.run([sys.executable, str(TOOL), '--platform', 'linux', '--junit', str(junit), '--log', str(log), '--last-tests-failed', str(last), '--output', str(out)], check=True)
    rows = list(csv.DictReader(out.open()))
    names = {row['test'] for row in rows}
    assert {'NormalFailed', 'Aborted', 'TimedOut', 'SegfaultTest', 'LaunchFailure', 'CancelledTest', 'BlockTimeout'} <= names, rows
    by_name = {row['test']: row for row in rows}
    assert 'Expected A actual B' in by_name['NormalFailed']['first_failure_line']
    assert 'Assertion failed' in by_name['Aborted']['first_failure_line']
    assert 'Segmentation fault' in by_name['SegfaultTest']['first_failure_line']
    assert by_name['TimedOut']['first_failure_line'] == '[Protocol] BEGIN IdleStop'
    assert by_name['LaunchFailure']['first_failure_line'] == 'listed in LastTestsFailed.log'
    assert by_name['CancelledTest']['status'] == 'interrupted'
    assert by_name['BlockTimeout']['first_failure_line'] == '[Protocol] BEGIN IdleStop'
print('OK: CTest failure summary covers JUnit, log fallback, crashes, timeouts, and LastTestsFailed entries.')
