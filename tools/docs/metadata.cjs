const path = require('node:path');
const { site, publicUrl } = require('./site.cjs');

// Decode rendered prose once before escaping it for an HTML metadata attribute.
function plainText(html) {
  const entities = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ' };
  return html.replace(/<[^>]*>/g, '').replace(/&(#x[\da-f]+|#\d+|amp|lt|gt|quot|apos|nbsp);/gi, (all, entity) => {
    if (entity[0] !== '#') return entities[entity.toLowerCase()];
    const value = entity[1].toLowerCase() === 'x' ? parseInt(entity.slice(2), 16) : Number(entity.slice(1));
    return value > 0 && value <= 0x10ffff ? String.fromCodePoint(value) : all;
  }).replace(/\s+/g, ' ').trim();
}

// Keep useful source prose, with a factual fallback and deterministic word boundary.
function descriptionFrom(summaryHtml) {
  let text = plainText(summaryHtml || '') || site.description;
  if (text.length < 120) text += ` ${site.description}`;
  if (text.length > 170) text = text.slice(0, 167).replace(/\s+\S*$/, '').trimEnd() + '…';
  return text;
}

// Publish self-canonicals for each channel, including future tagged Stable pages.
function metadata(page, context, escape) {
  const route = path.relative(context.siteDocs || context.docs, page.output).split(path.sep).join('/');
  const canonical = publicUrl(route);
  const title = `${site.name} Documentation • ${page.title}`;
  const description = descriptionFrom(page.summaryHtml);
  return `<meta name="description" content="${escape(description)}" />
<link rel="canonical" href="${escape(canonical)}" />
<meta name="theme-color" content="${site.themeColor}" />
<meta property="og:site_name" content="${site.name}" />
<meta property="og:type" content="website" />
<meta property="og:title" content="${escape(title)}" />
<meta property="og:description" content="${escape(description)}" />
<meta property="og:url" content="${escape(canonical)}" />
<meta property="og:image" content="${publicUrl(site.socialImage)}" />
<meta name="twitter:card" content="summary_large_image" />
<meta name="twitter:title" content="${escape(title)}" />
<meta name="twitter:description" content="${escape(description)}" />
<meta name="twitter:image" content="${publicUrl(site.socialImage)}" />`;
}

module.exports = { plainText, descriptionFrom, metadata };
