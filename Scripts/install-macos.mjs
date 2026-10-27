// Build Release + install to ~/.local/libexec/grazer/Grazer.app, with a
// ~/.local/bin/grazer launcher script that opens it through LaunchServices.
//
// The new bundle is copied next to the old one first, then swapped in by
// rename, so the install dir never holds a half-copied Grazer. A running installed
// Grazer is asked to quit first (and reopened after); dev builds are left alone.
// LaunchServices is pointed at the installed copy and told to forget every
// other Grazer bundle (dev builds in any checkout), so `open -b
// audio.pond.grazer` and apps that open URLs "with" Grazer (Rudder favourites) land on the ~/.local copy.
//
// Grazer used to install to /Applications/Grazer.app; that copy is quit,
// unregistered and removed.
//
// Grazer was called Wim (audio.pond.wim, before that com.pond.wim). The old
// /Applications/Wim.app is quit and removed, and every bundle LaunchServices
// knows under those ids is unregistered, so nothing resolves to Wim any more.
// Wim's data (~/Library/Application Support/pond/Wim, WebKit audio.pond.wim)
// is left alone.
//
// The Release tree is always reconfigured, so CPM refetches branch-pinned
// packages (Rudder@main) and the install has the latest Rudder client.
import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, rm, rename, writeFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { join } from 'node:path';
import { RELEASE_BUILD_DIR, cpmSourceArgs, main, run } from './lib.mjs';

const LOCAL_PREFIX = join(homedir(), '.local');
const INSTALL_DIR = join(LOCAL_PREFIX, 'libexec', 'grazer');
const COMMAND_DIR = join(LOCAL_PREFIX, 'bin');
const APP = join(INSTALL_DIR, 'Grazer.app');
const STAGING = join(INSTALL_DIR, '.Grazer.app.installing');
const OLD = join(INSTALL_DIR, '.Grazer.app.old');
const COMMAND_LAUNCHER = join(COMMAND_DIR, 'grazer');
const PREVIOUS_LOCATION_APP = '/Applications/Grazer.app';
const OLD_NAME_APP = '/Applications/Wim.app';
const OLD_BUNDLE_IDS = ['audio.pond.wim', 'com.pond.wim'];
const LSREGISTER =
  '/System/Library/Frameworks/CoreServices.framework/Frameworks/' +
  'LaunchServices.framework/Support/lsregister';

function bundleId(app) {
  return execFileSync('/usr/libexec/PlistBuddy',
    ['-c', 'Print :CFBundleIdentifier', join(app, 'Contents/Info.plist')],
    { encoding: 'utf8' }).trim();
}

const COMMAND_LAUNCHER_FILE_MODE = 0o755;

// A plain launch goes through LaunchServices, not the bundle's executable, so
// macOS makes the app (not the terminal) the responsible process for TCC
// prompts and grants. Any argument execs the executable directly instead:
// control flags (--open-url, --list-open-tabs, --focus-tab) and --snapshot are
// short-lived clients that print to this terminal and need no TCC grant.
function describeCommandLauncherScript(appBundlePath) {
  const appExecutablePath = join(appBundlePath, 'Contents', 'MacOS', 'Grazer');
  return [
    '#!/bin/sh',
    `[ "$#" -gt 0 ] && exec ${JSON.stringify(appExecutablePath)} "$@"`,
    `exec open -a ${JSON.stringify(appBundlePath)}`,
    '',
  ].join('\n');
}

async function writeCommandLauncher(launcherPath, appBundlePath) {
  await rm(launcherPath, { force: true });
  await writeFile(launcherPath, describeCommandLauncherScript(appBundlePath),
    { mode: COMMAND_LAUNCHER_FILE_MODE });
}

