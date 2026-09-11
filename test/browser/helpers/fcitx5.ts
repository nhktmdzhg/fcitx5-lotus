import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { setTimeout } from 'node:timers/promises';

const execFileAsync = promisify(execFile);

/**
 * Returns the currently active input method name (e.g. 'lotus', 'keyboard-us').
 */
export async function getActiveIM(): Promise<string> {
  const { stdout } = await execFileAsync('fcitx5-remote', ['-n']);
  return stdout.trim();
}

/**
 * Switches the active input method to the given name (e.g. 'lotus' or 'keyboard-us').
 */
export async function switchIM(name: string): Promise<void> {
  await execFileAsync('fcitx5-remote', ['-s', name]);
  if (name === 'lotus') {
    await execFileAsync('fcitx5-remote', ['-o']).catch(() => {});
  }
  // Small delay to allow fcitx5 to switch its active engine
  await setTimeout(100);
}
/**
 * Activates the input method engine (equivalent to fcitx5-remote -o).
 */
export async function activateIM(): Promise<void> {
  await execFileAsync('fcitx5-remote', ['-o']);
  await setTimeout(100);
}

/**
 * Inactivates the input method engine (equivalent to fcitx5-remote -c).
 */
export async function inactivateIM(): Promise<void> {
  await execFileAsync('fcitx5-remote', ['-c']);
  await setTimeout(100);
}

/**
 * Checks if Fcitx5 is currently running and responsive.
 * `fcitx5-remote` returns 1 (inactive) or 2 (active) when running, or 0 / error when not.
 */
export async function isFcitxRunning(): Promise<boolean> {
  try {
    const { stdout } = await execFileAsync('fcitx5-remote', []);
    const code = parseInt(stdout.trim(), 10);
    return code === 1 || code === 2;
  } catch {
    return false;
  }
}
