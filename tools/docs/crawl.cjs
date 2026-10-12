const path = require('node:path');
const { publicUrl } = require('./site.cjs');
const { escapeHtml } = require('./render_markdown.cjs');

// Respect a noindex directive regardless of attribute order in published HTML.
function indexable(html) {
  return ![...String(html).matchAll(/<meta\b[^>]*>/gi)].some(([tag]) =>
    /\bname\s*=\s*["']robots["']/i.test(tag) && /\bcontent\s*=\s*["'][^"']*\bnoindex\b/i.test(tag));
}

// Derive only indexable routes from the frozen release inventory and current pages.
function crawlOutputs(context, stable, outputs) {
  const routes = ['', ...(stable ? stable.pages.filter(name =>
    indexable(stable.outputs.get(path.join(stable.directory, name)))).map(name => `stable/${name}`) : [])];
  for (const page of context.pages.filter(page => page.developer)) {
    if (indexable(outputs.get(page.output))) {
      routes.push(path.relative(context.docs, page.output).split(path.sep).join('/'));
    }
  }
  const urls = [...new Set(routes.map(publicUrl))].sort();
  return new Map([
    [path.join(context.docs, 'sitemap.xml'), `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n${urls.map(url => `  <url><loc>${escapeHtml(url)}</loc></url>`).join('\n')}\n</urlset>\n`],
    [path.join(context.docs, 'robots.txt'), `User-agent: *\nAllow: /\n\nSitemap: ${publicUrl('sitemap.xml')}\n`]
  ]);
}

module.exports = { crawlOutputs, indexable };