// Gracefully quit (a quit Apple event, like Cmd+Q) whatever runs from `app`,
// found by its bundle id. Returns whether one was running.
async function quitInstalled(app = APP) {
  if (!existsSync(app))
    return false;

  const script = `
    ObjC.import('AppKit');
    ObjC.unwrap($.NSRunningApplication
      .runningApplicationsWithBundleIdentifier(${JSON.stringify(bundleId(app))}))
      .filter((app) => ObjC.unwrap(app.bundleURL.path) === ${JSON.stringify(app)})
      .map((app) => { app.terminate; return app.processIdentifier; })
      .join(' ');
  `;

  const pids = execFileSync('osascript', ['-l', 'JavaScript', '-e', script],
    { encoding: 'utf8' }).trim().split(' ').filter(Boolean).map(Number);

  // osascript has no run loop to see NSRunningApplication.terminated flip,
  // so wait on the pids here.
  const alive = (pid) => {
    try {
      process.kill(pid, 0);
      return true;
    } catch {
      return false;
    }
  };

  for (let i = 0; i < 100 && pids.some(alive); ++i)
    await new Promise((resolve) => setTimeout(resolve, 100));

  if (pids.some(alive))
    throw new Error(`${app} did not quit`);

  return pids.length > 0;
}

// Every bundle path LaunchServices has registered under one of `ids`.
function registeredPaths(ids) {
  const dump = execFileSync(LSREGISTER, ['-dump'],
    { encoding: 'utf8', maxBuffer: 512 * 1024 * 1024 });
  const paths = new Set();

  for (const record of dump.split(/^-{20,}$/m)) {
    const id = record.match(/^identifier:\s+(\S+)/m)?.[1];
    const path = record.match(/^path:\s+(.+?)(?: \(0x[0-9a-f]+\))?$/m)?.[1];

    if (id && path && ids.includes(id))
      paths.add(path);
  }

  return [...paths];
}

async function quitUnregisterAndRemoveBundle(app) {
  if (!existsSync(app))
    return;

  await quitInstalled(app);
  await run(LSREGISTER, ['-u', app], { stdio: 'ignore' }).catch(() => {});
  await rm(app, { recursive: true, force: true });
  console.log(`Removed ${app}`);
}

// Quit and remove /Applications/Wim.app, then unregister every Wim bundle.
async function removeOldName() {
  await quitUnregisterAndRemoveBundle(OLD_NAME_APP);

  for (const path of registeredPaths(OLD_BUNDLE_IDS))
    await run(LSREGISTER, ['-u', path], { stdio: 'ignore' }).catch(() => {});
}

await main(async () => {
  await run('cmake', [
    '-G', 'Ninja', '-B', RELEASE_BUILD_DIR, '-DCMAKE_BUILD_TYPE=Release',
    ...cpmSourceArgs(),
  ]);

  await run('cmake', ['--build', RELEASE_BUILD_DIR, '--target', 'Grazer']);

  await mkdir(INSTALL_DIR, { recursive: true });
  await mkdir(COMMAND_DIR, { recursive: true });
  await rm(STAGING, { recursive: true, force: true });
  await run('ditto', [join(RELEASE_BUILD_DIR, 'Grazer.app'), STAGING]);

  // The linker's ad-hoc signature covers the executable only; seal the whole
  // bundle (Info.plist, icon) so it verifies and carries the bundle id. Still
  // ad-hoc: no identity, no keychain.
  await run('codesign', ['--force', '--sign', '-', STAGING]);
  await run('codesign', ['--verify', '--deep', '--strict', STAGING]);

  await removeOldName();

  const previousLocationWasRunning = existsSync(PREVIOUS_LOCATION_APP)
    && await quitInstalled(PREVIOUS_LOCATION_APP);
  await quitUnregisterAndRemoveBundle(PREVIOUS_LOCATION_APP);

  const wasRunning = await quitInstalled() || previousLocationWasRunning;

  await rm(OLD, { recursive: true, force: true });

  if (existsSync(APP))
    await rename(APP, OLD);

  await rename(STAGING, APP);
  await rm(OLD, { recursive: true, force: true });

  await writeCommandLauncher(COMMAND_LAUNCHER, APP);

  await run(LSREGISTER, ['-f', APP]);

  const otherRegisteredGrazerBundles = registeredPaths([bundleId(APP)])
    .filter((path) => path !== APP);

  for (const path of otherRegisteredGrazerBundles)
    await run(LSREGISTER, ['-u', path], { stdio: 'ignore' }).catch(() => {});

  if (wasRunning)
    await run('open', [APP]);

  console.log(`Installed ${APP} (${bundleId(APP)})`);
});
