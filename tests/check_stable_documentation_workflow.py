#!/usr/bin/env python3
"""Protect the published-release documentation boundary and safe main publication."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
workflow = (root / '.github/workflows/promote-stable-documentation.yml').read_text()
minor = (root / '.github/workflows/minor-draft-release.yml').read_text()
recovery = (root / '.github/workflows/recover-minor-release.yml').read_text()

for marker in ('release:', 'types: [published]', '!github.event.release.prerelease',
               'group: perastage-stable-documentation', 'cancel-in-progress: false',
               'ref: main', 'fetch-depth: 0', 'PUBLISHED_SHA: ${{ github.sha }}',
               'node tools/docs/published_release.cjs',
               'node tools/docs/build_static_docs.cjs --check',
               '--promote-stable "$RELEASE_VERSION" --source-ref "$SOURCE_REF"',
               'node tests/check_static_docs.cjs', 'node tests/check_versioned_docs.cjs',
               "':!docs/sitemap.xml'", 'docs/sitemap.xml docs/robots.txt',
               'python3 tests/check_docs_links.py', 'python3 tests/check_docs_links_fixtures.py',
               '[skip version-bump]', 'git push origin HEAD:refs/heads/main'):
    assert marker in workflow, f'published-release workflow is missing {marker}'
assert 'workflow_dispatch:' in workflow and 'push:' not in workflow
assert 'node tools/docs/latest_release.cjs --fetch' in workflow
assert 'RELEASE_METADATA_PATH: ${{ runner.temp }}/latest-release.json' in workflow
assert workflow.index('latest_release.cjs --fetch') < workflow.index('published_release.cjs') < workflow.index('--promote-stable')
assert "':!docs/assets/data/latest-release.json'" in workflow
assert 'docs/robots.txt docs/assets/data/latest-release.json' in workflow
assert 'GH_TOKEN: ${{ steps.release-app-token.outputs.token || github.token }}' in workflow
assert 'node tests/check_latest_release.cjs' in workflow
assert not re.search(r'git\s+push[^\n]*(?:--force|\s-f\b|\+HEAD)', workflow)
assert 'gh release' not in workflow and 'git tag' not in workflow
assert 'upload-pages-artifact' not in workflow and 'deploy-pages' not in workflow
assert 'continue-on-error' not in workflow
assert workflow.index('--promote-stable') < workflow.index('node tests/check_versioned_docs.cjs') < workflow.index('git commit -m') < workflow.index('git push origin')
assert "git add -- docs/stable ':(glob)docs/**/*.html' ':(glob)docs/*.html'" in workflow
assert 'Unexpected source changes' in workflow
for guarded_step in ('Promote and validate static documentation',):
    block = workflow[workflow.index(guarded_step):].split('      - name:', 1)[0]
    assert "if: ${{ steps.release.outputs.promote == 'true' }}" in block
assert 'node tools/docs/build_static_docs.cjs --check' in minor
assert minor.index('node tools/docs/build_static_docs.cjs --check') < minor.index('  stage-release-commit:')
assert '--promote-stable' not in minor and '--promote-stable' not in recovery
print('OK: published stable downloads with manual recovery and no push trigger; only published non-prerelease releases promote Stable; exact tag identity, validation, generated-only commits, fail-closed App credentials and non-force publication are guarded.')
