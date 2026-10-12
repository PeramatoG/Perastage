const fs = require('node:fs');
const path = require('node:path');
const marked = require('./vendor/marked.min.js');

// The source index alone owns channel navigation, including historical inventories.
function parseNavigation(markdown) {
  const groups = [];
  const seen = new Set();
  let heading;
  for (const token of marked.lexer(markdown)) {
    if (token.type === 'heading' && token.depth === 2) heading = token.text;
    if (token.type !== 'list') continue;
    const pages = token.items.map(item => {
      const link = item.tokens.flatMap(part => part.tokens || []).find(part => part.type === 'link');
      if (!heading || !link || !/^[a-z\d_-]+\.md$/i.test(link.href) || link.href === 'index.md' || seen.has(link.href)) {
        throw new Error('Invalid or duplicate user navigation entry');
      }
      seen.add(link.href);
      return { file: link.href, title: link.text };
    });
    if (pages.length) groups.push({ title: heading, pages });
  }
  if (!groups.length) throw new Error('User navigation is empty');
  return groups;
}

// Discover canonical inputs with independently configurable source/output roots.
function discoverSources(root, options = {}) {
  const docs = path.join(root, 'docs');
  const userSource = options.userSource || path.join(docs, 'user');
  const userOutput = options.userOutput || docs;
  const developerSource = options.developerSource || path.join(docs, 'developer');
  const developerOutput = options.developerOutput || path.join(docs, 'developer');
  const hub = path.join(userOutput, 'documentation.html');
  const routes = new Map([[path.join(userSource, 'index.md'), hub]]);
  const groups = parseNavigation(fs.readFileSync(path.join(userSource, 'index.md'), 'utf8')).map(group => ({
    title: group.title,
    pages: group.pages.map(page => {
      const source = path.join(userSource, page.file);
      const output = path.join(userOutput, page.file.replace(/\.md$/, '.html'));
      if (!fs.statSync(source).isFile()) throw new Error(`Missing user page: ${page.file}`);
      routes.set(source, output);
      return { source, output, title: page.title, section: group.title };
    })
  }));
  const indexed = groups.flatMap(group => group.pages);
  const names = fs.readdirSync(userSource).filter(name => name.endsWith('.md') && name !== 'index.md').sort();
  if (!options.historical && names.join('\n') !== indexed.map(page => path.basename(page.source)).sort().join('\n')) {
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
  if (!options.userOnly) visit(developerSource);
  return { docs, hub, groups, routes, pages: [...indexed, ...developers], developerIndex: path.join(developerOutput, 'index.html') };
}

module.exports = { discoverSources, parseNavigation };
