// Build + run the unit tests in build-test/, never build/: rebuilding the app
// bundle in place under a running Grazer is what `just run` is for. Only the
// test target builds.
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, cpmSourceArgs, main, run } from './lib.mjs';

const TEST_DIR = join(ROOT, 'build-test');
const windows = process.platform === 'win32';

await main(async () => {
  if (!existsSync(join(TEST_DIR, 'CMakeCache.txt')))
    await run('cmake', [
      ...(windows ? [] : ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug']),
      '-B', TEST_DIR, ...cpmSourceArgs(),
    ]);

  await run('cmake', ['--build', TEST_DIR, '--target', 'GrazerTests',
    ...(windows ? ['--config', 'Debug'] : [])]);

  await run(windows
    ? join(TEST_DIR, 'Tests', 'Debug', 'GrazerTests.exe')
    : join(TEST_DIR, 'Tests', 'GrazerTests'));
});
