// Validate deterministic generation, drift detection and URL rewriting without dependencies.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync, spawnSync } = require('node:child_process');
const { generate } = require('../tools/docs/build_static_docs.cjs');
const { MARKER } = require('../tools/docs/templates.cjs');
const { rewriteUrl, renderMarkdown } = require('../tools/docs/render_markdown.cjs');
const marked = require('../tools/docs/vendor/marked.min.js');
const root = path.resolve(__dirname, '..');
const first = generate();
const second = generate();
assert.deepEqual([...first.outputs], [...second.outputs], 'identical inputs must produce byte-identical HTML');
const { context } = first;
assert.equal(context.groups.flatMap(group => group.pages).length, 13);
for (const [source, output] of context.routes) {
  assert(!source.includes(`${path.sep}locales${path.sep}`));
  assert(first.outputs.get(output).includes(MARKER));
  if (source.endsWith(`${path.sep}index.md`) && source.includes(`${path.sep}user${path.sep}`)) continue;
  const body = first.outputs.get(output);
  assert(body.includes('<h1 id='), source);
  assert(!/loadMarkdown\(|marked\.min\.js|docs-shell\.js/.test(body), source);
  assert(body.includes('aria-label="Breadcrumb"'), source);
}
const user = path.join(root, 'docs/user/installation.md');
const userOutput = context.routes.get(user);
const developer = path.join(root, 'docs/developer/index.md');
const developerOutput = context.routes.get(developer);
assert.equal(rewriteUrl('quick-start.md?mode=compact#next', user, userOutput, context), 'quick-start.html?mode=compact#next');
assert.equal(rewriteUrl('../developer/build.md#building', user, userOutput, context), 'developer/build.html#building');
assert.equal(rewriteUrl('../user/features.md#overview', developer, developerOutput, context), '../features.html#overview');
assert.equal(rewriteUrl('../user/index.md#help', developer, developerOutput, context), '../documentation.html#help');
assert.equal(rewriteUrl('technical-notes/index.md#viewer-and-rendering-contracts', developer, developerOutput, context), 'technical-notes/index.html#viewer-and-rendering-contracts');
assert.equal(rewriteUrl('repository%5Flayout.md?mode=compact#paths', developer, developerOutput, context), 'repository_layout.html?mode=compact#paths');
assert.equal(rewriteUrl('../../CMakeLists.txt', developer, developerOutput, context), 'https://github.com/PeramatoG/Perastage/blob/main/CMakeLists.txt');
assert.equal(rewriteUrl('../../.github', developer, developerOutput, context), 'https://github.com/PeramatoG/Perastage/tree/main/.github');
assert.equal(rewriteUrl('../assets/images/Perastage_logo.png?v=1', user, userOutput, context), 'assets/images/Perastage_logo.png?v=1');
for (const href of ['#local', 'https://example.org/file.md?x=1&y=2#fragment', '//example.org/file.md', 'mailto:docs@example.org']) {
  assert.equal(rewriteUrl(href, user, userOutput, context), href);
}
for (const href of ['/etc/passwd', 'C:\\private.md', '../../../../etc/passwd', 'missing.md']) {
  assert.throws(() => rewriteUrl(href, user, userOutput, context));
}

const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'perastage-static-docs-'));
try {
  fs.cpSync(path.join(root, 'docs'), path.join(fixture, 'docs'), { recursive: true });
  fs.cpSync(path.join(root, 'tools/docs'), path.join(fixture, 'tools/docs'), { recursive: true });
  // Copy only tracked repository destinations referenced by canonical Markdown.
  for (const page of context.pages) {
    marked.walkTokens(marked.lexer(fs.readFileSync(page.source, 'utf8')), token => {
      if (token.type !== 'link' || !token.href || /^(?:[a-z][\w+.-]*:|\/\/|#)/i.test(token.href)) return;
      const source = path.resolve(path.dirname(page.source), decodeURIComponent(token.href.split(/[?#]/)[0]));
      if (source.startsWith(context.docs + path.sep) || !fs.existsSync(source)) return;
      const destination = path.join(fixture, path.relative(root, source));
      fs.mkdirSync(path.dirname(destination), { recursive: true });
      fs.cpSync(source, destination, { recursive: true });
    });
  }
  execFileSync('git', ['init', '-q'], { cwd: fixture, stdio: ['ignore', 'pipe', 'pipe'] });
  execFileSync('git', ['add', '.'], { cwd: fixture, stdio: ['ignore', 'pipe', 'pipe'] });
  // Hash all HTML bytes around --check and repeated write mode, including the landing.
  const snapshot = () => {
    const files = [];
    const visit = directory => {
      for (const name of fs.readdirSync(directory).sort()) {
        const file = path.join(directory, name);
        if (fs.statSync(file).isDirectory()) visit(file);
        else if (name.endsWith('.html')) files.push([file, fs.readFileSync(file, 'utf8')]);
      }
    };
    visit(path.join(fixture, 'docs'));
    return files;
  };
  const run = (args, expected = 0) => {
    const result = spawnSync(process.execPath, ['tools/docs/build_static_docs.cjs', ...args], {
      cwd: fixture, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe']
    });
    assert.equal(result.status, expected, result.stderr + result.stdout);
    return result;
  };
  const before = snapshot();
  run(['--check']);
  run([]);
  run([]);
  assert.deepEqual(snapshot(), before, 'regeneration must not modify identical output');
  const stale = path.join(fixture, 'docs/installation.html');
  const original = fs.readFileSync(stale, 'utf8');
  fs.writeFileSync(stale, original + '<!-- stale -->\n');
  const changed = snapshot();
  const drift = run(['--check'], 1);
  assert(drift.stderr.includes('docs/installation.html'), drift.stderr + drift.stdout);
  assert.deepEqual(snapshot(), changed, '--check must not rewrite stale artifacts');
  run([]);
  assert.equal(fs.readFileSync(stale, 'utf8'), original);
  const canonical = path.join(fixture, 'docs/user/installation.md');
  fs.appendFileSync(canonical, '\nA new canonical paragraph.\n');
  run(['--check'], 1);
  run([]);
  run(['--check']);
  assert(fs.readFileSync(stale, 'utf8').includes('<p>A new canonical paragraph.</p>'));
  fs.unlinkSync(stale);
  run(['--check'], 1);
  run([]);
  const orphan = path.join(fixture, 'docs/developer/orphan.html');
  fs.writeFileSync(orphan, MARKER + '\n');
  run(['--check'], 1);
  assert(fs.existsSync(orphan));
  run([]);
  assert(!fs.existsSync(orphan));
  assert(fs.existsSync(path.join(fixture, 'docs/index.html')), 'hand-authored landing must survive cleanup');
  const unsafe = path.join(fixture, 'untracked-local.txt');
  fs.writeFileSync(unsafe, 'local');
  assert.throws(() => rewriteUrl('../../untracked-local.txt', path.join(fixture, 'docs/developer/index.md'), path.join(fixture, 'docs/developer/index.html'), {
    ...context, root: fixture, docs: path.join(fixture, 'docs'), routes: new Map()
  }), /Untracked/);
  const sample = path.join(fixture, 'docs/user/slug-fixture.md');
  fs.writeFileSync(sample, '# Heading with `code` & punctuation!\n\n## Repeated\n\n## Repeated\n\n![Logo](../assets/images/Perastage_logo.png)\n\n[External](https://example.org/x?y=1&z=2)\n\n<a href="../assets/images/Perastage_logo.png">Asset</a>\n');
  const sampleContext = { ...context, root: fixture, docs: path.join(fixture, 'docs') };
  const sampleOutput = path.join(fixture, 'docs/slug-fixture.html');
  const rendered = renderMarkdown(sample, sampleOutput, sampleContext).html;
  assert(rendered.includes('id="repeated-1"'));
  assert(rendered.includes('src="assets/images/Perastage_logo.png"'));
  assert(rendered.includes('href="assets/images/Perastage_logo.png"'));
  assert(rendered.includes('href="https://example.org/x?y=1&z=2"'));
  // The same machinery can use another source/output root without a locale inventory.
  fs.unlinkSync(sample);
  const translatedRoot = path.join(fixture, 'docs/custom-source');
  fs.cpSync(path.join(fixture, 'docs/user'), translatedRoot, { recursive: true });
  const future = generate(fixture, { userSource: translatedRoot, userOutput: path.join(fixture, 'docs/custom-output') });
  assert(future.outputs.has(path.join(fixture, 'docs/custom-output/installation.html')));
  assert(![...future.outputs.keys()].some(file => file.includes('/locales/')));
} finally {
  fs.rmSync(fixture, { recursive: true, force: true });
}
console.log('PASS: deterministic artifacts, non-mutating drift/missing/orphan checks, canonical route discovery, assets, URL rewriting, slug behavior and reusable source/output roots.');
