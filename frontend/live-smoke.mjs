import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import path from 'node:path';
import { chromium } from '@playwright/test';

const target = new URL(process.env.PASTEBOX_UI_ORIGIN || 'https://pastebox.micutu.com');
const production = target.protocol === 'https:' && target.hostname === 'pastebox.micutu.com' && !target.port;
const preview = target.protocol === 'http:' && target.hostname === '127.0.0.1' && target.port;
assert((production || preview) && !target.username && !target.password && target.pathname === '/' && !target.search && !target.hash,'Use the production origin or an explicit loopback preview origin.');
const browser = await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE ? {executablePath:process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE} : {})});
const context = await browser.newContext({viewport:{width:1440,height:1050},serviceWorkers:'block'});
const writes = [], runtimeErrors = [];
// Every write is blocked, including worker/popup traffic. Third-party requests
// are blocked so this verification does not produce analytics pageviews.
await context.route('**/*',route => {
  const request = route.request();
  if (!['GET','HEAD'].includes(request.method())) {
    if (new URL(request.url()).origin === target.origin) writes.push(request.method());
    return route.abort();
  }
  return new URL(request.url()).origin === target.origin ? route.continue() : route.abort();
});
const page = await context.newPage();
page.on('pageerror',error => runtimeErrors.push(error.message));
try {
  const response = await page.goto(target.origin);
  assert.equal(response.status(),200);
  await page.getByRole('heading',{name:/A good idea/}).waitFor();
  assert.equal(await page.getByRole('checkbox',{name:'Save public draft on this device'}).isChecked(),false);
  const source = '# Read-only release check\n\n- [x] Draft preview\n- [ ] No publication\n\n![hidden](https://tracker.invalid/pixel)';
  await page.getByRole('combobox',{name:'Language',exact:true}).selectOption('markdown');
  await page.getByRole('textbox',{name:'Paste content',exact:true}).fill(source);
  await page.getByRole('button',{name:'Preview Markdown draft',exact:true}).click();
  const frame = page.frameLocator('iframe[title="Markdown preview"]');
  await frame.getByRole('heading',{name:'Read-only release check',exact:true}).waitFor();
  assert.equal(await frame.locator('img,script,form').count(),0);
  for (const width of [375,320]) {
    await page.setViewportSize({width,height:812});
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth),`Layout overflow at ${width}px`);
  }
  if (process.env.PASTEBOX_UI_SCREENSHOT_DIR) {
    const directory = path.resolve(process.env.PASTEBOX_UI_SCREENSHOT_DIR);
    await mkdir(directory,{recursive:true,mode:0o700});
    await page.screenshot({path:path.join(directory,'markdown-mobile.png'),fullPage:true});
    await page.setViewportSize({width:1440,height:1050});
    await page.screenshot({path:path.join(directory,'markdown-desktop.png'),fullPage:true});
  }
  await page.getByRole('button',{name:'View Markdown draft source',exact:true}).click();
  assert.equal(await page.getByRole('textbox',{name:'Paste content',exact:true}).inputValue(),source);
  assert(!page.url().includes('#/paste/'));
  assert.deepEqual(writes,[],'Application attempted a write during a read-only smoke check.');
  assert.deepEqual(runtimeErrors,[]);
  console.log(JSON.stringify({origin:target.origin,status:'passed',markdownDraft:'passed',widths:[375,320],writes:0,runtimeErrors:0}));
} finally {await browser.close();}
