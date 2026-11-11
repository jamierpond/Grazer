import { spawn } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
export const BUILD_DIR = join(ROOT, 'build');
export const RELEASE_BUILD_DIR = join(ROOT, 'build-release');

// lockstep source overrides from the environment, for a fresh configure: export
// e.g. LOCKSTEP_eacp_SOURCE=$HOME/projects/eacp to build against a local checkout.
export function lockstepSourceArgs() {
  return Object.entries(process.env)
    .filter(([name]) => /^LOCKSTEP_\w+_SOURCE$/.test(name))
    .map(([name, value]) => `-D${name}=${value}`);
}

const PLATFORMS = { darwin: 'macos', win32: 'windows' };

// Resolve `scripts/<name>-<platform>.mjs` for the current platform.
export function platformScript(name) {
  const platform = PLATFORMS[process.platform];
  if (!platform) throw new Error(`Unsupported platform: ${process.platform}`);
  return join(ROOT, 'scripts', `${name}-${platform}.mjs`);
}

// Run a command with inherited stdio. Rejects when it can't start or exits
// non-zero, unless `allowFail`.
export function run(command, args = [], { allowFail = false, ...options } = {}) {
  return new Promise((resolve, reject) => {
    const fail = (error) => (allowFail ? resolve() : reject(error));
    const child = spawn(command, args, {
      cwd: ROOT,
      stdio: 'inherit',
      shell: process.platform === 'win32',
      ...options,
    });
    child.on('exit', (code) =>
      code === 0
        ? resolve()
        : fail(new Error(`${command} ${args.join(' ')} exited with ${code}`)),
    );
    child.on('error', fail);
  });
}

export function runNode(script, args = []) {
  return run(process.execPath, [script, ...args]);
}

// Entry-point wrapper: run `fn`, exit non-zero on failure.
export async function main(fn) {
  try {
    await fn();
  } catch (error) {
    console.error(error.message);
    process.exit(1);
  }
}
