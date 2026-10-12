const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { checkPresentation } = require('./docs_browser_metadata.cjs');

// Check shared components, including frozen Stable markup and unenhanced redirects.
async function checkVisualSystem(browser, base, observe, errors) {
  const routes = [
    'index.html', 'stable/documentation.html', 'stable/quick-start.html',
    'next/documentation.html', 'next/quick-start.html', 'developer/index.html',
    'developer/technical-notes/gdtf-editor/gdtf_editor_architecture.html', '404.html'
  ];
  const fallbackRoutes = ['documentation.html', 'quick-start.html', 'build.html', 'repository_layout.html', 'doc.html'];
  const screenshotRoutes = ['index.html', 'stable/documentation.html', 'next/documentation.html', 'developer/index.html', 'doc.html', '404.html'];
  const screenshots = process.env.DOCS_TEST_SCREENSHOTS;
  if (screenshots) fs.mkdirSync(screenshots, { recursive: true });
  let minimumContrast = Infinity;
  for (const javaScriptEnabled of [true, false]) {
    const context = await browser.newContext({ javaScriptEnabled });
    const page = await context.newPage();
    observe(page, errors);
    try {
      for (const width of [1440, 1024, 768, 390, 320]) {
        await page.setViewportSize({ width, height: 900 });
        for (const route of javaScriptEnabled ? routes : [...routes, ...fallbackRoutes]) {
          await page.goto(base + route);
          await page.waitForLoadState('load');
          assert.equal(page.url(), base + route, `${route}: fallback must not redirect without JavaScript`);
          const styles = await page.evaluate(() => {
            const computed = element => getComputedStyle(element);
            const body = computed(document.body);
            const header = document.querySelector('.site-header, .product-header');
            const technical = [...document.querySelectorAll('.nav-panel, .content-panel, .hero-panel, .doc-card, #content pre, #content code, .channel-switcher a, .menu-toggle')];
            return {
              background: body.backgroundColor, backgroundImage: body.backgroundImage,
              overflow: document.documentElement.scrollWidth > innerWidth,
              header: { background: computed(header).backgroundColor, radius: computed(header).borderTopLeftRadius,
                shadow: computed(header).boxShadow, border: computed(header).borderBottomWidth },
              technical: technical.map(element => ({ name: element.className || element.tagName,
                radius: computed(element).borderTopLeftRadius, shadow: computed(element).boxShadow,
                background: computed(element).backgroundColor })),
              taglineHidden: [...document.querySelectorAll('.brand p')].every(element => computed(element).display === 'none'),
              normalNavNeutral: [...document.querySelectorAll('.nav-links a')].every(element => computed(element).color === 'rgb(238, 238, 238)')
            };
          });
          assert.equal(styles.background, 'rgb(8, 8, 8)', route);
          assert.equal(styles.backgroundImage, 'none', route);
          assert.equal(styles.overflow, false, `${width}px ${route}: horizontal overflow`);
          assert.equal(styles.header.background, 'rgb(24, 24, 24)', route);
          assert.equal(styles.header.radius, '0px', route);
          assert.equal(styles.header.shadow, 'none', route);
          assert.equal(styles.header.border, '1px', route);
          assert(styles.taglineHidden, route);
          assert(styles.normalNavNeutral, route);
          for (const surface of styles.technical) {
            assert(parseFloat(surface.radius) <= 2, `${route}: ${surface.name} radius`);
            assert.equal(surface.shadow, 'none', `${route}: ${surface.name} shadow`);
            assert(['rgba(0, 0, 0, 0)', 'rgb(8, 8, 8)', 'rgb(18, 18, 18)', 'rgb(24, 24, 24)'].includes(surface.background), `${route}: ${surface.name} neutral surface`);
          }
          assert(await page.locator('main h1').isVisible(), route);
          minimumContrast = Math.min(minimumContrast, await checkPresentation(page));
          const nav = page.locator('#docNav');
          if (await nav.count()) {
            if (width <= 860 && javaScriptEnabled) {
              const toggle = page.locator('#menuToggle');
              assert.equal(await nav.isVisible(), false);
              await toggle.focus();
              const focus = await toggle.evaluate(element => {
                const style = getComputedStyle(element);
                return { width:style.outlineWidth, color:style.outlineColor, rect:element.getBoundingClientRect().toJSON() };
              });
              assert(parseFloat(focus.width) >= 3);
              assert.equal(focus.color, 'rgb(255, 176, 0)');
              assert(focus.rect.width >= 44 && focus.rect.height >= 44);
              await page.keyboard.press('Enter');
              assert.equal(await nav.isVisible(), true);
              assert.equal(await toggle.getAttribute('aria-expanded'), 'true');
            } else assert(await nav.isVisible(), route);
            if (/^(stable|next)\/quick-start/.test(route)) {
              const active = nav.locator('[aria-current="page"]');
              assert.equal(await active.count(), 1);
              const style = await active.evaluate(element => {
                const s = getComputedStyle(element);
                return { marker:s.borderLeftWidth, color:s.borderLeftColor, underline:s.textDecorationLine, weight:s.fontWeight };
              });
              assert.equal(style.marker, '3px');
              assert.equal(style.color, 'rgb(0, 255, 0)');
              assert(style.underline.includes('underline'));
              assert(Number(style.weight) >= 600);
            }
            if (width <= 860 && javaScriptEnabled) {
              await page.keyboard.press('Space');
              assert.equal(await nav.isVisible(), false);
              assert.equal(await page.locator('#menuToggle').getAttribute('aria-expanded'), 'false');
            }
          }
          if (fallbackRoutes.includes(route)) {
            assert.equal(await page.locator('meta[name="robots"]').getAttribute('content'), 'noindex,follow');
            assert.equal(await page.locator('meta[name="theme-color"]').getAttribute('content'), '#080808');
            assert((await page.locator('link[rel="icon"]').getAttribute('href')).endsWith('favicon.png'));
            assert(await page.locator('main p a').first().isVisible());
          }
          // Check a real keyboard focus indicator and card hover border, not pixel baselines.
          const card = page.locator('.doc-card').first();
          if (await card.count()) {
            await card.focus();
            assert.equal(await card.evaluate(element => getComputedStyle(element).outlineStyle), 'solid');
            assert.equal(await card.evaluate(element => getComputedStyle(element).borderTopColor), 'rgb(0, 255, 0)');
          }
          if (screenshots && [1440, 390].includes(width) && screenshotRoutes.includes(route)
              && (route === 'doc.html' ? !javaScriptEnabled : javaScriptEnabled)) {
            // Return to the header after focus scrolling before capturing representative views.
            await page.locator('.brand').focus();
            await page.screenshot({ path:path.join(screenshots, `${width}-${route.replaceAll('/', '-')}.png`) });
          }
        }
      }
    } finally { await context.close(); }
  }
  return minimumContrast;
}

module.exports = { checkVisualSystem };
