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
const { site, publicUrl } = require('../tools/docs/site.cjs');
const { descriptionFrom } = require('../tools/docs/metadata.cjs');
const { indexable, crawlOutputs } = require('../tools/docs/crawl.cjs');
const root = path.resolve(__dirname, '..');
const first = generate();
const second = generate();
assert.deepEqual([...first.outputs], [...second.outputs], 'identical inputs must produce byte-identical HTML');
const { context } = first;
assert(![...first.outputs.keys()].some(file => file.startsWith(path.join(root, 'docs/stable/'))), 'normal generation must never own frozen Stable bytes');
const sitemap = first.outputs.get(path.join(context.docs, 'sitemap.xml'));
const locations = [...sitemap.matchAll(/<loc>([^<]+)<\/loc>/g)].map(match => match[1]);
const expectedLocations = [publicUrl(), ...first.stable.pages.map(name => publicUrl(`stable/${name}`)),
  ...context.pages.filter(page => page.developer).map(page => publicUrl(path.relative(context.docs, page.output).split(path.sep).join('/')))];
assert.deepEqual(locations, expectedLocations.sort());
assert.equal(new Set(locations).size, locations.length);
assert(!/\/next\/|manifest\.json|<lastmod>|\.md</.test(sitemap));
assert(first.outputs.get(path.join(context.docs, 'robots.txt')).includes(`Sitemap: ${publicUrl('sitemap.xml')}`));
assert(!first.outputs.get(path.join(context.docs, 'robots.txt')).includes('Disallow'));
assert(!indexable('<meta content="noindex,follow" name="robots">'));
assert(indexable('<meta name="robots" content="index,follow">'));
const excludedPage = context.pages.find(page => page.developer);
const excludedStable = first.stable.pages[0];
const noindexOutputs = new Map(first.outputs);
noindexOutputs.set(excludedPage.output, '<meta name="robots" content="noindex,follow">');
const noindexStable = { ...first.stable, outputs: new Map(first.stable.outputs) };
noindexStable.outputs.set(path.join(first.stable.directory, excludedStable), '<meta content="noindex,follow" name="robots">');
const filteredSitemap = crawlOutputs(context, noindexStable, noindexOutputs).get(path.join(context.docs, 'sitemap.xml'));
assert(!filteredSitemap.includes(publicUrl(`stable/${excludedStable}`)));
assert(!filteredSitemap.includes(publicUrl(path.relative(context.docs, excludedPage.output).split(path.sep).join('/'))));
assert.equal(descriptionFrom(''), site.description);
assert(!descriptionFrom('Use &quot;quotes&quot;, &amp; symbols and &#39;apostrophes&#39;.').includes('&amp;'));
assert(descriptionFrom('Long useful prose '.repeat(30)).length <= 170);
for (const page of context.pages) {
  const html = first.outputs.get(page.output);
  const canonical = publicUrl(path.relative(context.docs, page.output).split(path.sep).join('/'));
  assert(html.includes(`<link rel="canonical" href="${canonical}"`));
  assert(html.includes(`<meta property="og:url" content="${canonical}"`));
  for (const field of ['description', 'twitter:card', 'twitter:title', 'twitter:description', 'twitter:image']) assert(html.includes(`name="${field}"`));
  assert.equal((html.match(/<h1\b/g) || []).length, 1);
  assert(html.includes('alt="" width="48" height="48"'));
  assert(html.includes('logo-web.png') && !html.includes('alt="Perastage logo"'));
  if (!page.developer) {
    assert(html.includes('content="noindex,follow"'));
    assert(html.includes('aria-label="Documentation version"'));
    assert(html.includes('aria-current="page"'));
  }
}
assert(!first.outputs.get(context.hub).includes('<h3'));
const landing = fs.readFileSync(path.join(context.docs, 'index.html'), 'utf8');
assert(landing.includes(`<link rel="canonical" href="${publicUrl()}"`));
for (const field of ['description', 'twitter:card', 'twitter:title', 'twitter:description', 'twitter:image']) assert(landing.includes(`name="${field}"`));
for (const field of ['og:site_name', 'og:type', 'og:title', 'og:description', 'og:url', 'og:image']) assert(landing.includes(`property="${field}"`));
const jsonLd = JSON.parse(landing.match(/<script type="application\/ld\+json">([^]*?)<\/script>/)[1]);
assert.equal(jsonLd['@type'], 'SoftwareApplication');
assert.equal(jsonLd.url, publicUrl());
assert.equal(jsonLd.description, site.description);
assert.equal(jsonLd.license, 'https://spdx.org/licenses/GPL-3.0-or-later.html');
assert.equal(jsonLd.offers.price, '0');
assert(!landing.includes('Perastage_logo.png'));
assert(landing.includes('fetchpriority="high" loading="eager" decoding="async"'));
assert(landing.includes('href="stable/quick-start.html"') && landing.includes('href="stable/troubleshooting.html"'));
assert(landing.includes('experimental local CLI and OSC interfaces') && landing.includes('configured separately'));
const imageBytes = name => fs.statSync(path.join(context.docs, 'assets/images', name)).size;
assert(imageBytes('logo-web.png') < imageBytes('Perastage_logo.png') / 5);
assert(imageBytes('favicon.png') < 10000);
for (const [file, html] of first.outputs) {
  if (!file.endsWith('.html')) continue;
  if (html.includes('Compatibility-only')) assert(html.includes('name="robots" content="noindex,follow"'));
  assert(!/<script[^>]+src="https?:|<link[^>]+rel="stylesheet"[^>]+href="https?:/.test(html), 'no remote runtime dependencies');
}
assert(first.outputs.get(path.join(context.docs, 'documentation.html')).includes(`href="${publicUrl('stable/documentation.html')}"`));
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
assert.equal(rewriteUrl('../developer/build.md#building', user, userOutput, context), '../developer/build.html#building');
assert.equal(rewriteUrl('../user/features.md#overview', developer, developerOutput, context), '../next/features.html#overview');
assert.equal(rewriteUrl('../user/index.md#help', developer, developerOutput, context), '../next/documentation.html#help');
assert.equal(rewriteUrl('technical-notes/index.md#viewer-and-rendering-contracts', developer, developerOutput, context), 'technical-notes/index.html#viewer-and-rendering-contracts');
assert.equal(rewriteUrl('repository%5Flayout.md?mode=compact#paths', developer, developerOutput, context), 'repository_layout.html?mode=compact#paths');
assert.equal(rewriteUrl('../../CMakeLists.txt', developer, developerOutput, context), 'https://github.com/PeramatoG/Perastage/blob/main/CMakeLists.txt');
assert.equal(rewriteUrl('../../.github', developer, developerOutput, context), 'https://github.com/PeramatoG/Perastage/tree/main/.github');
assert.equal(rewriteUrl('../assets/images/Perastage_logo.png?v=1', user, userOutput, context), '../assets/images/Perastage_logo.png?v=1');
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
  const stale = path.join(fixture, 'docs/next/installation.html');
  const original = fs.readFileSync(stale, 'utf8');
  fs.writeFileSync(stale, original + '<!-- stale -->\n');
  const changed = snapshot();
  const drift = run(['--check'], 1);
  assert(drift.stderr.includes('docs/next/installation.html'), drift.stderr + drift.stdout);
  assert.deepEqual(snapshot(), changed, '--check must not rewrite stale artifacts');
  run([]);
  assert.equal(fs.readFileSync(stale, 'utf8'), original);
  for (const name of ['sitemap.xml', 'robots.txt']) {
    const file = path.join(fixture, 'docs', name), originalCrawl = fs.readFileSync(file, 'utf8');
    fs.appendFileSync(file, 'drift');
    assert(run(['--check'], 1).stderr.includes(`docs/${name}`));
    assert.equal(fs.readFileSync(file, 'utf8'), originalCrawl + 'drift', '--check must not rewrite crawl artifacts');
    run([]);
    assert.equal(fs.readFileSync(file, 'utf8'), originalCrawl);
  }
  const addedSource = path.join(fixture, 'docs/developer/sitemap-fixture.md');
  fs.writeFileSync(addedSource, '# Sitemap fixture\n\nCanonical developer prose for a newly discovered public route.\n');
  run(['--check'], 1); run([]);
  assert(fs.readFileSync(path.join(fixture, 'docs/sitemap.xml'), 'utf8').includes('/developer/sitemap-fixture.html</loc>'));
  fs.unlinkSync(addedSource);
  run(['--check'], 1); run([]);
  assert(!fs.readFileSync(path.join(fixture, 'docs/sitemap.xml'), 'utf8').includes('sitemap-fixture'));
  assert(!fs.existsSync(path.join(fixture, 'docs/developer/sitemap-fixture.html')));
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
  assert(rendered.includes('<img loading="lazy" decoding="async"'));
  assert(rendered.includes('alt="Logo"'));
  assert(rendered.includes('href="assets/images/Perastage_logo.png"'));
  assert(rendered.includes('href="https://example.org/x?y=1&z=2"'));
  fs.writeFileSync(sample, '# Summary fixture\n\n[Navigation](https://example.org/)\n\nUseful **prose**, `code`, &quot;quotes&quot; and &amp; symbols describe this canonical page, rather than its navigation.\n');
  const prose = renderMarkdown(sample, sampleOutput, sampleContext).descriptionHtml;
  const description = descriptionFrom(prose);
  assert(description.startsWith('Useful prose, code, "quotes" and & symbols'));
  assert(!description.includes('Navigation') && !description.includes('**') && !description.includes('`'));
  fs.writeFileSync(sample, '# Heading only\n\n[Navigation](https://example.org/)\n');
  assert.equal(descriptionFrom(renderMarkdown(sample, sampleOutput, sampleContext).descriptionHtml), site.description);
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
