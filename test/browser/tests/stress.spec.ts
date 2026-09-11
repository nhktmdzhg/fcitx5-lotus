import { test, expect } from '@playwright/test';
import { clearInput, typeWithLotus, typeXdotool } from '../helpers/x11-input';
import { getActiveIM, switchIM, activateIM } from '../helpers/fcitx5';
import { attachEventLog } from '../helpers/events';

test.describe('Fcitx5 Lotus Stress Tests', () => {
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

  test('rapid typing with low key delay does not drop characters', async ({
    page,
  }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);

    // Rapid typing at 20ms delay between key events
    await typeWithLotus(
      page,
      input,
      ['v', 'i', 'e', 'e', 't', 'j', 'space', 'n', 'a', 'm'],
      20
    );
    await expect(input).toHaveValue('việt nam');
  });

  test('rapid backspace deletion followed by new composition', async ({
    page,
  }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);

    // Rapid sequence: 'd' + 'd' -> 'đ', BackSpace -> deletes 'đ', 'd' + 'd' -> 'đ'
    await typeWithLotus(page, input, ['d', 'd', 'BackSpace', 'd', 'd'], 40);
    await expect(input).toHaveValue('đ');
  });

  // Note: Current MVP runs Lotus in default Preedit mode. Word editing tests
  // verify engine state transitions on committed text followed by new composition.
  test('edits committed text with backspace and tone modification', async ({
    page,
  }) => {
    const input = page.locator('#test-input');
    await clearInput(page, input);

    await typeWithLotus(page, input, [
      't', 'o', 'o', 'i',
      'space',
      'l', 'a', 'f',
    ]);
    await expect(input).toHaveValue('tôi là');

    // Backspace once to delete 'à' (leaving "tôi l"), then retype with acute tone
    await typeXdotool('BackSpace', 50);
    await typeXdotool(['a', 's'], 50);
    await expect(input).toHaveValue('tôi lá');
  });

  // Note: Historical #215 fixed a Gecko async surrounding-text race in SurroundingText mode.
  // In this Preedit MVP, this test stresses repeated rapid Telex composition across cycles
  // to ensure browser event dispatch and Fcitx preedit do not drop or scramble characters.
  test('repeated rapid composition cycles remain stable without dropping keys', async ({
    page,
  }) => {
    const input = page.locator('#test-input');

    // Repeat typing "nhieeuf" -> "nhiều" across multiple cycles
    for (let cycle = 0; cycle < 5; cycle++) {
      await clearInput(page, input);
      await typeWithLotus(
        page,
        input,
        ['n', 'h', 'i', 'e', 'e', 'u', 'f'],
        25
      );
      await expect(input).toHaveValue('nhiều');
    }

    // Verify multi-word composition in a single session without clearing
    await clearInput(page, input);
    await typeWithLotus(
      page,
      input,
      [
        'n', 'h', 'i', 'e', 'e', 'u', 'f',
        'space',
        'n', 'h', 'i', 'e', 'e', 'u', 'f',
      ],
      25
    );
    await expect(input).toHaveValue('nhiều nhiều');
  });
});
