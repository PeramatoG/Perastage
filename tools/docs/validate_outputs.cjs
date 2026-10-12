const fs = require('node:fs');
const path = require('node:path');

// Validate rendered resources and fragments against staged output before publishing it.
function validateOutputs(outputs, docs) {
  const read = file => outputs.has(file) ? outputs.get(file) : fs.readFileSync(file);
  const exists = file => outputs.has(file) || (fs.existsSync(file) && fs.statSync(file).isFile());
  const ids = file => new Set([...String(read(file)).matchAll(/\bid=["']([^"']+)["']/g)].map(match => match[1]));
  for (const [file, content] of outputs) {
    if (!file.endsWith('.html')) continue;
    for (const match of String(content).matchAll(/\b(?:href|src|poster)=["']([^"']*)["']/g)) {
      const href = match[1].replace(/&amp;/g, '&');
      if (/^(?:[a-z][\w+.-]*:|\/\/)/i.test(href)) continue;
      const parsed = new URL(href, 'https://validation.invalid/');
      const target = parsed.pathname === '/' && !href.split(/[?#]/)[0] ? file :
        path.resolve(path.dirname(file), decodeURIComponent(href.split(/[?#]/)[0] || path.basename(file)));
      if (!target.startsWith(docs + path.sep) || !exists(target)) throw new Error(`Missing generated link/resource in ${file}: ${href}`);
      if (parsed.hash && target.endsWith('.html') && !ids(target).has(decodeURIComponent(parsed.hash.slice(1)))) {
        throw new Error(`Missing generated fragment in ${file}: ${href}`);
      }
    }
  }
}

module.exports = { validateOutputs };
