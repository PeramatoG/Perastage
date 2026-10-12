#!/usr/bin/env node
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { discoverSources } = require('./sources.cjs');
const { renderMarkdown } = require('./render_markdown.cjs');
const { MARKER, pageTemplate, hubContent, compatibilityTemplate } = require('./templates.cjs');

// Build every artifact in memory before writing, also exposing the pipeline to fixtures.
function generate(root = path.resolve(__dirname, '../..'), options = {}) {
  const context = { root, ...discoverSources(root, options) };
  context.tracked = new Set(execFileSync('git', ['ls-files', '-z'], { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).split('\0'));
  const rendered = new Map(context.pages.map(page => [page.source, renderMarkdown(page.source, page.output, context)]));
  const outputs = new Map();
  for (const page of context.pages) {
    const result = rendered.get(page.source);
    outputs.set(page.output, pageTemplate({ ...page, title: page.title || result.title }, result.html, context));
  }
  outputs.set(context.hub, pageTemplate({ output: context.hub, title: 'User Documentation' }, hubContent(context, rendered), context, true));
  const legacyRoutes = Object.fromEntries([...context.routes].map(([source, output]) => [
    path.relative(context.docs, source).split(path.sep).join('/'),
    path.relative(context.docs, output).split(path.sep).join('/')
  ]));
  outputs.set(path.join(context.docs, 'doc.html'), compatibilityTemplate(null, legacyRoutes));
  // These established root bookmarks predate the canonical developer route hierarchy.
  for (const name of ['build', 'repository_layout']) {
    outputs.set(path.join(context.docs, `${name}.html`), compatibilityTemplate(`developer/${name}.html`));
  }
  return { context, outputs };
}

// Locate obsolete generated artifacts after canonical pages are removed or renamed.
function staleOutputs(docs, outputs) {
  const stale = [];
  function visit(directory) {
    for (const name of fs.readdirSync(directory).sort()) {
      const file = path.join(directory, name);
      if (fs.statSync(file).isDirectory()) visit(file);
      else if (name.endsWith('.html') && !outputs.has(file) && fs.readFileSync(file, 'utf8').includes(MARKER)) stale.push(file);
    }
  }
  visit(docs);
  return stale;
}

// Check without mutations, or write deterministic artifacts and remove only marked orphans.
function main(args = process.argv.slice(2)) {
  if (args.some(arg => arg !== '--check') || args.length > 1) throw new Error('Usage: node tools/docs/build_static_docs.cjs [--check]');
  const check = args.includes('--check');
  const { context, outputs } = generate();
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
  console.log(`OK: ${outputs.size} static routes (${context.groups.flatMap(group => group.pages).length} user, ${context.pages.filter(page => page.developer).length} developer, hub and 3 compatibility routes); ${check ? 'no drift' : `${changed.length} updated`}.`);
  return 0;
}

if (require.main === module) {
  try { process.exitCode = main(); }
  catch (error) { console.error(error.message); process.exitCode = 1; }
}
module.exports = { generate, main, staleOutputs };
