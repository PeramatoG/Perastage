// Exercise release promotion and failure isolation in a real, local fixture repository.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync, spawnSync } = require('node:child_process');
const { compareVersions, readStable, renderTagged } = require('../tools/docs/versioning.cjs');
const { publicationDecision } = require('../tools/docs/published_release.cjs');
const { generate } = require('../tools/docs/build_static_docs.cjs');
const { replaceDirectory } = require('../tools/docs/atomic_directory.cjs');
const root = path.resolve(__dirname, '..');
const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'perastage-versioned-docs-'));

// All fixture refs and resources are created locally; no network or historical edits.
const write = (name, content) => {
  const file = path.join(fixture, name);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, content);
};
const git = args => execFileSync('git', args, { cwd: fixture, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
const run = (args = [], expected = 0) => {
  const result = spawnSync(process.execPath, ['tools/docs/build_static_docs.cjs', ...args], { cwd: fixture, encoding: 'utf8' });
  assert.equal(result.status, expected, result.stderr + result.stdout);
  return result;
};
const snapshot = () => [...readStable(fixture).outputs].map(([file, bytes]) => [path.relative(fixture, file), bytes.toString('base64')]);
const stableRead = name => fs.readFileSync(path.join(fixture, 'docs/stable', name), 'utf8');
const release = (tag, extra = {}) => ({ action: 'published', release: { tag_name: tag, draft: false, prerelease: false, ...extra } });
try {
  fs.cpSync(path.join(root, 'tools/docs'), path.join(fixture, 'tools/docs'), { recursive: true });
  for (const name of ['assets/css/style.css', 'assets/js/docs-nav.js', 'assets/images/Perastage_logo.png']) {
    const destination = path.join(fixture, 'docs', name);
    fs.mkdirSync(path.dirname(destination), { recursive: true });
    fs.copyFileSync(path.join(root, 'docs', name), destination);
  }
  write('docs/index.html', '<h1>Product landing</h1>');
  write('VERSION', '1.6.0\n');
  write('docs/user/index.md', '# User Documentation\n\n## Released guides\n\n- [Quick Start](quick-start.md)\n');
  write('docs/user/quick-start.md', '# Quick Start\n\nTagged original manual.\n\n## Start\n\n![Release image](../assets/manual.svg)\n\n[Build](../developer/build.md#building)\n\n[Version](../../VERSION)\n\n[Unindexed historical page](historical-extra.md)\n\n[External](https://example.org/reference.md?q=1#fragment)\n');
  write('docs/user/historical-extra.md', '# Historical extra\n');
  write('docs/assets/manual.svg', '<svg xmlns="http://www.w3.org/2000/svg"><title>Release image</title></svg>');
  write('docs/developer/index.md', '# Developer Documentation\n\n[Build](build.md#building)\n');
  write('docs/developer/build.md', '# Build\n\n## Building\n\nTagged build instructions.\n');
  write('docs/developer/repository_layout.md', '# Repository Layout\n');
  git(['init', '-q']);
  git(['config', 'user.name', 'Docs Fixture']); git(['config', 'user.email', 'fixture@example.invalid']);
  git(['add', '.']); git(['commit', '-qm', 'Release fixture']); git(['tag', 'v1.6.0']);
  const originalCommit = git(['rev-parse', 'v1.6.0^{commit}']);
  write('VERSION', '1.6.99\n');
  write('docs/user/index.md', '# User Documentation\n\n## Current guides\n\n- [Quick Start](quick-start.md)\n- [Inspector](inspector.md)\n');
  fs.unlinkSync(path.join(fixture, 'docs/user/historical-extra.md'));
  write('docs/user/quick-start.md', '# Quick Start\n\nWorking-tree manual.\n\n## Start\n\n![Current image](../assets/manual.svg)\n\n[Build](../developer/build.md#building)\n');
  write('docs/user/inspector.md', '# Inspector\n\n## Inspect\n\nCurrent-only workflow.\n');
  write('docs/assets/manual.svg', '<svg xmlns="http://www.w3.org/2000/svg"><title>Current image</title></svg>');
  write('docs/developer/build.md', '# Build\n\n## Building\n\nCurrent build instructions.\n');
  git(['add', '.']); git(['commit', '-qm', 'Current source']);
  run(['--promote-stable', '1.6.0', '--source-ref', 'v1.6.0']); run(['--check']);
  const stable = readStable(fixture);
  assert.equal(stable.source_commit, originalCommit);
  assert.deepEqual(stable.pages, ['documentation.html', 'quick-start.html']);
  const tagged = stableRead('quick-start.html');
  assert(tagged.includes('Tagged original manual.'));
  assert(!tagged.includes('Working-tree manual.'));
  assert(tagged.includes('Released guides'));
  assert(tagged.includes('blob/v1.6.0/docs/developer/build.md#building'));
  assert(tagged.includes('blob/v1.6.0/VERSION'));
  assert(tagged.includes('blob/v1.6.0/docs/user/historical-extra.md'));
  assert(tagged.includes('https://example.org/reference.md?q=1#fragment'));
  assert(tagged.includes('content/docs/assets/manual.svg'));
  assert(stableRead('content/docs/assets/manual.svg').includes('Release image'));
  assert(![...stable.outputs.keys()].some(file => /content.*(?:style\.css|logo|docs-nav)/.test(file)));
  const nextFile = path.join(fixture, 'docs/next/quick-start.html');
  assert(fs.readFileSync(nextFile, 'utf8').includes('Working-tree manual.'));
  assert(fs.readFileSync(nextFile, 'utf8').includes('href="../developer/build.html#building"'));
  assert(fs.readFileSync(nextFile, 'utf8').includes('Current guides'));
  assert(fs.readFileSync(path.join(fixture, 'docs/next/inspector.html'), 'utf8').includes('unavailable in Stable 1.6.0'));
  assert(fs.readFileSync(path.join(fixture, 'docs/quick-start.html'), 'utf8').includes('location.replace("stable/quick-start.html"'));
  assert(fs.readFileSync(path.join(fixture, 'docs/inspector.html'), 'utf8').includes('location.replace("next/inspector.html"'));
  const before = snapshot();
  assert.throws(() => replaceDirectory(path.join(fixture, 'missing-staging-directory'), path.join(fixture, 'docs/stable')));
  assert.deepEqual(snapshot(), before, 'failed atomic swap leaves the old snapshot present and intact');
  run(); run(); run(['--promote-stable', '1.6.0', '--source-ref', 'v1.6.0']);
  assert.deepEqual(snapshot(), before, 'same-version promotion and normal writes preserve every Stable artifact');
  // Rendering from the same exact objects produces identical bytes independently of HEAD.
  const preview = generate(fixture);
  const rendered = renderTagged(fixture, '1.6.0', 'v1.6.0', originalCommit, new Set(['documentation.html', 'quick-start.html', 'inspector.html']), preview.outputs);
  for (const [file, bytes] of rendered.outputs) assert.deepEqual(Buffer.from(bytes), fs.readFileSync(file));
  const manifestBefore = fs.readFileSync(path.join(fixture, 'docs/stable/manifest.json'));
  fs.appendFileSync(path.join(fixture, 'docs/user/quick-start.md'), '\nA new Next paragraph.\n');
  run(['--check'], 1); run(); run(['--check']);
  assert(fs.readFileSync(nextFile, 'utf8').includes('A new Next paragraph.'));
  assert.deepEqual(snapshot(), before);
  assert.deepEqual(fs.readFileSync(path.join(fixture, 'docs/stable/manifest.json')), manifestBefore);
  for (const mutation of ['edit', 'delete', 'extra']) {
    const file = path.join(fixture, 'docs/stable', mutation === 'extra' ? 'unexpected.txt' : 'quick-start.html');
    const original = fs.existsSync(file) ? fs.readFileSync(file) : null;
    if (mutation === 'delete') fs.unlinkSync(file); else fs.writeFileSync(file, 'tamper');
    run(['--check'], 1); run([], 1);
    if (original) fs.writeFileSync(file, original); else fs.unlinkSync(file);
  }
  // Release publication gating is tested with exact tags, not only workflow text.
  assert.equal(publicationDecision(fixture, release('v1.6.0'), originalCommit).promote, false);
  for (const event of [release('v1.7.0', { prerelease: true }), release('v1.7.0-rc.1'), release('v1.7.0', { draft: true }), { ...release('v1.7.0'), action: 'created' }]) {
    assert.equal(publicationDecision(fixture, event, '').promote, false);
  }
  assert.throws(() => publicationDecision(fixture, release('v1.6.0'), '0'.repeat(40)), /published release commit/);
  write('VERSION', '1.5.0\n'); git(['add', '.']); git(['commit', '-qm', 'Older fixture']); git(['tag', 'v1.5.0']);
  assert.equal(publicationDecision(fixture, release('v1.5.0'), git(['rev-parse', 'v1.5.0'])).promote, false);
  run(['--promote-stable', '1.5.0', '--source-ref', 'v1.5.0'], 1);
  run(['--promote-stable', '1.7.0-rc.1', '--source-ref', 'v1.6.0'], 1);
  run(['--promote-stable', '1.7.0', '--source-ref', 'v1.6.0'], 1);
  run(['--promote-stable', '1.7.0', '--source-ref', '--upload-pack=bad'], 1);
  assert.deepEqual(snapshot(), before);
  write('VERSION', '1.7.0\n'); git(['add', '.']); git(['commit', '-qm', 'New release fixture']); git(['tag', 'v1.7.0']);
  const publishedCommit = git(['rev-parse', 'v1.7.0']);
  assert.equal(publicationDecision(fixture, release('v1.7.0'), publishedCommit).promote, true);
  fs.appendFileSync(path.join(fixture, 'docs/user/quick-start.md'), '\nAfter tagged release.\n');
  run(['--promote-stable', '1.7.0', '--source-ref', 'v1.7.0']); run(['--check']);
  assert.equal(readStable(fixture).version, '1.7.0');
  assert(stableRead('inspector.html').includes('Current-only workflow.'));
  assert(!stableRead('quick-start.html').includes('After tagged release.'));
  assert(fs.readFileSync(path.join(fixture, 'docs/inspector.html'), 'utf8').includes('location.replace("stable/inspector.html"'));
  assert.equal(publicationDecision(fixture, release('v1.7.0'), publishedCommit).promote, false);
  const promoted = snapshot();
  write('VERSION', '1.8.0\n');
  const validNext = fs.readFileSync(path.join(fixture, 'docs/user/quick-start.md'));
  fs.appendFileSync(path.join(fixture, 'docs/user/quick-start.md'), '\n![Missing resource](../assets/missing.png)\n');
  git(['add', '.']); git(['commit', '-qm', 'Invalid release fixture']); git(['tag', 'v1.8.0']);
  fs.writeFileSync(path.join(fixture, 'docs/user/quick-start.md'), validNext);
  run(['--promote-stable', '1.8.0', '--source-ref', 'v1.8.0'], 1);
  assert.deepEqual(snapshot(), promoted, 'failed rendering cannot damage the published snapshot');
  assert(!fs.readdirSync(path.join(fixture, 'docs')).some(name => name.startsWith('.stable-staging-')));
  assert.equal(compareVersions('10.0.0', '2.99.0'), 1);
  assert.equal(compareVersions('1.6.10', '1.6.9'), 1);
  assert.equal(compareVersions('1.6.0', '1.6.0'), 0);
  assert.throws(() => compareVersions('01.6.0', '1.6.0'));
} finally { fs.rmSync(fixture, { recursive: true, force: true }); }
console.log('PASS: exact tagged source, per-channel inventories/navigation/assets, deterministic promotion, Stable immutability/tamper detection, compatibility routing, newer/same/older/prerelease release policy and failed-promotion isolation.');
