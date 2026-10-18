const assert = require('node:assert/strict');
const { site, publicUrl } = require('../tools/docs/site.cjs');

// Inspect actual browser head values and image semantics on each public surface.
async function checkMetadata(page, route) {
  const meta = name => page.locator(`meta[name="${name}"], meta[property="${name}"]`).getAttribute('content');
  assert.equal(await page.locator('h1').count(), 1, route);
  if (route.startsWith('stable/')) {
    assert.equal(await page.locator('link[rel="canonical"]').getAttribute('href'), publicUrl(route));
    return; // The published 1.6.0 snapshot intentionally retains its original metadata and logo assets.
  }
  if (route === '404.html') {
    assert.equal(await meta('robots'), 'noindex,follow');
    assert.equal(await page.locator('main a').count(), 4);
    return;
  }
  const canonical = publicUrl(route === 'index.html' ? '' : route);
  assert.equal(await page.locator('link[rel="canonical"]').getAttribute('href'), canonical);
  const description = await meta('description');
  assert(description.length >= 120 && description.length <= 170);
  assert.equal(await meta('og:url'), canonical);
  assert.equal(await meta('og:site_name'), site.name);
  assert.equal(await meta('og:description'), description);
  assert.equal(await meta('twitter:description'), description);
  assert.equal(await meta('twitter:card'), 'summary_large_image');
  assert.equal(await meta('og:image'), publicUrl(site.socialImage));
  assert.equal(await meta('twitter:image'), publicUrl(site.socialImage));
  assert.equal(await meta('theme-color'), route === 'index.html' ? '#080808' : site.themeColor);
  const brand = page.locator('.brand');
  assert.equal(await brand.getAttribute('aria-label'), 'Perastage home');
  const logo = brand.locator('img');
  assert.equal(await logo.getAttribute('alt'), '');
  assert.equal(await logo.getAttribute('width'), '48');
  assert.equal(await logo.getAttribute('height'), '48');
  assert((await logo.getAttribute('src')).endsWith('logo-web.png'));
  assert((await page.locator('link[rel="icon"]').getAttribute('href')).endsWith('favicon.png'));
  assert((await page.locator('link[rel="apple-touch-icon"]').getAttribute('href')).endsWith('apple-touch-icon.png'));
  if (route.startsWith('next/')) {
    assert.equal(await meta('robots'), 'noindex,follow');
    assert.equal(await page.locator('.channel-switcher [aria-current]').getAttribute('aria-current'), 'page');
    assert((await page.locator('.doc-channel').innerText()).includes('may describe unreleased changes'));
  } else assert.equal(await page.locator('meta[name="robots"]').count(), 0);
  if (route === 'index.html') {
    const hero = page.locator('.product-screenshot');
    assert.equal(await hero.getAttribute('loading'), 'eager');
    assert.equal(await hero.getAttribute('fetchpriority'), 'high');
    assert.equal(await hero.getAttribute('decoding'), 'async');
    assert.deepEqual(await page.locator('script[src]').evaluateAll(scripts => scripts.map(script => script.getAttribute('src'))), ['assets/js/downloads.js']);
    assert.equal(await page.locator('script[src]').getAttribute('defer'), '');
    const data = JSON.parse(await page.locator('script[type="application/ld+json"]').textContent());
    assert.equal(data['@type'], 'SoftwareApplication');
  }
}

// Check underlines and WCAG contrast against actual rendered component backgrounds.
async function checkPresentation(page) {
  const result = await page.evaluate(() => {
    const style = getComputedStyle(document.documentElement);
    const rgb = name => style.getPropertyValue(name).trim().slice(1).match(/../g).map(value => parseInt(value, 16) / 255);
    const luminance = color => color.map(value => value <= .04045 ? value / 12.92 : ((value + .055) / 1.055) ** 2.4)
      .reduce((sum, value, index) => sum + value * [.2126, .7152, .0722][index], 0);
    const foregrounds = ['--text-primary', '--text-muted', '--accent', '--accent-strong'];
    const backgrounds = ['--bg-primary', '--bg-secondary', '--bg-card'].map(rgb);
    for (const element of document.querySelectorAll('.nav-panel, .content-panel, .hero-panel, .doc-card, .product-card, .product-release, .nav-links a.active, #content code, #content pre, #content th, #content td')) {
      const color = getComputedStyle(element).backgroundColor;
      const components = color.match(/[\d.]+/g).map(Number);
      if (components.length === 3 || components[3] === 1) backgrounds.push(components.slice(0, 3).map(value => value / 255));
    }
    const ratios = foregrounds.flatMap(name => backgrounds.map(background => {
      const a = luminance(rgb(name)), b = luminance(background);
      return (Math.max(a, b) + .05) / (Math.min(a, b) + .05);
    }));
    // Resolve actual visible text through its nearest opaque ancestor surface.
    const textRatios = [];
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    const parseColor = color => color.match(/[\d.]+/g).slice(0, 3).map(value => Number(value) / 255);
    while (walker.nextNode()) {
      const node = walker.currentNode, element = node.parentElement;
      if (!node.textContent.trim() || !element.checkVisibility() || element.closest('script, style, svg')) continue;
      let surface = element;
      while (surface && getComputedStyle(surface).backgroundColor === 'rgba(0, 0, 0, 0)') surface = surface.parentElement;
      const background = surface ? parseColor(getComputedStyle(surface).backgroundColor) : rgb('--bg-primary');
      const foreground = parseColor(getComputedStyle(element).color);
      const a = luminance(foreground), b = luminance(background);
      textRatios.push((Math.max(a, b) + .05) / (Math.min(a, b) + .05));
    }
    const links = [...document.querySelectorAll('#content p a, #content li a')];
    return {
      contrast: Math.min(...ratios, ...textRatios),
      underlined: links.every(link => getComputedStyle(link).textDecorationLine.includes('underline')),
      focusOutline: style.getPropertyValue('--accent-strong').trim(),
      allImagesHaveAlt: [...document.images].every(image => image.hasAttribute('alt')),
      contentImagesDeferred: [...document.querySelectorAll('#content img')].every(image => image.loading === 'lazy' && image.decoding === 'async')
    };
  });
  assert(result.contrast >= 4.5, `WCAG AA text/link contrast: ${result.contrast.toFixed(2)}`);
  assert(result.underlined, 'prose links must remain distinguishable without color');
  assert(result.allImagesHaveAlt);
  assert(result.contentImagesDeferred);
  return result.contrast;
}

module.exports = { checkMetadata, checkPresentation };
