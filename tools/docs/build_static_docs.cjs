#!/usr/bin/env node
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { discoverSources } = require('./sources.cjs');
const { renderMarkdown } = require('./render_markdown.cjs');
const { MARKER, pageTemplate, hubContent, compatibilityTemplate } = require('./templates.cjs');
const { readStable, promoteStable } = require('./versioning.cjs');
const { validateOutputs } = require('./validate_outputs.cjs');

// Build every artifact in memory before writing, also exposing the pipeline to fixtures.
function generate(root = path.resolve(__dirname, '../..'), options = {}) {
  const stable = readStable(root, options.allowMissingStable);
  const context = {
    root, ...discoverSources(root, { userOutput: path.join(root, 'docs/next'), ...options }),
    channel: 'next', otherPages: new Set(stable ? stable.pages : []), stableVersion: stable ? stable.version : 'unavailable'
  };
  context.tracked = new Set(execFileSync('git', ['ls-files', '-z'], { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).split('\0'));
  const rendered = new Map(context.pages.map(page => [page.source, renderMarkdown(page.source, page.output, context)]));
  const outputs = new Map();
  for (const page of context.pages) {
    const result = rendered.get(page.source);
    outputs.set(page.output, pageTemplate({ ...page, title: page.title || result.title }, result.html, context));
  }
  outputs.set(context.hub, pageTemplate({ output: context.hub, title: 'User Documentation' }, hubContent(context, rendered), context, true));
  const legacyTarget = output => `${stable && stable.pages.includes(path.basename(output)) ? 'stable' : 'next'}/${path.basename(output)}`;
  for (const page of context.pages.filter(page => !page.developer)) {
    outputs.set(path.join(context.docs, path.basename(page.output)), compatibilityTemplate(legacyTarget(page.output)));
  }
  outputs.set(path.join(context.docs, 'documentation.html'), compatibilityTemplate('stable/documentation.html'));
  // Preserve stable-owned bookmarks even if a page is later removed from Next.
  for (const name of stable ? stable.pages : []) {
    if (!outputs.has(path.join(context.docs, name))) outputs.set(path.join(context.docs, name), compatibilityTemplate(`stable/${name}`));
    if (!outputs.has(path.join(context.docs, 'next', name))) outputs.set(path.join(context.docs, 'next', name), compatibilityTemplate('documentation.html'));
  }
  const legacyRoutes = Object.fromEntries([...context.routes].map(([source, output]) => [
    path.relative(context.docs, source).split(path.sep).join('/'),
    source.startsWith(path.join(context.docs, 'developer') + path.sep) ? path.relative(context.docs, output).split(path.sep).join('/') : legacyTarget(output)
  ]));
  for (const name of stable ? stable.pages : []) legacyRoutes[`user/${name === 'documentation.html' ? 'index.md' : name.replace(/\.html$/, '.md')}`] = `stable/${name}`;
  legacyRoutes['user/index.md'] = 'stable/documentation.html';
  outputs.set(path.join(context.docs, 'doc.html'), compatibilityTemplate(null, legacyRoutes));
  // These established root bookmarks predate the canonical developer route hierarchy.
  for (const name of ['build', 'repository_layout']) {
    outputs.set(path.join(context.docs, `${name}.html`), compatibilityTemplate(`developer/${name}.html`));
  }
  return { context, outputs, stable };
}

// Locate obsolete generated artifacts after canonical pages are removed or renamed.
function staleOutputs(docs, outputs) {
  const stale = [];
  function visit(directory) {
    for (const name of fs.readdirSync(directory).sort()) {
      const file = path.join(directory, name);
      if (file === path.join(docs, 'stable')) continue;
      if (fs.statSync(file).isDirectory()) visit(file);
      else if (name.endsWith('.html') && !outputs.has(file) && fs.readFileSync(file, 'utf8').includes(MARKER)) stale.push(file);
    }
  }
  visit(docs);
  return stale;
}

// Check without mutations, or write deterministic artifacts and remove only marked orphans.
function main(args = process.argv.slice(2)) {
  const check = args.length === 1 && args[0] === '--check';
  const promotion = args.length === 4 && args[0] === '--promote-stable' && args[2] === '--source-ref';
  if (args.length && !check && !promotion) throw new Error('Usage: node tools/docs/build_static_docs.cjs [--check | --promote-stable VERSION --source-ref REF]');
  if (promotion) {
    const preview = generate(undefined, { allowMissingStable: true });
    promoteStable(preview.context.root, args[1], args[3], new Set(['documentation.html', ...preview.context.pages.filter(page => !page.developer).map(page => path.basename(page.output))]), preview.outputs);
  }
  const { context, outputs, stable } = generate();
  validateOutputs(new Map([...outputs, ...stable.outputs]), context.docs);
  const stale = staleOutputs(context.docs, outputs);
  const changed = [...outputs].filter(([file, content]) => !fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== content);
  if (check && (changed.length || stale.length)) {
    console.error('Static documentation is stale; run node tools/docs/build_static_docs.cjs');
    for (const file of [...changed.map(([file]) => file), ...stale]) console.error(path.relative(context.root, file));
    return 1;
  }
  if (!check) {
    for (const [file, content] of changed) {
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, content, 'utf8');
    }
    for (const file of stale) fs.unlinkSync(file);
  }
  console.log(`OK: Stable ${stable.version} (${stable.pages.length - 1} guides), Next (${context.groups.flatMap(group => group.pages).length} guides), ${context.pages.filter(page => page.developer).length} current developer pages; ${check ? 'no drift' : `${changed.length} updated`}.`);
  return 0;
}

if (require.main === module) {
  try { process.exitCode = main(); }
  catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = { generate, main, staleOutputs };
