// Build Release + install to /Applications/Wim.app.
//
// The new bundle is copied next to the old one first, then swapped in by
// rename, so /Applications never holds a half-copied Wim. A running installed
// Wim is asked to quit first (and reopened after); dev builds are left alone.
// LaunchServices is pointed at the installed copy and told to forget the
// build*/Wim.app bundles, so `open -b audio.pond.wim` and apps that open URLs
// "with" Wim (Rudder favourites) land on /Applications.
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import { rm, rename } from 'node:fs/promises';
import { join } from 'node:path';
import { RELEASE_BUILD_DIR, ROOT, cpmSourceArgs, main, run } from './lib.mjs';

const APP = '/Applications/Wim.app';
const STAGING = '/Applications/.Wim.app.installing';
const OLD = '/Applications/.Wim.app.old';
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
function quitInstalled() {
  if (!existsSync(APP))
    return false;

  const script = `
    ObjC.import('AppKit');
    const apps = ObjC.unwrap($.NSRunningApplication
      .runningApplicationsWithBundleIdentifier(${JSON.stringify(bundleId(APP))}))
      .filter((app) => ObjC.unwrap(app.bundleURL.path) === ${JSON.stringify(APP)});
    apps.forEach((app) => app.terminate);
    for (let i = 0; i < 100 && apps.some((app) => !app.terminated); ++i)
      delay(0.1);
    if (apps.some((app) => !app.terminated))
      throw new Error('Wim did not quit');
    apps.length > 0;
  `;

  return execFileSync('osascript', ['-l', 'JavaScript', '-e', script],
    { encoding: 'utf8' }).trim() === 'true';
}

await main(async () => {
  if (!existsSync(join(RELEASE_BUILD_DIR, 'build.ninja')))
    await run('cmake', [
      '-G', 'Ninja', '-B', RELEASE_BUILD_DIR, '-DCMAKE_BUILD_TYPE=Release',
      ...cpmSourceArgs(),
    ]);

  await run('cmake', ['--build', RELEASE_BUILD_DIR, '--target', 'Wim']);

  await rm(STAGING, { recursive: true, force: true });
  await run('ditto', [join(RELEASE_BUILD_DIR, 'Wim.app'), STAGING]);

  // The linker's ad-hoc signature covers the executable only; seal the whole
  // bundle (Info.plist, icon) so it verifies and carries the bundle id. Still
  // ad-hoc: no identity, no keychain.
  await run('codesign', ['--force', '--sign', '-', STAGING]);
  await run('codesign', ['--verify', '--deep', '--strict', STAGING]);

  const wasRunning = quitInstalled();

  await rm(OLD, { recursive: true, force: true });

  if (existsSync(APP))
    await rename(APP, OLD);

  await rename(STAGING, APP);
  await rm(OLD, { recursive: true, force: true });

  await run(LSREGISTER, ['-f', APP]);

  for (const dir of readdirSync(ROOT).filter((name) => name.startsWith('build'))) {
    const dev = join(ROOT, dir, 'Wim.app');

    if (existsSync(dev))
      await run(LSREGISTER, ['-u', dev]);
  }

  if (wasRunning)
    await run('open', [APP]);

  console.log(`Installed ${APP} (${bundleId(APP)})`);
});
