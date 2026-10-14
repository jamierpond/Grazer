// Build Release + install to /Applications/Grazer.app.
//
// The new bundle is copied next to the old one first, then swapped in by
// rename, so /Applications never holds a half-copied Grazer. A running installed
// Grazer is asked to quit first (and reopened after); dev builds are left alone.
// LaunchServices is pointed at the installed copy and told to forget the
// build*/Grazer.app bundles, so `open -b audio.pond.grazer` and apps that open URLs
// "with" Grazer (Rudder favourites) land on /Applications.
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import { rm, rename } from 'node:fs/promises';
import { join } from 'node:path';
import { RELEASE_BUILD_DIR, ROOT, cpmSourceArgs, main, run } from './lib.mjs';

const APP = '/Applications/Grazer.app';
const STAGING = '/Applications/.Grazer.app.installing';
const OLD = '/Applications/.Grazer.app.old';
const LSREGISTER =
  '/System/Library/Frameworks/CoreServices.framework/Frameworks/' +
  'LaunchServices.framework/Support/lsregister';

function bundleId(app) {
  return execFileSync('/usr/libexec/PlistBuddy',
    ['-c', 'Print :CFBundleIdentifier', join(app, 'Contents/Info.plist')],
    { encoding: 'utf8' }).trim();
}

// Gracefully quit (a quit Apple event, like Cmd+Q) whatever runs from APP,
// found by its bundle id. Returns whether one was running.
async function quitInstalled() {
  if (!existsSync(APP))
    return false;

  const script = `
    ObjC.import('AppKit');
    ObjC.unwrap($.NSRunningApplication
      .runningApplicationsWithBundleIdentifier(${JSON.stringify(bundleId(APP))}))
      .filter((app) => ObjC.unwrap(app.bundleURL.path) === ${JSON.stringify(APP)})
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
    throw new Error('Grazer did not quit');

  return pids.length > 0;
}

await main(async () => {
  if (!existsSync(join(RELEASE_BUILD_DIR, 'build.ninja')))
    await run('cmake', [
      '-G', 'Ninja', '-B', RELEASE_BUILD_DIR, '-DCMAKE_BUILD_TYPE=Release',
      ...cpmSourceArgs(),
    ]);

  await run('cmake', ['--build', RELEASE_BUILD_DIR, '--target', 'Grazer']);

  await rm(STAGING, { recursive: true, force: true });
  await run('ditto', [join(RELEASE_BUILD_DIR, 'Grazer.app'), STAGING]);

  // The linker's ad-hoc signature covers the executable only; seal the whole
  // bundle (Info.plist, icon) so it verifies and carries the bundle id. Still
  // ad-hoc: no identity, no keychain.
  await run('codesign', ['--force', '--sign', '-', STAGING]);
  await run('codesign', ['--verify', '--deep', '--strict', STAGING]);

  const wasRunning = await quitInstalled();

  await rm(OLD, { recursive: true, force: true });

  if (existsSync(APP))
    await rename(APP, OLD);

  await rename(STAGING, APP);
  await rm(OLD, { recursive: true, force: true });

  await run(LSREGISTER, ['-f', APP]);

  for (const dir of readdirSync(ROOT).filter((name) => name.startsWith('build'))) {
    const dev = join(ROOT, dir, 'Grazer.app');

    // Fails (-10814) for a bundle LaunchServices never saw; that's fine.
    if (existsSync(dev))
      await run(LSREGISTER, ['-u', dev], { stdio: 'ignore' }).catch(() => {});
  }

  if (wasRunning)
    await run('open', [APP]);

  console.log(`Installed ${APP} (${bundleId(APP)})`);
});
