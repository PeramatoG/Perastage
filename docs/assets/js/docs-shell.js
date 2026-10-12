// Presentation mirror of docs/user/index.md, verified by check_docs_links.py.
// WEB-040 will replace this temporary metadata with the static documentation pipeline.
const docsSections = [
  { title: 'Getting Started', pages: [
    { md: 'user/installation.md', html: 'installation.html', title: 'Installation', icon: '🛠️', summary: 'Install a release package on your desktop platform.' },
    { md: 'user/quick-start.md', html: 'quick-start.html', title: 'Quick Start', icon: '🚀', summary: 'Learn the fastest path to open a project and navigate the app.' },
    { md: 'user/opening-mvr-files.md', html: 'opening-mvr-files.html', title: 'Opening MVR Files', icon: '📁', summary: 'Import MVR scenes and inspect fixtures, trusses, and objects.' },
  ] },
  { title: 'Workflows', pages: [
    { md: 'user/gdtf-download.md', html: 'gdtf-download.html', title: 'GDTF Download', icon: '⬇️', summary: 'Download and manage GDTF fixture profiles from supported sources.' },
    { md: 'user/inspector.md', html: 'inspector.html', title: 'MVR / GDTF Inspector', icon: '🔎', summary: 'Inspect project snapshots and external packages without editing them.' },
    { md: 'user/create-from-text.md', html: 'create-from-text.html', title: 'Create from Text', icon: '📝', summary: 'Create scene content from text, TXT or PDF riders.' },
    { md: 'user/views.md', html: 'views.html', title: 'Views', icon: '🧭', summary: 'Use 2D and 3D views to review and edit scenes.' },
    { md: 'user/layouts.md', html: 'layouts.html', title: 'Layouts and PDF', icon: '📄', summary: 'Arrange printable pages and export layout PDFs.' },
  ] },
  { title: 'Reference', pages: [
    { md: 'user/preferences.md', html: 'preferences.html', title: 'Preferences', icon: '⚙️', summary: 'Configure application behavior, units, and user-facing defaults.' },
    { md: 'user/shortcuts-and-command-bar.md', html: 'shortcuts-and-command-bar.html', title: 'Shortcuts and Command Bar', icon: '⌨️', summary: 'Look up keyboard, mouse and embedded Console commands.' },
    { md: 'user/features.md', html: 'features.html', title: 'Feature Overview', icon: '✨', summary: 'Explore key capabilities and practical tools available in Perastage.' },
  ] },
  { title: 'Help', pages: [
    { md: 'user/faq.md', html: 'faq.html', title: 'FAQ', icon: '❓', summary: 'Find concise answers to frequent workflow and feature questions.' },
    { md: 'user/troubleshooting.md', html: 'troubleshooting.html', title: 'Troubleshooting', icon: '🩺', summary: 'Resolve common setup, import, and rendering issues quickly.' },
  ] }
];
const docsPages = docsSections.flatMap((section) => section.pages);
const mdToHtmlMap = new Map(docsPages.map((page) => [page.md, page.html]));
// Preserve the existing build bookmark without adding it to the user inventory.
mdToHtmlMap.set('developer/build.md', 'build.html');

// Normalizes documentation markdown paths so website links can use nested folders safely.
function normalizeMarkdownPath(markdownPath) {
  const stack = [];
  markdownPath.split('/').forEach((part) => {
    if (!part || part === '.') {
      return;
    }
    if (part === '..') {
      stack.pop();
      return;
    }
    stack.push(part);
  });
  return stack.join('/');
}

// Renders the canonical user groups or the developer index's high-level context.
function renderNav(activeHtml, developer) {
  if (developer) {
    return `<ul class="nav-links">
      <li><a href="documentation.html">User Documentation</a></li>
      <li><a href="doc.html?md=developer/index.md">Developer Documentation</a></li>
    </ul>`;
  }
  return docsSections.map((section, index) => `<section class="nav-group" aria-labelledby="nav-section-${index}">
    <h2 id="nav-section-${index}">${section.title}</h2>
    <ul class="nav-links">${section.pages.map((page) => `<li><a class="${page.html === activeHtml ? 'active' : ''}"${page.html === activeHtml ? ' aria-current="page"' : ''} href="${page.html}"><span aria-hidden="true">${page.icon}</span> ${page.title}</a></li>`).join('')}</ul>
  </section>`).join('');
}

// Resolves page identity without displaying raw filenames, queries, or encoded paths.
function pageIdentity(activeHtml) {
  const section = docsSections.find((group) => group.pages.some((page) => page.html === activeHtml));
  if (section) {
    return { section: section.title, title: section.pages.find((page) => page.html === activeHtml).title };
  }
  const md = new URL(activeHtml, window.location.href).searchParams.get('md') || '';
  return { section: md.startsWith('developer/') || activeHtml === 'build.html' || activeHtml === 'repository_layout.html' ? 'Developer' : 'User Documentation', title: 'Documentation page' };
}

