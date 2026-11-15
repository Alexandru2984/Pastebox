// Preserve the existing self-hosted analytics integration, with manual public-only
// pageviews. Never send paste IDs/titles, owner keys, form values or full referrers.
const WEBSITE = '71064230-990b-40f3-8d78-6bc6a53adcb5';
let tracker;
function publicRoute() {
  if (location.pathname !== '/') return '';
  return !location.hash || location.hash === '#/' ? '/' : location.hash === '#/list' ? '/#/list' : '';
}
export async function trackPublicPage() {
  const route = publicRoute();
  if (!route || location.hostname !== 'pastebox.micutu.com' || navigator.doNotTrack === '1' || navigator.globalPrivacyControl) return;
  try {
    if (!tracker) tracker = new Promise((resolve,reject) => {
      const script = document.createElement('script');
      script.src = 'https://analytics.micutu.com/script.js';
      script.dataset.websiteId = WEBSITE; script.dataset.autoTrack = 'false';
      script.dataset.domains = 'pastebox.micutu.com'; script.dataset.doNotTrack = 'true';
      script.onload = resolve; script.onerror = reject; document.head.append(script);
    });
    await tracker;
    if (publicRoute() !== route) return;
    await window.umami?.track({website:WEBSITE,hostname:'pastebox.micutu.com',url:route,title:route === '/' ? 'PasteBox' : 'Explore · PasteBox'});
  } catch { /* Analytics must not affect the application. */ }
}
