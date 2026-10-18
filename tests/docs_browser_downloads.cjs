const assert = require('node:assert/strict');
const current = require('../docs/assets/data/latest-release.json');

// Exercise actual download controls, fallback behavior and installation help at mobile/desktop sizes.
async function checkDownloads(browser, base) {
  const context = await browser.newContext();
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', error => errors.push(String(error)));
  page.on('request', request => {
    if (request.url().includes('api.github.com')) errors.push('Visitor queried GitHub API');
  });
  try {
    for (const width of [1440, 768, 390, 320]) {
      await page.setViewportSize({ width, height: 900 });
      await page.goto(base + 'index.html#download');
      await page.locator('[data-platform="windows"] a').waitFor();
      assert.equal(await page.locator('#download [data-platform] a').count(), 5);
      for (const [platform, asset] of Object.entries(current.assets)) {
        assert.equal(await page.locator(`[data-platform="${platform}"] a`).getAttribute('href'), asset.url);
      }
      const mac15 = await page.locator('[data-platform="macos15"] a').getAttribute('href');
      const mac26 = await page.locator('[data-platform="macos26"] a').getAttribute('href');
      assert.notEqual(mac15, mac26);
      assert(mac15.endsWith('-macOS15-arm64.dmg'));
      assert(mac26.endsWith('-macOS26-arm64.dmg'));
      for (const platform of ['macos15', 'macos26']) {
        assert(await page.locator(`[data-platform="${platform}"] a`).isVisible());
      }
      assert.equal(await page.locator('[data-release-notes]').first().getAttribute('href'), current.release_notes_url);
      assert(await page.locator('#download-status').textContent().then(text => text.includes(current.version)));
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
      const help = page.locator('[data-platform="windows"]').locator('..').locator('summary');
      const wasOpen = await help.locator('..').evaluate(element => element.open);
      await help.focus(); await page.keyboard.press('Enter');
      await page.waitForFunction(previous => document.querySelector('.download-card details').open !== previous, wasOpen);
      const focus = await help.evaluate(element => ({outline:getComputedStyle(element).outlineStyle, width:getComputedStyle(element).outlineWidth}));
      assert.notEqual(focus.outline, 'none'); assert(parseFloat(focus.width) >= 2);
    }
    const newer = JSON.parse(JSON.stringify(current).replaceAll(current.version, '1.8.0'));
    await page.route('**/assets/data/latest-release.json', route => route.fulfill({json: newer}));
    await page.reload(); await page.locator('[data-platform="windows"] a').waitFor();
    assert((await page.locator('#download').innerHTML()).includes('1.8.0'));
    assert(!(await page.locator('main').innerHTML()).includes(current.version), 'new stable leaves no pinned version links or title');
    await page.unroute('**/assets/data/latest-release.json');
    const partial = structuredClone(current); delete partial.assets.macos15;
    await page.route('**/assets/data/latest-release.json', route => route.fulfill({json: partial}));
    await page.reload(); await page.locator('[data-platform="windows"] a').waitFor();
    assert.equal(await page.locator('[data-platform="macos15"] a').count(), 0);
    assert(await page.locator('[data-platform="macos15"]').textContent().then(text => text.includes('unavailable')));
    assert.equal(await page.locator('#download [data-platform] a').count(), 4);
    await page.unroute('**/assets/data/latest-release.json');
    for (const invalid of [null, {}, {...current, version:'1.8.0-rc.1'}, {...current, tag:'v1.6.0'},
      {...current, assets: {windows: {...current.assets.windows, url:'javascript:alert(1)'}}},
      {...current, assets: {...current.assets, macos26: current.assets.macos15}},
      {...current, assets: {...current.assets, windows: {...current.assets.windows, size:0}}}]) {
      await page.route('**/assets/data/latest-release.json', route => route.fulfill({body:JSON.stringify(invalid), contentType:'application/json'}));
      await page.reload(); await page.waitForFunction(() => document.querySelector('#download-status').textContent.includes('unavailable right now'));
      assert.equal(await page.locator('#download [data-platform] a').count(), 0);
      assert(await page.locator('#download a[href$="/releases/latest"]').first().isVisible());
      assert(await page.locator('#features h2').isVisible());
      await page.unroute('**/assets/data/latest-release.json');
    }
    for (const failure of ['missing', 'invalid-json', 'network']) {
      await page.route('**/assets/data/latest-release.json', route => failure === 'network' ? route.abort() :
        route.fulfill({status:failure === 'missing' ? 404 : 200, body:'not JSON'}));
      await page.reload(); await page.waitForFunction(() => document.querySelector('#download-status').textContent.includes('unavailable right now'));
      assert.equal(await page.locator('#download [data-platform] a').count(), 0);
      await page.unroute('**/assets/data/latest-release.json');
    }
    assert.deepEqual(errors, []);
  } finally { await context.close(); }
  const disabled = await browser.newContext({javaScriptEnabled:false, viewport:{width:320,height:900}});
  try {
    const plain = await disabled.newPage();
    await plain.goto(base + 'index.html#download');
    assert(await plain.locator('#download h2').isVisible());
    assert(await plain.locator('#download a[href$="/releases/latest"]').first().isVisible());
    await plain.locator('.download-security summary').click();
    assert(await plain.locator('.download-security p').first().isVisible());
    assert(await plain.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    assert.equal(await plain.locator('#download [data-platform]').count(), 5);
    assert.equal(await plain.locator('#download [data-platform] a').count(), 0);
  } finally { await disabled.close(); }
  console.log('PASS: downloads at 1440/768/390/320px, five distinct destinations, newer stable refresh, missing platforms, invalid/missing/network metadata fallbacks, no browser API calls, keyboard/focus and JavaScript-disabled installation help.');
}
module.exports = { checkDownloads };
