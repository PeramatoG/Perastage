// Enhance the static navigation; content and links remain available without JavaScript.
(() => {
  const toggle = document.getElementById('menuToggle');
  const nav = document.getElementById('docNav');
  if (!toggle || !nav) return;
  toggle.hidden = false;
  nav.classList.add('enhanced');
  toggle.addEventListener('click', () => {
    const expanded = toggle.getAttribute('aria-expanded') === 'true';
    toggle.setAttribute('aria-expanded', String(!expanded));
    nav.classList.toggle('open', !expanded);
  });
})();
