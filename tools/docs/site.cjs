// Canonical public identity shared by documentation metadata and crawl artifacts.
const site = Object.freeze({
  origin: 'https://perastage.luismaperamato.com',
  name: 'Perastage',
  description: 'Perastage is a free, cross-platform desktop application to open, inspect, edit and document MVR scenes and work with GDTF fixtures.',
  socialImage: 'assets/images/screenshots/hero-main.png',
  themeColor: '#0b1220'
});

// Encode route segments while keeping one absolute, release-neutral public origin.
function publicUrl(route = '') {
  return `${site.origin}/${route.split('/').map(encodeURIComponent).join('/')}`;
}

module.exports = { site, publicUrl };