// Injects the standard documentation shell so all pages share header and navigation.
function renderDocShell(activeHtml) {
  const identity = pageIdentity(activeHtml);
  const root = document.getElementById('app');
  root.innerHTML = `
    <a href="#content" class="skip-link">Skip to content</a>
    <div class="page-shell">
      <header class="site-header">
        <a class="brand" href="index.html" aria-label="Perastage home">
          <img src="assets/images/Perastage_logo.png" alt="Perastage logo" />
          <div><h1>Perastage</h1><p>Lighting and Rigging visualization tool</p></div>
        </a>
        <button id="menuToggle" class="menu-toggle" aria-expanded="false" aria-controls="docNav">Menu</button>
      </header>
      <div class="layout">
        <aside id="docNav" class="nav-panel"><nav aria-label="Documentation navigation">${renderNav(activeHtml, identity.section === 'Developer')}</nav></aside>
        <main class="content-panel"><nav class="breadcrumb" aria-label="Breadcrumb"><a href="documentation.html">Documentation</a> / ${identity.section === 'Developer' ? '<a href="doc.html?md=developer/index.md">Developer</a>' : identity.section} / <span id="pageTitle">${identity.title}</span></nav><div id="content" tabindex="-1"></div><a class="back-top" href="#top">Back to top</a></main>
      </div>
      <footer>Perastage documentation • <a href="https://github.com/PeramatoG/Perastage">Main GitHub repository</a> • <a href="https://github.com/PeramatoG/Perastage/releases/latest">Latest releases</a></footer>
    </div>`;

  const toggle = document.getElementById('menuToggle');
  const nav = document.getElementById('docNav');
  toggle.addEventListener('click', () => {
    const expanded = toggle.getAttribute('aria-expanded') === 'true';
    toggle.setAttribute('aria-expanded', String(!expanded));
    nav.classList.toggle('open');
  });
}

// Resolves a markdown path from either explicit mapping or a generic markdown shell route.
function resolveMarkdownHref(markdownPath) {
  const normalizedPath = normalizeMarkdownPath(markdownPath);
  const mappedTarget = mdToHtmlMap.get(normalizedPath);
  if (mappedTarget) {
    return mappedTarget;
  }
  if (normalizedPath && normalizedPath.endsWith('.md')) {
    return `doc.html?md=${encodeURIComponent(normalizedPath)}`;
  }
  return null;
}

// Converts documentation Markdown links to their HTML shell counterparts.
function rewriteMarkdownLinks(contentElement, currentMdFile) {
  const currentFolder = currentMdFile.split('/').slice(0, -1).join('/');
  const links = contentElement.querySelectorAll('a[href]');
  links.forEach((link) => {
    const href = link.getAttribute('href');
    if (!href || href.startsWith('#') || /^(?:[a-z][a-z0-9+.-]*:|\/\/)/i.test(href)) {
      return;
    }
    try {
      const parsed = new URL(href, window.location.href);
      if (!parsed.pathname.endsWith('.md')) {
        return;
      }
      const rawLink = decodeURIComponent(href.split(/[?#]/)[0]);
      const rawPath = rawLink.startsWith('/') ? rawLink.replace(/^\/+/g, '') : `${currentFolder}/${rawLink}`;
      const htmlTarget = resolveMarkdownHref(rawPath);
      if (!htmlTarget) {
        return;
      }
      const separator = htmlTarget.includes('?') ? '&' : '?';
      link.setAttribute('href', `${htmlTarget}${parsed.search ? separator + parsed.search.slice(1) : ''}${parsed.hash}`);
    } catch (_unused) {
      // Ignores invalid URLs and keeps their original href value.
    }
  });
}

// Loads a markdown document, converts it to HTML, and injects it into the page content area.
function loadMarkdown(mdFile) {
  fetch(mdFile)
    .then((res) => res.text())
    .then((md) => {
      const contentElement = document.getElementById('content');
      contentElement.innerHTML = marked.parse(md);
      rewriteMarkdownLinks(contentElement, mdFile);
      const heading = contentElement.querySelector('h1');
      const page = docsPages.find((entry) => entry.md === normalizeMarkdownPath(mdFile));
      const title = page ? page.title : heading ? heading.textContent.trim() : 'Documentation page';
      document.getElementById('pageTitle').textContent = title;
      document.title = `Perastage Documentation • ${title}`;
      // Markdown arrives after initial navigation; honor inbound heading fragments then.
      if (window.location.hash) {
        const target = document.getElementById(decodeURIComponent(window.location.hash.slice(1)));
        if (target) target.scrollIntoView();
      }
    })
    .catch((err) => {
      document.getElementById('content').innerHTML = `<p>Could not load documentation file: ${mdFile}</p>`;
      console.error(err);
    });
}
