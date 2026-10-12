const fs = require('node:fs');
const path = require('node:path');
const marked = require('./vendor/marked.min.js');

// Encode text once when embedding it in generated HTML attributes or labels.
function escapeHtml(value) {
  return String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

// Construct a web-relative URL without depending on the host path separator.
function relativeUrl(output, target) {
  return path.relative(path.dirname(output), target).split(path.sep).map(part => encodeURIComponent(part)).join('/');
}

// Resolve canonical links and assets at build time, preserving queries and fragments.
function rewriteUrl(href, source, output, context, resource = false) {
  if (/^(?:file:|[a-z]:[\\/])/i.test(href || '')) throw new Error(`Unsupported filesystem URL in ${source}: ${href}`);
  if (!href || href.startsWith('#') || /^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href)) return href;
  const match = href.match(/^([^?#]*)([?#].*)?$/);
  const filename = decodeURIComponent(match[1]);
  const suffix = match[2] || '';
  if (!filename) return href;
  if (filename.startsWith('/') || filename.includes('\\') || /[\x00-\x1f]/.test(filename)) {
    throw new Error(`Unsupported absolute or filesystem URL in ${source}: ${href}`);
  }
  const target = path.resolve(path.dirname(source), filename);
  const relative = path.relative(context.root, target);
  if (relative === '..' || relative.startsWith(`..${path.sep}`) || path.isAbsolute(relative)) {
    throw new Error(`Link escapes repository in ${source}: ${href}`);
  }
  const published = context.routes.get(target);
  if (published) return relativeUrl(output, published) + suffix;
  if (context.tagged) return context.tagged.url(relative.split(path.sep).join('/'), output, suffix, resource);
  if (!fs.existsSync(target)) throw new Error(`Missing local link/asset in ${source}: ${href}`);
  // Only tracked files/directories may become repository URLs; never expose local files.
  const insideDocs = target.startsWith(context.docs + path.sep);
  if (!insideDocs || target.endsWith('.md')) {
    const repoPath = relative.split(path.sep).join('/');
    const directory = fs.statSync(target).isDirectory();
    if (!context.tracked.has(repoPath) && !(directory && [...context.tracked].some(file => file.startsWith(repoPath + '/')))) {
      throw new Error(`Untracked repository link in ${source}: ${href}`);
    }
    return `https://github.com/PeramatoG/Perastage/${directory ? 'tree' : 'blob'}/main/${repoPath.split('/').map(encodeURIComponent).join('/')}${suffix}`;
  }
  return relativeUrl(output, target) + suffix;
}

// Render with the vendored Marked slugger and rewrite real links, including raw HTML.
function renderMarkdown(source, output, context) {
  const markdown = fs.readFileSync(source, 'utf8').replace(/\r\n?/g, '\n');
  const renderer = new marked.Renderer();
  const originalLink = renderer.link;
  const originalImage = renderer.image;
  renderer.link = function(href, title, text) {
    return originalLink.call(this, rewriteUrl(href, source, output, context), title, text);
  };
  renderer.image = function(href, title, text) {
    return originalImage.call(this, rewriteUrl(href, source, output, context, true), title, text).replace('<img ', '<img loading="lazy" decoding="async" ');
  };
  renderer.html = html => html.replace(/\b(href|src|poster)=(['"])(.*?)\2/gi, (_, attribute, quote, url) =>
    `${attribute}=${quote}${escapeHtml(rewriteUrl(url.replace(/&amp;/g, '&'), source, output, context, attribute.toLowerCase() !== 'href'))}${quote}`);
  const html = marked.parse(markdown, { renderer, mangle: false, headerIds: true });
  const tokens = marked.lexer(markdown);
  const heading = tokens.find(token => token.type === 'heading' && token.depth === 1);
  const paragraph = tokens.find(token => token.type === 'paragraph');
  // Marked already escapes prose. Strip presentation tags and preserve its entities once.
  const summaryHtml = paragraph ? marked.parseInline(paragraph.text, { mangle: false }).replace(/<[^>]*>/g, '').replace(/\s+/g, ' ').trim() : '';
  const useful = tokens.find(token => token.type === 'paragraph' &&
    token.tokens && token.tokens.some(part => part.type === 'text' && /[a-z]{3}/i.test(part.text)));
  const descriptionHtml = useful ? marked.parseInline(useful.text, { mangle: false }).replace(/<img[^>]*>/g, '').replace(/<[^>]*>/g, '').replace(/\s+/g, ' ').trim() : '';
  return { html, title: heading ? heading.text : path.basename(source, '.md'), summaryHtml, descriptionHtml };
}

module.exports = { escapeHtml, relativeUrl, rewriteUrl, renderMarkdown };
