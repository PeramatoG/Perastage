const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { compareVersions } = require('./versioning.cjs');
const REPOSITORY = 'PeramatoG/Perastage';
const RELEASES = `https://github.com/${REPOSITORY}/releases`;

// Select only the installer contract, requiring exactly one complete asset per platform.
function releaseManifest(release, contract) {
  if (!release || release.draft !== false || release.prerelease !== false ||
      !/^v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/.test(release.tag_name || '') ||
      !release.published_at || !Number.isFinite(Date.parse(release.published_at)) ||
      release.html_url !== `${RELEASES}/tag/${release.tag_name}` || !Array.isArray(release.assets)) {
    throw new Error('Expected a published stable GitHub release');
  }
  const version = release.tag_name.slice(1), assets = {};
  for (const [platform, pattern] of Object.entries(contract.packages)) {
    const expression = new RegExp('^' + pattern.split('*').map(part => part.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('.*') + '$');
    const matches = release.assets.filter(asset => typeof asset.name === 'string' && expression.test(asset.name));
    if (matches.length !== 1) throw new Error(`Expected exactly one ${platform} package, found ${matches.length}`);
    const asset = matches[0];
    const token = version.replace(/\./g, '[._-]');
    if (!new RegExp(`(^|[^0-9])${token}([^0-9]|$)`).test(asset.name) ||
        asset.state !== 'uploaded' || !Number.isSafeInteger(asset.size) || asset.size <= 0 ||
        asset.browser_download_url !== `${RELEASES}/download/${release.tag_name}/${encodeURIComponent(asset.name)}`) {
      throw new Error(`Invalid or stale ${platform} asset`);
    }
    assets[platform] = { name: asset.name, url: asset.browser_download_url, size: asset.size };
  }
  return { schema_version: 1, version, tag: release.tag_name, published_at: release.published_at,
    release_notes_url: release.html_url, assets };
}

// Never replace a public stable manifest with an older release, even on delayed events.
function writeManifest(file, manifest) {
  if (fs.existsSync(file)) {
    const previous = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (compareVersions(manifest.version, previous.version) < 0) throw new Error('Cannot regress public downloads');
  }
  const content = JSON.stringify(manifest, null, 2) + '\n';
  if (fs.existsSync(file) && fs.readFileSync(file, 'utf8') === content) return false;
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file + '.tmp', content);
  fs.renameSync(file + '.tmp', file);
  return true;
}

// Fetch at publication time, never from visitors; --input supports audited offline bootstraps.
function main(args = process.argv.slice(2)) {
  const root = path.resolve(__dirname, '../..');
  let release;
  if (args.length === 2 && args[0] === '--input') release = JSON.parse(fs.readFileSync(args[1], 'utf8'));
  else if (args.length === 2 && args[0] === '--fetch') {
    const repository = process.env.GITHUB_REPOSITORY;
    if (repository !== REPOSITORY) throw new Error('Unexpected release repository');
    release = JSON.parse(execFileSync('gh', ['api', `repos/${repository}/releases/latest`], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }));
    // Save the exact API response for the documentation publication decision.
    fs.writeFileSync(args[1], JSON.stringify(release));
  } else throw new Error('Usage: latest_release.cjs --input RELEASE.json | --fetch RELEASE.json');
  const contract = JSON.parse(fs.readFileSync(path.join(root, '.github/release-artifact-contract.json'), 'utf8'));
  writeManifest(path.join(root, 'docs/assets/data/latest-release.json'), releaseManifest(release, contract));
}
if (require.main === module) {
  try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = { releaseManifest, writeManifest };
