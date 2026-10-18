/* Enhance static installation help with validated, publication-time release metadata. */
(() => {
  const releases = 'https://github.com/PeramatoG/Perastage/releases';
  const section = document.getElementById('download');
  if (!section) return;

  // Validate metadata before exposing any URL; missing platform entries remain unavailable.
  function validate(data) {
    if (!data || data.schema_version !== 1 || !/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(data.version) ||
        data.tag !== `v${data.version}` || data.release_notes_url !== `${releases}/tag/${data.tag}` ||
        !data.published_at || !Number.isFinite(Date.parse(data.published_at)) ||
        !data.assets || typeof data.assets !== 'object' || Array.isArray(data.assets)) throw new Error('Invalid release metadata');
    const urls = new Set();
    const entries = [...section.querySelectorAll('[data-platform]')].map(slot => {
      const asset = data.assets[slot.dataset.platform];
      if (asset === undefined) return [slot, null];
      if (!asset || typeof asset.name !== 'string' || !asset.name || /[\/\\\x00-\x1f]/.test(asset.name) ||
          asset.url !== `${releases}/download/${data.tag}/${encodeURIComponent(asset.name)}` ||
          (asset.size !== undefined && (!Number.isSafeInteger(asset.size) || asset.size <= 0))) throw new Error('Invalid download asset');
      if (urls.has(asset.url) || !new RegExp(`(^|[^0-9])${data.version.replace(/\./g, '[._-]')}([^0-9]|$)`).test(asset.name)) throw new Error('Duplicate or stale asset');
      urls.add(asset.url);
      return [slot, asset];
    });
    return entries;
  }

  // Load once from the same static site; a failure never restores cached version links.
  async function load() {
    const status = document.getElementById('download-status');
    try {
      const response = await fetch('assets/data/latest-release.json', { cache: 'no-store' });
      if (!response.ok) throw new Error('Release metadata unavailable');
      const data = await response.json();
      const urls = new Set();
    const entries = validate(data);
      for (const [slot, asset] of entries) {
        if (!asset) continue;
        const link = document.createElement('a');
        link.className = 'product-button product-button-primary';
        link.href = asset.url;
        link.textContent = `Download ${slot.dataset.label}`;
        slot.replaceChildren(link);
        if (asset.size) {
          const size = document.createElement('span');
          size.className = 'download-size';
          size.textContent = `${(asset.size / 1048576).toFixed(1)} MiB`;
          slot.append(size);
        }
      }
      for (const link of document.querySelectorAll('[data-release-notes]')) link.href = data.release_notes_url;
      status.textContent = `Latest stable: ${data.version} · Published ${data.published_at.slice(0, 10)}`;
    } catch {
      status.textContent = 'Direct downloads are unavailable right now. Use the latest GitHub release below to find official packages.';
    }
  }
  load();
})();
