// Focused DOM checks using Playwright supplied by the browser test environment.
// Serve docs/ locally, then run this file with DOCS_TEST_BASE_URL set if needed.
// PERASTAGE_CHROMIUM optionally selects an installed Chromium executable.
const { chromium } = require('playwright');
const fs = require('node:fs');
const assert = require('node:assert/strict');
const root = require('node:path').resolve(__dirname, '..');
const index = fs.readFileSync(`${root}/docs/user/index.md`, 'utf8');
const expected = [];
let section;
for (const line of index.split('\n')) {
  if (line.startsWith('## ')) section = line.slice(3);
  const link = line.match(/^- \[([^\]]+)\]\(([^)]+)\)/);
  if (link && !link[2].startsWith('../')) expected.push({section, title:link[1], md:'user/'+link[2], html:link[2].replace('.md','.html')});
}
(async () => {
  const browser = await chromium.launch({executablePath:process.env.PERASTAGE_CHROMIUM || undefined,headless:true,args:['--no-sandbox']});
  const page = await browser.newPage({viewport:{width:1280,height:900}});
  const errors=[]; page.on('pageerror', error=>errors.push(String(error)));
  const base=process.env.DOCS_TEST_BASE_URL || 'http://127.0.0.1:8765/';
  const ready = async route => {
    const response=await page.goto(base+route);assert.equal(response.status(),200,route);
    await page.waitForSelector('#content h1');
  };
  await page.goto(base+'documentation.html');
  assert.deepEqual(await page.locator('#docGrid h3').allTextContents(),[...new Set(expected.map(p=>p.section))]);
  assert.deepEqual(await page.locator('#docGrid .doc-card').evaluateAll(links=>links.map(a=>a.getAttribute('href'))),expected.map(p=>p.html));
  assert.equal(await page.locator('#docGrid a[href*="developer"], #docGrid a[href="build.html"]').count(),0);
  assert.equal(await page.locator('.developer-entry a').getAttribute('href'),'doc.html?md=developer/index.md');
  assert.equal(await page.locator('main .developer-entry').count(),1);
  for (const entry of expected) {
    await ready(entry.html);
    assert.deepEqual(await page.locator('#docNav h2').allTextContents(),[...new Set(expected.map(p=>p.section))]);
    assert.deepEqual(await page.locator('#docNav a').evaluateAll(links=>links.map(a=>a.getAttribute('href'))),expected.map(p=>p.html));
    assert.equal(await page.locator('[aria-current="page"]').getAttribute('href'),entry.html);
    assert.equal(await page.locator('.breadcrumb').innerText(),`Documentation / ${entry.section} / ${entry.title}`);
    assert.equal(await page.title(),`Perastage Documentation • ${entry.title}`);
    assert.equal(await page.locator('main').count(),1);
    assert.equal(await page.locator('.skip-link').getAttribute('href'),'#content');
  }
  await ready('doc.html?md=user%2Ffeatures.md');
  assert.equal(await page.locator('.breadcrumb').innerText(),'Documentation / Reference / Feature Overview');
  for (const route of ['doc.html?md=developer/index.md','doc.html?md=developer/architecture.md','doc.html?md=developer/technical-notes/index.md','build.html','repository_layout.html']) {
    await ready(route);
    assert.deepEqual(await page.locator('#docNav a').allTextContents(),['User Documentation','Developer Documentation']);
    assert.equal(await page.locator('#docNav a').nth(1).getAttribute('href'),'doc.html?md=developer/index.md');
    const title=await page.locator('#content h1').first().textContent();
    assert.equal(await page.locator('.breadcrumb').innerText(),`Documentation / Developer / ${title.trim()}`);
  }
  await ready('doc.html?md=developer/index.md');
  const architecture=page.locator('#content a').filter({hasText:'Architecture'}).first();
  assert.equal(await architecture.getAttribute('href'),'doc.html?md=developer%2Farchitecture.md');
  await architecture.click(); await page.waitForSelector('#content h1');
  assert.equal(new URL(page.url()).searchParams.get('md'),'developer/architecture.md');
  // Exercise link rewriting on real DOM nodes: queries, fragments, nesting and escaping.
  const rewritten=await page.evaluate(()=>{
    const node=document.createElement('div');
    const hrefs=['../user/quick-start.md?mode=compact#next','technical-notes/index.md#viewer-and-rendering-contracts','../user/features.md#overview','build.md#building','cli.md?mode=compact#usage','repository%5Flayout.md#paths','#local','https://example.org/file.md#x'];
    hrefs.forEach(href=>{const a=document.createElement('a');a.setAttribute('href',href);node.append(a);});
    rewriteMarkdownLinks(node,'developer/index.md');return [...node.children].map(a=>a.getAttribute('href'));
  });
  assert.deepEqual(rewritten,['quick-start.html?mode=compact#next','doc.html?md=developer%2Ftechnical-notes%2Findex.md#viewer-and-rendering-contracts','features.html#overview','build.html#building','doc.html?md=developer%2Fcli.md&mode=compact#usage','doc.html?md=developer%2Frepository_layout.md#paths','#local','https://example.org/file.md#x']);
  await ready('doc.html?md=developer/index.md#active-subsystem-and-technical-contracts');
  assert(await page.locator('#active-subsystem-and-technical-contracts').evaluate(el=>window.scrollY>0 && el.getBoundingClientRect().top>=0 && el.getBoundingClientRect().top<window.innerHeight));
  await page.setViewportSize({width:390,height:844});
  for (const route of ['quick-start.html','doc.html?md=developer/index.md']) {
    await ready(route);
    const nav=page.locator('#docNav'),toggle=page.locator('#menuToggle');
    assert.equal(await nav.isVisible(),false);
    assert.equal(await toggle.getAttribute('aria-expanded'),'false');
    await toggle.focus();await page.keyboard.press('Enter');
    assert.equal(await nav.isVisible(),true);assert.equal(await toggle.getAttribute('aria-expanded'),'true');
    await page.keyboard.press('Space');assert.equal(await nav.isVisible(),false);assert.equal(await toggle.getAttribute('aria-expanded'),'false');
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth));
  }
  assert.deepEqual(errors,[]);
  await browser.close();
  console.log('PASS: grouped hub and all 13 public routes, generic user/developer routes, titles/breadcrumbs, nested Markdown links, queries/fragments, inbound scrolling, semantic landmarks and keyboard/mobile toggle.');
})().catch(error=>{console.error(error);process.exit(1);});
