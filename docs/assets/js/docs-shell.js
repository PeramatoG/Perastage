const docsPages = [
  { md: 'installation.md', html: 'installation.html', title: 'Installation', icon: '🛠️' },
  { md: 'quick-start.md', html: 'quick-start.html', title: 'Quick Start', icon: '🚀' },
  { md: 'opening-mvr-files.md', html: 'opening-mvr-files.html', title: 'Opening MVR Files', icon: '📁' },
  { md: 'gdtf-download.md', html: 'gdtf-download.html', title: 'GDTF Download', icon: '⬇️' },
  { md: 'views.md', html: 'views.html', title: 'Views', icon: '🧭' },
  { md: 'preferences.md', html: 'preferences.html', title: 'Preferences', icon: '⚙️' },
  { md: 'troubleshooting.md', html: 'troubleshooting.html', title: 'Troubleshooting', icon: '🩺' },
  { md: 'faq.md', html: 'faq.html', title: 'FAQ', icon: '❓' },
  { md: 'features.md', html: 'features.html', title: 'Feature overview', icon: '✨' },
  { md: 'build.md', html: 'build.html', title: 'Build guide', icon: '🏗️' },
  { md: 'repository_layout.md', html: 'repository_layout.html', title: 'Repository layout', icon: '🧩' },
  { md: 'architecture.md', html: 'doc.html?md=architecture.md', title: 'Architecture', icon: '🏛️' },
  { md: 'shortcuts-and-command-bar.md', html: 'doc.html?md=shortcuts-and-command-bar.md', title: 'Shortcuts and commands', icon: '⌨️' }
];

const mdToHtmlMap = new Map(docsPages.map((page) => [page.md, page.html]));

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

// Resolves a markdown file name from either explicit mapping or a generic .md to doc shell route.
function resolveMarkdownHref(markdownName) {
  const mappedTarget = mdToHtmlMap.get(markdownName);
  if (mappedTarget) {
    return mappedTarget;
  }
  if (markdownName && markdownName.endsWith('.md')) {
    return `doc.html?md=${encodeURIComponent(markdownName)}`;
  }
  return null;
}

// Converts documentation Markdown links to their HTML shell counterparts.
function rewriteMarkdownLinks(contentElement) {
  const links = contentElement.querySelectorAll('a[href]');
  links.forEach((link) => {
    const href = link.getAttribute('href');
    if (!href || href.startsWith('#')) {
      return;
    }
    try {
      const parsed = new URL(href, window.location.href);
      const markdownName = parsed.pathname.split('/').pop();
      const htmlTarget = resolveMarkdownHref(markdownName);
      if (!htmlTarget) {
        return;
      }
      if (htmlTarget.includes('?')) {
        link.setAttribute('href', htmlTarget);
        return;
      }
      parsed.pathname = parsed.pathname.replace(/[^/]+$/, htmlTarget);
      link.setAttribute('href', `${parsed.pathname}${parsed.search}${parsed.hash}`);
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
      rewriteMarkdownLinks(contentElement);
    })
    .catch((err) => {
      document.getElementById('content').innerHTML = `<p>Could not load documentation file: ${mdFile}</p>`;
      console.error(err);
    });
}
