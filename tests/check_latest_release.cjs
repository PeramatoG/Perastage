const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { releaseManifest, writeManifest } = require('../tools/docs/latest_release.cjs');
const contract = require('../.github/release-artifact-contract.json');
const root = path.resolve(__dirname, '..');

// Explicit fixtures ensure five independent packages, including both different macOS DMGs.
function release(version = '1.8.0') {
  const names = [
    `Perastage_${version}_Setup.exe`, `Perastage-${version}-x86_64.AppImage`,
    `Perastage-${version}-macOS15-arm64.dmg`, `Perastage-${version}-macOS26-arm64.dmg`,
    `Perastage-${version}-arch-x86_64.pkg.tar.zst`, `Perastage-${version}-Debug-Symbols-Developers-Only.zip`
  ];
  return { tag_name: `v${version}`, draft: false, prerelease: false, published_at: '2026-10-18T12:00:00Z',
    html_url: `https://github.com/PeramatoG/Perastage/releases/tag/v${version}`,
    assets: names.map((name, index) => ({ name, size: 1000000 + index, state: 'uploaded',
      browser_download_url: `https://github.com/PeramatoG/Perastage/releases/download/v${version}/${name}` })) };
}
const latest = release();
const manifest = releaseManifest(latest, contract);
assert.deepEqual(Object.keys(manifest.assets), ['windows', 'linux', 'macos15', 'macos26', 'arch']);
for (const [platform, suffix] of Object.entries({windows: '_Setup.exe', linux: '-x86_64.AppImage',
  macos15: '-macOS15-arm64.dmg', macos26: '-macOS26-arm64.dmg', arch: '-arch-x86_64.pkg.tar.zst'})) {
  assert(manifest.assets[platform].url.endsWith(suffix));
  assert.equal(manifest.assets[platform].url, latest.assets.find(asset => asset.name === manifest.assets[platform].name).browser_download_url);
}
assert.notEqual(manifest.assets.macos15.url, manifest.assets.macos26.url);
assert(!JSON.stringify(manifest).includes('Debug-Symbols'));
assert.equal(JSON.stringify(releaseManifest(latest, contract)), JSON.stringify(manifest));
assert.deepEqual(releaseManifest({...latest, assets: [...latest.assets].reverse()}, contract), manifest);
for (const mutation of [
  value => { value.draft = true; }, value => { value.prerelease = true; },
  value => { value.tag_name = 'v1.8.0-rc.1'; }, value => { value.tag_name = 'v01.8.0'; },
  value => { value.published_at = null; }, value => { value.published_at = 'bad'; },
  value => { value.html_url = 'https://example.com/release'; }, value => { value.assets = null; },
  ...Object.keys(contract.packages).map((_, index) => value => { value.assets.splice(index, 1); }),
  value => { value.assets.push(value.assets[0]); }, value => { value.assets[0].size = 0; },
  value => { value.assets[0].state = 'new'; }, value => { delete value.assets[0].browser_download_url; },
  value => { value.assets[0].browser_download_url = value.assets[0].browser_download_url.replace('v1.8.0', 'v1.7.0'); },
  value => { value.assets[0].name = 'Perastage_1.7.0_Setup.exe'; }
]) {
  const invalid = structuredClone(latest); mutation(invalid);
  assert.throws(() => releaseManifest(invalid, contract));
}
for (const value of [null, {}, { draft: true }]) assert.throws(() => releaseManifest(value, contract));
// A contract edit changes selection without maintaining a second production pattern list.
const changedContract = structuredClone(contract); changedContract.packages.windows = 'Renamed-*.exe';
const renamed = release(); renamed.assets[0].name = 'Renamed-1.8.0.exe';
renamed.assets[0].browser_download_url = renamed.assets[0].browser_download_url.replace('Perastage_1.8.0_Setup.exe', renamed.assets[0].name);
assert.equal(releaseManifest(renamed, changedContract).assets.windows.name, 'Renamed-1.8.0.exe');
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'perastage-downloads-'));
try {
  const file = path.join(temp, 'latest-release.json');
  assert(writeManifest(file, releaseManifest(release('1.7.0'), contract)));
  assert(writeManifest(file, manifest));
  const bytes = fs.readFileSync(file, 'utf8');
  assert.equal(writeManifest(file, manifest), false);
  assert.equal(fs.readFileSync(file, 'utf8'), bytes);
  assert(!bytes.includes('1.7.0'), 'a successful publication removes every old version link');
  assert.throws(() => writeManifest(file, releaseManifest(release('1.7.0'), contract)), /regress/);
  const broken = release('1.9.0'); broken.assets.pop(); broken.assets.pop();
  assert.throws(() => writeManifest(file, releaseManifest(broken, contract)));
  assert.equal(fs.readFileSync(file, 'utf8'), bytes, 'incomplete publication preserves the public manifest');
  const repaired = structuredClone(manifest); repaired.assets.windows.size += 10;
  assert(writeManifest(file, repaired), 'same-version repaired assets refresh metadata');
  assert.equal(writeManifest(file, repaired), false);
  assert.deepEqual(fs.readdirSync(temp), ['latest-release.json']);
} finally { fs.rmSync(temp, { recursive: true, force: true }); }
// Check the checked-in bootstrap through the same production validation boundary.
const current = JSON.parse(fs.readFileSync(path.join(root, 'docs/assets/data/latest-release.json'), 'utf8'));
assert.deepEqual(releaseManifest({ tag_name: current.tag, draft: false, prerelease: false,
  published_at: current.published_at, html_url: current.release_notes_url,
  assets: Object.values(current.assets).map(asset => ({name: asset.name, browser_download_url: asset.url, size: asset.size, state: 'uploaded'})) }, contract), current);
const landing = fs.readFileSync(path.join(root, 'docs/index.html'), 'utf8');
for (const platform of Object.keys(contract.packages)) assert.equal(landing.split(`data-platform="${platform}"`).length - 1, 1);
assert(landing.includes('id="download"'));
assert(landing.includes('<noscript>'));
console.log('PASS: five contract-selected downloads, distinct macOS DMGs, actual URLs, drafts/prereleases/invalid or incomplete assets rejected, debug archive excluded, deterministic atomic updates, newer stable publication and repaired-release recovery.');
module.exports = { release };
