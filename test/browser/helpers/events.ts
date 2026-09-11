import type { Page, TestInfo } from '@playwright/test';

export interface RecordedInputEvent {
  type: string;
  key?: string | null;
  code?: string | null;
  data?: string | null;
  inputType?: string | null;
  isComposing?: boolean;
  targetId?: string;
  selectionStart?: number | null;
  selectionEnd?: number | null;
  domValue?: string | null;
  activeElementId?: string | null;
  timestamp: number;
}

declare global {
  interface Window {
    __inputEvents?: RecordedInputEvent[];
    __resetEvents?: () => void;
  }
}

/**
 * Retrieves the list of recorded input, key, and composition events from the page.
 */
export async function getEventLog(page: Page): Promise<RecordedInputEvent[]> {
  return await page.evaluate(() => window.__inputEvents || []);
}

/**
 * Resets the recorded input events array on the page.
 */
export async function resetEventLog(page: Page): Promise<void> {
  await page.evaluate(() => {
    if (typeof window.__resetEvents === 'function') {
      window.__resetEvents();
    }
  });
}
/**
 * Attaches recorded events as a JSON diagnostic artifact to Playwright's TestInfo.
 */
export async function attachEventLog(
  page: Page,
  testInfo: TestInfo
): Promise<void> {
  const events = await getEventLog(page).catch(() => []);
  if (events.length > 0) {
    await testInfo.attach('input-events.json', {
      body: JSON.stringify(events, null, 2),
      contentType: 'application/json',
    });
  }
}
