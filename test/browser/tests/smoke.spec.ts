import { test, expect } from '@playwright/test';
import { clearInput, ensureActive, typeWithLotus, typeXdotool } from '../helpers/x11-input';
import { getActiveIM, switchIM, activateIM } from '../helpers/fcitx5';
import { attachEventLog } from '../helpers/events';

test.describe('Fcitx5 Lotus Smoke Tests', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await activateIM();
    await switchIM('lotus');
  });

  test.afterEach(async ({ page }, testInfo) => {
    if (testInfo.status !== testInfo.expectedStatus) {
      await attachEventLog(page, testInfo);
    }
  });

  test('types telex single character on text input', async ({ page }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);
    await typeWithLotus(page, input, ['d', 'd']);
    await expect(input).toHaveValue('đ');
  });

  test('types telex acute accent tone on text input', async ({ page }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);
    await typeWithLotus(page, input, ['a', 's']);
    await expect(input).toHaveValue('á');
  });

  test('types multi-word phrase with telex tones', async ({ page }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);
    await typeWithLotus(page, input, [
      't', 'i', 'e', 'e', 'n', 'g', 's',
      'space',
      'v', 'i', 'e', 'e', 't', 'j',
    ]);
    await expect(input).toHaveValue('tiếng việt');
  });

  test('types telex phrase in textarea', async ({ page }) => {
    const textarea = page.locator('#test-textarea');
    await clearInput(page, textarea);
    await typeWithLotus(page, textarea, [
      'x', 'i', 'n',
      'space',
      'c', 'h', 'a', 'o', 'f',
    ]);
    await expect(textarea).toHaveValue('xin chào');
  });

  test('types telex phrase in contenteditable element', async ({ page }) => {
    const contenteditable = page.locator('#test-contenteditable');
    await clearInput(page, contenteditable);
    await typeWithLotus(page, contenteditable, ['v', 'i', 'e', 'e', 't', 'j']);
    await expect(contenteditable).toHaveText('việt');
  });

  test('preserves committed text and resumes typing across blur and refocus', async ({
    page,
  }) => {
    const input1 = page.locator('#test-input');
    const input2 = page.locator('#test-input-2');

    // Type first word
    await clearInput(page, input1);
    await typeWithLotus(page, input1, ['t', 'i', 'e', 'e', 'n', 'g', 's']);
    await expect(input1).toHaveValue('tiếng');

    // Blur by focusing second input
    await ensureActive(page, input2);
    await expect(input2).toBeFocused();

    // Refocus first input
    await ensureActive(page, input1);
    await expect(input1).toBeFocused();
    // Move caret to end to ensure typing appends cleanly
    await typeXdotool('End', 50);

    // Type remaining phrase
    await typeXdotool(['space', 'v', 'i', 'e', 'e', 't', 'j']);
    await expect(input1).toHaveValue('tiếng việt');
  });
});
