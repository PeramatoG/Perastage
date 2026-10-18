// Focused DOM checks using Playwright supplied by the browser test environment.
// Serve docs/ locally, then run with DOCS_TEST_BASE_URL and optional PERASTAGE_CHROMIUM.
let chromium;
try { ({ chromium } = require('playwright')); }
catch (error) {
  if (error.code !== 'MODULE_NOT_FOUND') throw error;
  ({ chromium } = require('playwright-core'));
}
const fs = require('node:fs');
const assert = require('node:assert/strict');
const { checkMetadata, checkPresentation } = require('./docs_browser_metadata.cjs');
const { checkVisualSystem } = require('./docs_browser_visual_system.cjs');
const { checkDownloads } = require('./docs_browser_downloads.cjs');
const root = require('node:path').resolve(__dirname, '..');
const index = fs.readFileSync(`${root}/docs/user/index.md`, 'utf8');
const expected = [];
let section;
for (const line of index.split('\n')) {
  if (line.startsWith('## ')) section = line.slice(3);
  const link = line.match(/^- \[([^\]]+)\]\(([^)]+)\)/);
  if (link && !link[2].startsWith('../')) expected.push({section, title:link[1], html:link[2].replace('.md','.html')});
}

// Observe errors and forbidden runtime Markdown requests on every browser context.
function observe(page, errors) {
  page.on('pageerror', error => errors.push(String(error)));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  page.on('requestfailed', request => errors.push(`${request.url()}: ${request.failure().errorText} (${request.resourceType()} on ${page.url()})`));
  page.on('response', response => { if (response.status() >= 400) errors.push(`${response.status()}: ${response.url()}`); });
  page.on('request', request => {
    if (/\.md$|marked\.min\.js/.test(new URL(request.url()).pathname)) errors.push(`Runtime Markdown request: ${request.url()}`);
  });
}

