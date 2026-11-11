import { spawn } from 'node:child_process';
import { accessSync, constants, statSync } from 'node:fs';
import { delimiter, dirname, join } from 'node:path';
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

const isWindows = process.platform === 'win32';

// The absolute path of `command` on PATH, or null.
function which(command) {
  const extensions = isWindows
    ? (process.env.PATHEXT || '.EXE;.CMD;.BAT').split(';')
    : [''];

  for (const directory of (process.env.PATH || '').split(delimiter)) {
    for (const extension of extensions) {
      const candidate = join(directory, command + extension);

      try {
        accessSync(candidate, constants.X_OK);

        if (statSync(candidate).isFile())
          return candidate;
      } catch {
        // Not in this directory.
      }
    }
  }

  return null;
}

// On Windows a .cmd/.bat shim (npm) can't be spawned directly (Node rejects it,
// CVE-2024-27980), so it runs through cmd.exe as one command string: Node
// deprecates passing args alongside shell: true (DEP0190). The bare name goes
// to the shell, which resolves it through PATH itself. Everything else, here
// and on macOS, is spawned directly with no shell.
function describeSpawn(command, args) {
  const resolved = isWindows ? (which(command) ?? command) : command;

  if (!isWindows || !/\.(cmd|bat)$/i.test(resolved))
    return { target: resolved, args, shell: false };

  const quotedArgs = args.map((arg) => `"${String(arg).replace(/"/g, '\\"')}"`);
  return { target: [command, ...quotedArgs].join(' '), args: [], shell: true };
}

// Run a command with inherited stdio. Rejects when it can't start or exits
// non-zero, unless `allowFail`.
export function run(command, args = [], { allowFail = false, ...options } = {}) {
  return new Promise((resolve, reject) => {
    const fail = (error) => (allowFail ? resolve() : reject(error));
    const spawnPlan = describeSpawn(command, args);
    const child = spawn(spawnPlan.target, spawnPlan.args, {
      cwd: ROOT,
      stdio: 'inherit',
      shell: spawnPlan.shell,
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

// On a terminal Ninja overwrites one status line; TERM=dumb prints every step on its own line, as in CI.
const LINE_PER_STEP_ENVIRONMENT = { TERM: 'dumb', CLICOLOR_FORCE: '1' };

export function buildTarget(buildDirectory, target) {
  return run('cmake', ['--build', buildDirectory, '--target', target], {
    env: { ...process.env, ...LINE_PER_STEP_ENVIRONMENT },
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
