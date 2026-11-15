import { test, expect } from '@playwright/test';

test.beforeEach(async ({ page }) => {
  await page.route('https://**/*',route => route.abort());
});

async function create(page, {title = 'Browser lifecycle',content = 'const answer = 42;\nconsole.log(answer);',visibility = 'public',password = '',burn = false,language = 'javascript'} = {}) {
  await page.goto('/');
  await page.getByRole('textbox',{name:'Paste title',exact:true}).fill(title);
  await page.getByRole('textbox',{name:'Paste content',exact:true}).fill(content);
  await page.getByRole('combobox',{name:'Language',exact:true}).selectOption(language);
  await page.getByRole('combobox',{name:'Paste visibility',exact:true}).selectOption(visibility);
  if (password) await page.getByLabel('Password protection',{exact:true}).fill(password);
  if (burn) await page.getByRole('checkbox',{name:'Burn after read'}).check();
  await page.getByRole('button',{name:'Create paste',exact:true}).click();
  await expect(page).toHaveURL(/#\/paste\/[a-f0-9]{16}$/);
  return page.url();
}

test('create, highlight, edit, favorite, explore, restore and delete', async ({ page }) => {
  const runtimeErrors = [];
  page.on('pageerror',error => runtimeErrors.push(error.message));
  await create(page);
  const code = page.getByRole('textbox',{name:'Code content',exact:true});
  await expect(code).toContainText('const answer = 42;');
  await expect(code.locator('.hljs-keyword').first()).toBeVisible();
  await page.getByRole('button',{name:'Toggle word wrap'}).click();
  await expect(page.locator('.code-wrap')).toBeVisible();
  await page.getByRole('button',{name:'Save to favorites',exact:true}).click();
  await page.getByRole('button',{name:'Edit paste',exact:true}).click();
  await page.getByRole('textbox',{name:'Title',exact:true}).fill('Edited browser lifecycle');
  await page.getByRole('textbox',{name:'Content',exact:true}).fill('console.log("updated");');
  await page.getByRole('button',{name:'Save Changes',exact:true}).click();
  await expect(page.getByRole('heading',{name:'Edited browser lifecycle',exact:true})).toBeVisible();
  await expect(code).toContainText('console.log("updated");');
  await page.getByRole('link',{name:'Library',exact:true}).click();
  await expect(page.getByRole('button',{name:'Edited browser lifecycle',exact:true})).toBeVisible();
  await page.getByRole('link',{name:'Explore',exact:true}).click();
  await page.getByRole('searchbox',{name:'Search public pastes'}).fill('Edited browser lifecycle');
  await page.getByRole('button',{name:'Search',exact:true}).click();
  await page.getByRole('button',{name:'View paste: Edited browser lifecycle',exact:true}).first().click();
  await page.getByRole('button',{name:'Delete paste',exact:true}).click();
  await expect(page.getByRole('dialog')).toBeVisible();
  await page.getByRole('button',{name:'Permanently delete',exact:true}).click();
  await expect(page).toHaveURL(/#\/library$/);
  expect(runtimeErrors).toEqual([]);
});

test('private paste cannot be read from a fresh browser context', async ({page,browser}) => {
  const url = await create(page,{title:'Browser private paste',visibility:'private'});
  await expect(page.getByRole('textbox',{name:'Code content'})).toBeVisible();
  const visitor = await browser.newContext();
  const stranger = await visitor.newPage();
  await stranger.route('https://**/*',route => route.abort());
  await stranger.goto(url);
  await expect(stranger.getByRole('heading',{name:'Just for the owner.'})).toBeVisible();
  expect(await stranger.getByRole('textbox',{name:'Code content'}).count()).toBe(0);
  const key = await page.evaluate(() => JSON.parse(localStorage.getItem('pastebox-library-v1')).find(p => p.title === 'Browser private paste').owner_token);
  await stranger.getByLabel('Owner key',{exact:true}).fill(key);
  await stranger.getByRole('button',{name:'Restore access',exact:true}).click();
  await expect(stranger.getByRole('textbox',{name:'Code content'})).toBeVisible();
  await visitor.close();
});

test('one-time paste waits for reveal, shows its code, and disappears on reload', async ({page}) => {
  await create(page,{title:'Browser one-time paste',burn:true,content:'only once'});
  await expect(page.getByRole('heading',{name:'A fleeting little secret.'})).toBeVisible();
  await page.getByRole('button',{name:/Reveal one-time paste/}).click();
  await expect(page.getByRole('textbox',{name:'Code content'})).toHaveText('only once');
  await expect(page.getByRole('alert').first()).toContainText('server copy is gone');
  await page.reload();
  await expect(page.getByRole('heading',{name:'Nothing here, for now.'})).toBeVisible();
});

test('text file import, JSON format, draft privacy, theme and mobile fit', async ({page}) => {
  await page.goto('/');
  await page.getByLabel('Import code file',{exact:true}).setInputFiles({name:'settings.json',mimeType:'application/json',buffer:Buffer.from('{"hello":"world"}')});
  await expect(page.getByRole('textbox',{name:'Paste content',exact:true})).toHaveValue('{"hello":"world"}');
  await page.getByRole('button',{name:/Format JSON/}).click();
  await expect(page.getByRole('textbox',{name:'Paste content',exact:true})).toHaveValue('{\n  "hello": "world"\n}');
  await page.getByRole('checkbox',{name:'Save public draft on this device'}).check();
  await expect.poll(() => page.evaluate(() => localStorage.getItem('pastebox-public-draft-v1'))).not.toBeNull();
  await page.getByLabel('Password protection',{exact:true}).fill('protected');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('pastebox-public-draft-v1'))).toBeNull();
  await page.getByRole('combobox',{name:'Color theme',exact:true}).selectOption('light');
  await expect(page.locator('html')).toHaveAttribute('data-theme','light');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.setViewportSize({width:320,height:740});
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test('password readers can unlock and download without receiving edit access', async ({page,browser}) => {
  const url = await create(page,{title:'Read-only password test',password:'a good password'});
  const visitor = await browser.newContext();
  const reader = await visitor.newPage();
  await reader.route('https://**/*',route => route.abort());
  await reader.goto(url);
  await reader.getByRole('textbox',{name:'Code content'}).waitFor({state:'hidden'});
  await reader.getByLabel('Enter password to unlock paste',{exact:true}).fill('a good password');
  await reader.getByRole('button',{name:'Unlock paste',exact:true}).click();
  await expect(reader.getByRole('textbox',{name:'Code content'})).toBeVisible();
  await expect(reader.getByRole('button',{name:'Edit paste',exact:true})).toHaveCount(0);
  const [download] = await Promise.all([reader.waitForEvent('download'),reader.getByRole('button',{name:'Download paste as file'}).click()]);
  expect(download.suggestedFilename()).toBe('Read-only password test.js');
  await visitor.close();
});

test('owners compare, restore and clear history without exposing it to readers', async ({page}) => {
  await create(page,{title:'Revision browser flow',content:'const value = 1;'});
  await page.getByRole('button',{name:'Edit paste',exact:true}).click();
  await page.getByRole('textbox',{name:'Content',exact:true}).fill('const value = 2;');
  await page.getByRole('button',{name:'Save Changes',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Code content'})).toHaveText('const value = 2;');
  await page.getByRole('button',{name:'Version history',exact:true}).click();
  await page.getByRole('button',{name:'View version 1',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Line changes'})).toContainText('const value = 1;');
  await expect(page.getByRole('textbox',{name:'Line changes'})).toContainText('const value = 2;');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.getByRole('button',{name:'Restore v1',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Code content'})).toHaveText('const value = 1;');
  await page.getByRole('button',{name:'Version history',exact:true}).click();
  await expect(page.getByText('Current v3',{exact:true})).toBeVisible();
  await page.getByRole('button',{name:'Clear saved history',exact:true}).click();
  await page.getByRole('button',{name:'Clear history permanently',exact:true}).click();
  await expect(page.getByText(/No earlier versions yet/)).toBeVisible();
});

test('a stale browser tab preserves its text when saving is rejected', async ({page,context}) => {
  const url = await create(page,{title:'Two tab flow'});
  const other = await context.newPage();
  await other.route('https://**/*',route => route.abort());
  await other.goto(url);
  await other.getByRole('button',{name:'Edit paste',exact:true}).click();
  await other.getByRole('textbox',{name:'Content',exact:true}).fill('my unsaved second-tab work');
  await page.getByRole('button',{name:'Edit paste',exact:true}).click();
  await page.getByRole('textbox',{name:'Content',exact:true}).fill('first tab saved this');
  await page.getByRole('button',{name:'Save Changes',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Code content'})).toHaveText('first tab saved this');
  await other.getByRole('button',{name:'Save Changes',exact:true}).click();
  await expect(other.getByText(/Another tab saved a newer version/)).toBeVisible();
  await expect(other.getByRole('textbox',{name:'Content',exact:true})).toHaveValue('my unsaved second-tab work');
  await page.reload();
  await expect(page.getByRole('textbox',{name:'Code content'})).toHaveText('first tab saved this');
  await other.close();
});

test('Markdown drafts preview without publishing and load the parser on demand', async ({page}) => {
  const parserRequests = [];
  page.on('request',request => {if (/markdown[.-](worker|sanitize)/.test(request.url())) parserRequests.push(request.url());});
  await page.goto('/');
  await page.getByRole('combobox',{name:'Language',exact:true}).selectOption('markdown');
  await page.getByRole('textbox',{name:'Paste content',exact:true}).fill('# Draft preview\n\n- [x] first task\n- [ ] next task');
  expect(parserRequests).toEqual([]);
  await page.getByRole('button',{name:'Preview Markdown draft',exact:true}).click();
  const preview = page.frameLocator('iframe[title="Markdown preview"]');
  await expect(preview.getByRole('heading',{name:'Draft preview',exact:true})).toBeVisible();
  await expect(preview.locator('body')).toContainText('☑');
  expect(parserRequests.length).toBeGreaterThan(0);
  await page.setViewportSize({width:320,height:812});
  const sourceButton = page.getByRole('button',{name:'View Markdown draft source',exact:true});
  const sourceBox = await sourceButton.boundingBox();
  const panelBox = await page.locator('.editor-panel').boundingBox();
  expect(sourceBox.x+sourceBox.width).toBeLessThanOrEqual(panelBox.x+panelBox.width);
  await expect(page).not.toHaveURL(/#\/paste\//);
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.getByRole('button',{name:'View Markdown draft source',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Paste content',exact:true})).toHaveValue('# Draft preview\n\n- [x] first task\n- [ ] next task');
});

test('Markdown preview blocks scripts, parent access and remote image requests', async ({page}) => {
  const remoteImages = [];
  page.on('request',request => {if (request.url().includes('tracker.invalid')) remoteImages.push(request.url());});
  const content = '# Preview safety\n\n<script>parent.__markdownExecuted = true</script>\n\n<img src="https://tracker.invalid/html" onerror="parent.__markdownExecuted = true">\n\n![hidden image](https://tracker.invalid/markdown)\n\n[unsafe](javascript:alert(1))\n\n[good link](https://example.com/docs)';
  await create(page,{title:'Markdown isolation',content,visibility:'private',language:'markdown'});
  await page.getByRole('button',{name:'Preview Markdown',exact:true}).click();
  const preview = page.frameLocator('iframe[title="Markdown preview"]');
  await expect(preview.getByRole('heading',{name:'Preview safety',exact:true})).toBeVisible();
  await expect(preview.locator('script,img,svg,math,form,iframe')).toHaveCount(0);
  await expect(preview.locator('a[href^="javascript:"]')).toHaveCount(0);
  await expect(preview.getByRole('link',{name:'good link',exact:true})).toHaveAttribute('rel','noopener noreferrer');
  await expect(preview.getByRole('link',{name:'good link',exact:true})).toHaveAttribute('target','_blank');
  expect(await page.evaluate(() => window.__markdownExecuted)).toBeUndefined();
  expect(await preview.locator('body').evaluate(() => {try {window.parent.localStorage.getItem('pastebox-library-v1');return false;} catch {return true;}})).toBe(true);
  expect(remoteImages).toEqual([]);
  await page.getByRole('combobox',{name:'Color theme',exact:true}).selectOption('light');
  await expect(preview.locator('body')).toHaveCSS('background-color','rgb(246, 248, 250)');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.getByRole('button',{name:'View Markdown source',exact:true}).click();
  await expect(page.getByRole('textbox',{name:'Code content'})).toContainText('<script>parent.__markdownExecuted = true</script>');
});