// Assert the content and links exist in the response, before any DOM enhancement.
async function ready(page, base, route) {
  const response = await page.goto(base + route);
  assert.equal(response.status(), 200, route);
  await page.waitForSelector('#content h1');
  await page.waitForLoadState('networkidle');
  const html = await page.request.get(page.url());
  const initial = await html.text();
  assert(initial.includes('<h1 id='), route);
  assert(initial.includes('aria-label="Documentation navigation"'), route);
  assert(!/loadMarkdown\(|marked\.min\.js|docs-shell\.js/.test(initial), route);
}

(async () => {
  const browser = await chromium.launch({executablePath:process.env.PERASTAGE_CHROMIUM || undefined,headless:true,args:['--no-sandbox']});
  try {
    const page = await browser.newPage({viewport:{width:1280,height:900}});
    const errors = [];
    observe(page, errors);
    const base = process.env.DOCS_TEST_BASE_URL || 'http://127.0.0.1:8765/';
    await checkDownloads(browser, base);
    const visualContrast = await checkVisualSystem(browser, base, observe, errors);
    const stable = JSON.parse(fs.readFileSync(`${root}/docs/stable/manifest.json`, 'utf8'));
    const stableHas = route => stable.pages.includes(route);
    let contrast;
    for (const route of ['index.html', '404.html', 'stable/quick-start.html', 'next/documentation.html', 'next/quick-start.html', 'developer/index.html', 'developer/build.html']) {
      await page.goto(base + route);
      await page.waitForLoadState('networkidle');
      assert.equal(page.url(), base + route, 'normal pages must not redirect');
      await checkMetadata(page, route);
      contrast = await checkPresentation(page);
    }
    await page.goto(base + 'index.html');
    await page.keyboard.press('Tab');
    assert.equal(await page.locator(':focus').textContent(), 'Skip to content');
    await page.keyboard.press('Enter');
    assert.equal(await page.locator(':focus').getAttribute('id'), 'main');
    await ready(page, base, 'documentation.html');
    assert.equal(page.url(), base + 'stable/documentation.html');
    assert.equal(await page.locator('.channel-label').textContent(), `Stable ${stable.version}`);
    assert.equal(await page.locator('#docGrid .doc-card').count(), stable.pages.length - 1);
    await page.locator('.channel-switcher a').filter({hasText:'Next'}).click();
    assert.equal(page.url(), base + 'next/documentation.html');
    assert.deepEqual(await page.locator('#docGrid h2').allTextContents(), [...new Set(expected.map(p=>p.section))]);
    assert.deepEqual(await page.locator('#docGrid .doc-card').evaluateAll(links=>links.map(a=>a.getAttribute('href'))), expected.map(p=>p.html));
    assert.equal(await page.locator('#docGrid a[href*="developer"], #docGrid a[href="build.html"]').count(), 0);
    assert.equal(await page.locator('.developer-entry a').getAttribute('href'), '../developer/index.html');
    assert.equal(await page.locator('main .developer-entry').count(), 1);
    assert((await page.locator('#docGrid .doc-card').filter({hasText:'MVR / GDTF Inspector'}).innerText()).includes("project's snapshot"));
    for (const entry of expected) {
      await ready(page, base, 'next/' + entry.html);
      assert.deepEqual(await page.locator('#docNav h2').allTextContents(), [...new Set(expected.map(p=>p.section))]);
      assert.deepEqual(await page.locator('#docNav a').evaluateAll(links=>links.map(a=>a.getAttribute('href'))), expected.map(p=>p.html));
      assert.equal(await page.locator('#docNav [aria-current="page"]').getAttribute('href'), entry.html);
      assert.equal(await page.locator('.breadcrumb').innerText(), `Documentation / ${entry.section} / ${entry.title}`);
      assert.equal(await page.title(), `Perastage Documentation • ${entry.title}`);
      assert.equal(await page.locator('main').count(), 1);
      assert.equal(await page.locator('.skip-link').getAttribute('href'), '#content');
    }
    for (const route of ['developer/index.html','developer/architecture.html','developer/technical-notes/index.html','developer/technical-notes/gdtf-editor/gdtf_editor_architecture.html']) {
      await ready(page, base, route);
      assert.deepEqual(await page.locator('#docNav a').allTextContents(), ['User Documentation','Developer Documentation']);
      const title = await page.locator('#content h1').first().textContent();
      assert.equal(await page.locator('.breadcrumb').innerText(), `Documentation / Developer / ${title.trim()}`);
      await page.locator('#docNav a').nth(1).click();
      await page.waitForURL(base + 'developer/index.html');
    }
    await ready(page, base, 'developer/index.html');
    const architecture = page.locator('#content a').filter({hasText:'Architecture'}).first();
    assert.equal(await architecture.getAttribute('href'), 'architecture.html');
    await architecture.click();
    await page.waitForURL(base + 'developer/architecture.html');
    // A representative generated GFM table and code fence contain semantic HTML.
    await ready(page, base, 'developer/build.html');
    assert(await page.locator('#content pre code').count() > 0);
    await ready(page, base, 'next/quick-start.html');
    await page.locator('.channel-switcher a').filter({hasText:'Stable'}).click();
    assert.equal(page.url(), base + 'stable/quick-start.html');
    await page.locator('.channel-switcher a').filter({hasText:'Next'}).click();
    assert.equal(page.url(), base + 'next/quick-start.html');
    await ready(page, base, 'next/inspector.html');
    if (!stableHas('inspector.html')) assert((await page.locator('.doc-channel').innerText()).includes('unavailable in Stable'));
    await page.locator('.channel-switcher a').filter({hasText:'Stable'}).click();
    assert.equal(page.url(), base + (stableHas('inspector.html') ? 'stable/inspector.html' : 'stable/documentation.html'));
    await ready(page, base, 'next/preferences.html#mvr-import--export');
    assert(await page.locator('#mvr-import--export').evaluate(el=>window.scrollY>0 && el.getBoundingClientRect().top>=0 && el.getBoundingClientRect().top<window.innerHeight));
    await ready(page, base, 'developer/index.html#active-subsystem-and-technical-contracts');
    assert(await page.locator('#active-subsystem-and-technical-contracts').evaluate(el=>window.scrollY>0 && el.getBoundingClientRect().top>=0 && el.getBoundingClientRect().top<window.innerHeight));
    // Only these intentional compatibility checks use old query routes.
    for (const [legacy, target] of [
      ['doc.html?md=user%2Ffeatures.md', `${stableHas('features.html') ? 'stable' : 'next'}/features.html`],
      ['doc.html?md=user/index.md', 'stable/documentation.html'],
      ['doc.html?md=user/inspector.md#choose-what-to-inspect', `${stableHas('inspector.html') ? 'stable' : 'next'}/inspector.html#choose-what-to-inspect`],
      ['quick-start.html', 'stable/quick-start.html'],
      ['inspector.html', `${stableHas('inspector.html') ? 'stable' : 'next'}/inspector.html`],
      ['quick-start.html?mode=compact#quick-start', 'stable/quick-start.html?mode=compact#quick-start'],
      ['doc.html?md=developer/index.md', 'developer/index.html'],
      ['doc.html?md=developer/build.md', 'developer/build.html'],
      ['doc.html?md=developer/repository_layout.md', 'developer/repository_layout.html'],
      ['doc.html?md=developer/technical-notes/index.md#viewer-and-rendering-contracts', 'developer/technical-notes/index.html#viewer-and-rendering-contracts'],
      ['doc.html?md=developer%2Findex.md%23active-subsystem-and-technical-contracts', 'developer/index.html#active-subsystem-and-technical-contracts'],
      ['doc.html?md=developer/build.md&mode=compact#cmake-presets-strategy', 'developer/build.html?mode=compact#cmake-presets-strategy'],
      ['build.html#cmake-presets-strategy', 'developer/build.html#cmake-presets-strategy'],
      ['repository_layout.html#documentation-layout', 'developer/repository_layout.html#documentation-layout']
    ]) {
      await ready(page, base, legacy);
      assert.equal(page.url(), base + target);
      const fragment = new URL(page.url()).hash;
      if (fragment) assert.equal(await page.locator(fragment).count(), 1, legacy);
    }
    for (const md of ['../../etc/passwd','/etc/passwd','C:\\private.md','https://example.org/file.md','developer/../user/features.md','developer/missing.md','user/locales/es/features.md']) {
      await ready(page, base, `doc.html?md=${encodeURIComponent(md)}`);
      assert.equal(page.url(), base + 'developer/index.html');
    }
    await page.setViewportSize({width:390,height:844});
    for (const route of ['index.html', '404.html', 'next/documentation.html']) {
      await page.goto(base + route);
      await page.waitForLoadState('networkidle');
      await checkMetadata(page, route);
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), route);
    }
    for (const route of ['stable/quick-start.html','next/inspector.html','developer/index.html','developer/technical-notes/gdtf-editor/gdtf_editor_architecture.html']) {
      await ready(page, base, route);
      const nav = page.locator('#docNav'), toggle = page.locator('#menuToggle');
      assert.equal(await nav.isVisible(), false);
      assert.equal(await toggle.getAttribute('aria-expanded'), 'false');
      await toggle.focus(); await page.keyboard.press('Enter');
      assert.equal(await nav.isVisible(), true); assert.equal(await toggle.getAttribute('aria-expanded'), 'true');
      await page.keyboard.press('Space');
      assert.equal(await nav.isVisible(), false); assert.equal(await toggle.getAttribute('aria-expanded'), 'false');
      assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
    }
    const disabled = await browser.newContext({javaScriptEnabled:false, viewport:{width:1280,height:900}});
    const plain = await disabled.newPage();
    observe(plain, errors);
    for (const route of ['stable/documentation.html','stable/installation.html','next/documentation.html','next/installation.html','developer/index.html']) {
      await ready(plain, base, route);
      assert(await plain.locator('#content h1').isVisible());
      assert(await plain.locator('#docNav').isVisible());
    }
    await plain.setViewportSize({width:390,height:844});
    await ready(plain, base, 'next/installation.html');
    assert(await plain.locator('#docNav').isVisible(), 'mobile navigation remains available without JavaScript');
    assert.equal(await plain.locator('#menuToggle').isVisible(), false);
    await plain.goto(base + 'documentation.html');
    assert.equal(await plain.locator('link[rel="canonical"]').getAttribute('href'), 'https://perastage.luismaperamato.com/stable/documentation.html');
    assert.equal(await plain.locator('meta[name="robots"]').getAttribute('content'), 'noindex,follow');
    assert(await plain.locator('a[href="stable/documentation.html"]').isVisible());
    await plain.goto(base + 'build.html');
    assert(await plain.locator('a[href="developer/build.html"]').isVisible());
    for (const route of ['index.html', '404.html']) {
      await plain.goto(base + route);
      assert(await plain.locator('main h1').isVisible());
      await checkMetadata(plain, route);
      assert(await plain.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth));
    }
    assert.deepEqual(errors, []);
    await disabled.close();
    console.log(`PASS: unified visual system at 1440/1024/768/390/320px, all families with/without JavaScript, landing/404 desktop/mobile and DOM metadata, Stable/Next hubs and switchers, all 13 Next user routes, developer/nested routes, links, fragments, legacy/safe redirects, keyboard/mobile navigation, JavaScript-disabled content, WCAG AA palette (minimum ${Math.min(contrast, visualContrast).toFixed(2)}:1), and zero console/local resource errors.`);
  } finally {
    await browser.close();
  }
})().catch(error=>{console.error(error);process.exit(1);});
