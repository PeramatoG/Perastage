const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { createHash } = require('node:crypto');
const { execFileSync } = require('node:child_process');
const { parseNavigation, discoverSources } = require('./sources.cjs');
const { renderMarkdown, relativeUrl } = require('./render_markdown.cjs');
const { pageTemplate, hubContent } = require('./templates.cjs');
const { validateOutputs } = require('./validate_outputs.cjs');
const { replaceDirectory } = require('./atomic_directory.cjs');

const VERSION = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;
const SHARED = new Set(['docs/assets/css/style.css', 'docs/assets/js/docs-nav.js', 'docs/assets/images/Perastage_logo.png']);
const hash = content => createHash('sha256').update(content).digest('hex');
const git = (root, args, encoding = 'utf8') => execFileSync('git', args, { cwd: root, encoding, maxBuffer: 64 * 1024 * 1024, stdio: ['ignore', 'pipe', 'pipe'] });

// Compare strict release versions without integer precision loss or prerelease coercion.
function compareVersions(a, b) {
  if (!VERSION.test(a) || !VERSION.test(b)) throw new Error('Stable version must use MAJOR.MINOR.PATCH without prerelease suffixes');
  const left = a.split('.').map(BigInt), right = b.split('.').map(BigInt);
  for (let i = 0; i < 3; i++) if (left[i] !== right[i]) return left[i] > right[i] ? 1 : -1;
  return 0;
}

// Derive the published page inventory from the snapshot's original user index.
function inventory(index) {
  return ['documentation.html', ...parseNavigation(index).flatMap(group => group.pages.map(page => page.file.replace(/\.md$/, '.html')))].sort();
}

// Read a complete regular-file inventory; symlinks must never enter a snapshot.
function filesUnder(directory, prefix = '') {
  return fs.readdirSync(directory).sort().flatMap(name => {
    const relative = prefix + name, file = path.join(directory, name), stat = fs.lstatSync(file);
    if (stat.isSymbolicLink()) throw new Error(`Stable contains a symlink: ${relative}`);
    return stat.isDirectory() ? filesUnder(file, relative + '/') : [relative];
  });
}

// Validate the frozen snapshot without needing history or regenerating its templates.
function readStable(root, optional = false) {
  const directory = path.join(root, 'docs/stable');
  if (!fs.existsSync(directory) && optional) return null;
  const manifest = JSON.parse(fs.readFileSync(path.join(directory, 'manifest.json'), 'utf8'));
  if (manifest.schema_version !== 1 || !VERSION.test(manifest.version) || typeof manifest.source_ref !== 'string' ||
      !manifest.source_ref || !/^[\da-f]{40}$/.test(manifest.source_commit) || typeof manifest.user_index !== 'string') {
    throw new Error('Invalid Stable manifest identity');
  }
  const pages = inventory(manifest.user_index);
  if (JSON.stringify(manifest.pages) !== JSON.stringify(pages)) throw new Error('Stable inventory differs from its release user index');
  const artifacts = manifest.artifacts;
  if (!artifacts || typeof artifacts !== 'object' || Array.isArray(artifacts)) throw new Error('Invalid Stable artifact inventory');
  const names = Object.keys(artifacts).sort();
  if (names.some(name => (!/^[a-z\d_-]+\.html$/i.test(name) && !name.startsWith('content/')) ||
      /[\\\x00-\x1f]/.test(name) || name.split('/').some(part => !part || part === '.' || part === '..'))) throw new Error('Invalid Stable artifact path');
  if (JSON.stringify(names.filter(name => !name.startsWith('content/'))) !== JSON.stringify(pages)) throw new Error('Stable page artifact inventory differs from its index');
  if (JSON.stringify(filesUnder(directory)) !== JSON.stringify([...names, 'manifest.json'].sort())) throw new Error('Stable has missing or extra files');
  const outputs = new Map();
  for (const name of names) {
    const file = path.join(directory, name), content = fs.readFileSync(file);
    if (!/^[\da-f]{64}$/.test(artifacts[name]) || hash(content) !== artifacts[name]) throw new Error(`Stable hash mismatch: ${name}`);
    outputs.set(file, content);
  }
  return { ...manifest, directory, outputs };
}

