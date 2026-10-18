const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { compareVersions, readStable } = require('./versioning.cjs');

// Release-event policy is separate from rendering and never mutates a release or ref.
function publicationDecision(root, event, publishedSha) {
  const release = event.release;
  if (event.action !== 'published' || !release || release.draft || release.prerelease ||
      !/^v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/.test(release.tag_name || '')) {
    return { promote: false, reason: 'Not a published non-prerelease semantic release' };
  }
  if (!/^[\da-f]{40}$/.test(publishedSha || '')) throw new Error('Published release commit SHA is missing or invalid');
  const tag = release.tag_name, version = tag.slice(1);
  const git = args => execFileSync('git', args, { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
  const commit = git(['rev-parse', '--verify', `refs/tags/${tag}^{commit}`]);
  if (commit !== publishedSha) throw new Error('Local release tag does not resolve to the published release commit');
  if (git(['show', `${commit}:VERSION`]) !== version) throw new Error('Release tag VERSION does not match its name');
  const stable = readStable(root);
  const comparison = compareVersions(version, stable.version);
  if (comparison === 0 && (stable.source_commit !== commit || stable.source_ref !== tag)) throw new Error('Published version has changed its tagged source');
  return { promote: comparison > 0, version, source_ref: tag, source_commit: commit,
    reason: comparison > 0 ? 'Newer published release' : 'Stable already covers this version' };
}

// Recovery uses the current published API response and an exact local release tag.
function currentPublicationDecision(root, event, publishedSha, latest) {
  if (event.action === 'published') publicationDecision(root, event, publishedSha);
  if (!latest || latest.draft !== false || latest.prerelease !== false ||
      !latest.published_at || !/^v(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/.test(latest.tag_name || '')) {
    throw new Error('Invalid published release metadata');
  }
  const sha = execFileSync('git', ['rev-parse', '--verify', `refs/tags/${latest.tag_name}^{commit}`], { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
  return publicationDecision(root, { action: 'published', release: latest }, sha);
}

// Pass only validated values to Actions outputs; event-controlled text stays out of shell code.
function main() {
  const root = path.resolve(__dirname, '../..');
  const event = JSON.parse(fs.readFileSync(process.env.GITHUB_EVENT_PATH, 'utf8'));
  const decision = process.env.RELEASE_METADATA_PATH
    ? currentPublicationDecision(root, event, process.env.PUBLISHED_SHA, JSON.parse(fs.readFileSync(process.env.RELEASE_METADATA_PATH, 'utf8')))
    : publicationDecision(root, event, process.env.PUBLISHED_SHA);
  console.log(decision.reason);
  fs.appendFileSync(process.env.GITHUB_OUTPUT, `promote=${decision.promote}\n${decision.promote ? `version=${decision.version}\nsource_ref=${decision.source_ref}\n` : ''}`);
}

if (require.main === module) {
  try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = { publicationDecision, currentPublicationDecision };
