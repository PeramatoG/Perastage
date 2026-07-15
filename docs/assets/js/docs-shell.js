const docsPages = [
  { md: 'user/installation.md', html: 'installation.html', title: 'Installation', icon: '🛠️', summary: 'Set up dependencies and install Perastage on your platform.' },
  { md: 'user/quick-start.md', html: 'quick-start.html', title: 'Quick Start', icon: '🚀', summary: 'Learn the fastest path to open a project and navigate the app.' },
  { md: 'user/opening-mvr-files.md', html: 'opening-mvr-files.html', title: 'Opening MVR Files', icon: '📁', summary: 'Import MVR scenes and inspect fixtures, trusses, and objects.' },
  { md: 'user/gdtf-download.md', html: 'gdtf-download.html', title: 'GDTF Download', icon: '⬇️', summary: 'Download and manage GDTF fixture profiles from supported sources.' },
  { md: 'user/views.md', html: 'views.html', title: 'Views', icon: '🧭', summary: 'Understand 2D, 3D, and layout views for scene review workflows.' },
  { md: 'user/preferences.md', html: 'preferences.html', title: 'Preferences', icon: '⚙️', summary: 'Configure application behavior, units, and user-facing defaults.' },
  { md: 'user/troubleshooting.md', html: 'troubleshooting.html', title: 'Troubleshooting', icon: '🩺', summary: 'Resolve common setup, import, and rendering issues quickly.' },
  { md: 'user/faq.md', html: 'faq.html', title: 'FAQ', icon: '❓', summary: 'Find concise answers to frequent workflow and feature questions.' },
  { md: 'user/features.md', html: 'features.html', title: 'Feature overview', icon: '✨', summary: 'Explore key capabilities and practical tools available in Perastage.' },
  { md: 'developer/build.md', html: 'build.html', title: 'Build guide', icon: '🏗️', summary: 'Compile Perastage from source with the supported toolchains.' },
  { md: 'developer/index.md', html: 'doc.html?md=developer/index.md', title: 'Developer docs', icon: '🧩', summary: 'Find architecture, packaging, policy, and technical notes.' }
];

const mdToHtmlMap = new Map(docsPages.map((page) => [page.md, page.html]));

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

// Builds a shared navigation list and highlights the active HTML page.
function renderNav(activeHtml) {
  return docsPages
    .map((p) => `<li><a class="${p.html === activeHtml ? 'active' : ''}" href="${p.html}">${p.icon} ${p.title}</a></li>`)
    .join('');
}

// Injects the standard documentation shell so all pages share header and navigation.
function renderDocShell(activeHtml) {
  const root = document.getElementById('app');
  root.innerHTML = `
    <a href="#content" class="skip-link">Skip to content</a>
    <div class="page-shell">
      <header class="site-header">
        <a class="brand" href="index.html" aria-label="Perastage docs home">
          <img src="assets/images/Perastage_logo.png" alt="Perastage logo" />
          <div><h1>Perastage</h1><p>Lighting and Rigging visualization tool</p></div>
        </a>
        <button id="menuToggle" class="menu-toggle" aria-expanded="false" aria-controls="docNav">Menu</button>
      </header>
      <div class="layout">
        <aside id="docNav" class="nav-panel" aria-label="Documentation navigation"><ul class="nav-links">${renderNav(activeHtml)}</ul></aside>
        <main class="content-panel"><div class="breadcrumb"><a href="index.html">Home</a> / ${activeHtml.replace('.html', '')}</div><div id="content"></div><a class="back-top" href="#top">Back to top</a></main>
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
  const currentFolder = currentMdFile.includes('/') ? currentMdFile.replace(/\/[^/]+$/, '') : '';
  const links = contentElement.querySelectorAll('a[href]');
  links.forEach((link) => {
    const href = link.getAttribute('href');
    if (!href || href.startsWith('#')) {
      return;
    }
    try {
      const parsed = new URL(href, window.location.href);
      if (!parsed.pathname.endsWith('.md')) {
        return;
      }
      const rawLink = href.split('#')[0];
      const rawPath = rawLink.startsWith('/') ? rawLink.replace(/^\/+/g, '') : `${currentFolder}/${rawLink}`;
      const htmlTarget = resolveMarkdownHref(rawPath);
      if (!htmlTarget) {
        return;
      }
      link.setAttribute('href', `${htmlTarget}${parsed.hash}`);
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
    })
    .catch((err) => {
      document.getElementById('content').innerHTML = `<p>Could not load documentation file: ${mdFile}</p>`;
      console.error(err);
    });
}
