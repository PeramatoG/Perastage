const fs = require('node:fs');
const path = require('node:path');
const marked = require('./vendor/marked.min.js');

// Discover canonical inputs with independently configurable source/output roots.
function discoverSources(root, options = {}) {
  const docs = path.join(root, 'docs');
  const userSource = options.userSource || path.join(docs, 'user');
  const userOutput = options.userOutput || docs;
  const developerSource = options.developerSource || path.join(docs, 'developer');
  const developerOutput = options.developerOutput || path.join(docs, 'developer');
  const hub = path.join(userOutput, 'documentation.html');
  const routes = new Map([[path.join(userSource, 'index.md'), hub]]);
  const groups = [];
  let heading;
  for (const token of marked.lexer(fs.readFileSync(path.join(userSource, 'index.md'), 'utf8'))) {
    if (token.type === 'heading' && token.depth === 2) heading = token.text;
    if (token.type !== 'list') continue;
    const pages = [];
    for (const item of token.items) {
      const link = item.tokens.flatMap(part => part.tokens || []).find(part => part.type === 'link');
      if (!link || !/^[^/]+\.md$/.test(link.href) || link.href === 'index.md') {
        throw new Error('User navigation lists must link directly to canonical user pages');
      }
      const source = path.join(userSource, link.href);
      const output = path.join(userOutput, link.href.replace(/\.md$/, '.html'));
      if (!heading || routes.has(source) || !fs.statSync(source).isFile()) {
        throw new Error(`Invalid or duplicate user navigation entry: ${link.href}`);
      }
      routes.set(source, output);
      pages.push({ source, output, title: link.text, section: heading });
    }
    if (pages.length) groups.push({ title: heading, pages });
  }
  const indexed = groups.flatMap(group => group.pages);
  const names = fs.readdirSync(userSource).filter(name => name.endsWith('.md') && name !== 'index.md').sort();
  if (names.join('\n') !== indexed.map(page => path.basename(page.source)).sort().join('\n')) {
    throw new Error('Canonical user index must list every user page exactly once');
  }
  const developers = [];
  // Recursion is confined to the developer source root; localized mirrors are excluded.
  function visit(directory) {
    for (const name of fs.readdirSync(directory).sort()) {
      const source = path.join(directory, name);
      if (fs.statSync(source).isDirectory()) visit(source);
      else if (name.endsWith('.md')) {
        const output = path.join(developerOutput, path.relative(developerSource, source).replace(/\.md$/, '.html'));
        routes.set(source, output);
        developers.push({ source, output, developer: true });
      }
    }
  }
  visit(developerSource);
  return { docs, hub, groups, routes, pages: [...indexed, ...developers], developerIndex: path.join(developerOutput, 'index.html') };
}

module.exports = { discoverSources };
