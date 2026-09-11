import { defineConfig, devices } from '@playwright/test';

export default defineConfig({
  testDir: './tests',
  timeout: 30000,
  expect: {
    timeout: 5000,
  },
  fullyParallel: false,
  workers: 1,
  retries: 0,
  forbidOnly: !!process.env.CI,
  reporter: process.env.CI
    ? [
        ['dot'],
        ['html', { outputFolder: 'playwright-report', open: 'never' }],
      ]
    : [
        ['list'],
        ['html', { outputFolder: 'playwright-report', open: 'never' }],
      ],
  use: {
    baseURL: 'http://127.0.0.1:3000',
    headless: false,
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
  },
  projects: [
    {
      name: 'chromium',
      use: {
        ...devices['Desktop Chrome'],
        launchOptions: {
          args: [
            '--no-sandbox',
            '--disable-setuid-sandbox',
            '--enable-features=UseOzonePlatform',
            '--ozone-platform=x11',
            '--gtk-version=3',
          ],
        },
      },
    },
    {
      name: 'firefox',
      use: {
        ...devices['Desktop Firefox'],
        launchOptions: {
          firefoxUserPrefs: {
            'focusmanager.testmode': false,
            'dom.input_events.dispatch_before_compositionend': true,
          },
          env: {
            ...process.env,
            MOZ_ENABLE_WAYLAND: '0',
            GTK_IM_MODULE: 'fcitx',
            QT_IM_MODULE: 'fcitx',
            XMODIFIERS: '@im=fcitx',
          },
        },
      },
    },
  ],
  webServer: {
    command: 'node fixtures/server.mjs',
    port: 3000,
    reuseExistingServer: !process.env.CI,
  },
});