// Render only release-indexed user pages from the exact local Git object inventory.
function renderTagged(root, version, sourceRef, commit, nextPages, nextOutputs = new Map()) {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'perastage-tagged-docs-'));
  try {
    const tree = new Map(git(root, ['ls-tree', '-r', '-z', commit]).split('\0').filter(Boolean).map(line => {
      const [metadata, name] = line.split('\t');
      const [mode, type, oid] = metadata.split(' ');
      return [name, { mode, type, oid }];
    }));
    const bytes = name => {
      const entry = tree.get(name);
      if (!entry || entry.type !== 'blob' || !/^100(?:644|755)$/.test(entry.mode)) throw new Error(`Missing or non-regular tagged resource: ${name}`);
      return git(root, ['cat-file', 'blob', entry.oid], null);
    };
    const index = bytes('docs/user/index.md').toString('utf8');
    // Unindexed historical Markdown remains available through its tag, never invented routes.
    for (const name of ['index.md', ...parseNavigation(index).flatMap(group => group.pages.map(page => page.file))]) {
      const file = path.join(temporary, 'docs/user', name);
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, bytes(`docs/user/${name}`));
    }
    const docs = path.join(root, 'docs'), directory = path.join(docs, 'stable');
    const outputs = new Map();
    const repoUrl = (name, isDirectory = false) => `https://github.com/PeramatoG/Perastage/${isDirectory ? 'tree' : 'blob'}/${encodeURIComponent(sourceRef)}/${name.split('/').map(encodeURIComponent).join('/')}`;
    const context = {
      root: temporary, ...discoverSources(temporary, { userOnly: true, historical: true, userOutput: directory }),
      siteDocs: docs, channel: 'stable', version, otherPages: nextPages,
      developerHref: repoUrl('docs/developer', true),
      tagged: { url(name, output, suffix, resource) {
        const entry = tree.get(name), isDirectory = !entry && [...tree.keys()].some(file => file.startsWith(name + '/'));
        if (!entry && !isDirectory) throw new Error(`Missing tagged link/resource: ${name}`);
        if (SHARED.has(name)) return relativeUrl(output, path.join(root, name)) + suffix;
        if (resource || (entry && name.startsWith('docs/') && !/\.(?:md|html)$/.test(name))) {
          const destination = path.join(directory, 'content', name);
          outputs.set(destination, bytes(name));
          return relativeUrl(output, destination) + suffix;
        }
        return repoUrl(name, isDirectory) + suffix;
      } }
    };
    const rendered = new Map(context.pages.map(page => [page.source, renderMarkdown(page.source, page.output, context)]));
    for (const page of context.pages) outputs.set(page.output, pageTemplate({ ...page, summaryHtml: rendered.get(page.source).descriptionHtml }, rendered.get(page.source).html, context));
    outputs.set(context.hub, pageTemplate({ output: context.hub, title: 'User Documentation' }, hubContent(context, rendered), context, true));
    validateOutputs(new Map([...nextOutputs, ...outputs]), docs);
    const artifacts = Object.fromEntries([...outputs].map(([file, content]) => [path.relative(directory, file).split(path.sep).join('/'), hash(content)]).sort(([a], [b]) => a.localeCompare(b, 'en')));
    const manifest = { schema_version: 1, version, source_ref: sourceRef, source_commit: commit, user_index: index, pages: inventory(index), artifacts };
    outputs.set(path.join(directory, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
    return { directory, outputs };
  } finally { fs.rmSync(temporary, { recursive: true, force: true }); }
}

// Stage and validate the whole snapshot, then exchange directories in one atomic operation.
function promoteStable(root, version, sourceRef, nextPages, nextOutputs) {
  compareVersions(version, version);
  if (sourceRef !== `v${version}`) throw new Error('Stable source ref must be the exact release tag for its version');
  const commit = git(root, ['rev-parse', '--verify', '--end-of-options', `refs/tags/${sourceRef}^{commit}`]).trim();
  if (git(root, ['show', `${commit}:VERSION`]).trim() !== version) throw new Error('VERSION at source ref does not match requested Stable version');
  const stable = readStable(root, true);
  if (stable) {
    const comparison = compareVersions(version, stable.version);
    if (comparison < 0) throw new Error('Cannot regress Stable to an older release');
    if (comparison === 0) {
      if (stable.source_commit !== commit || stable.source_ref !== sourceRef) throw new Error('Same Stable version has a different source identity');
      return false;
    }
  }
  const result = renderTagged(root, version, sourceRef, commit, nextPages, nextOutputs);
  const staging = fs.mkdtempSync(path.join(root, 'docs/.stable-staging-'));
  const replacement = path.join(staging, 'replacement');
  try {
    for (const [file, content] of result.outputs) {
      const destination = path.join(replacement, path.relative(result.directory, file));
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.writeFileSync(destination, content);
    }
    // Validate staged bytes via the same contract as normal --check.
    const validationRoot = path.join(staging, 'validation');
    fs.mkdirSync(path.join(validationRoot, 'docs'), { recursive: true });
    fs.renameSync(replacement, path.join(validationRoot, 'docs/stable'));
    readStable(validationRoot);
    replaceDirectory(path.join(validationRoot, 'docs/stable'), result.directory);
    return true;
  } finally { fs.rmSync(staging, { recursive: true, force: true }); }
}

module.exports = { compareVersions, inventory, readStable, renderTagged, promoteStable };
