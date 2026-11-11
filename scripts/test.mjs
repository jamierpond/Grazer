// Build + run the unit tests in build-test/, never build/: rebuilding the app
// bundle in place under a running Grazer is what `just run` is for. Only the
// test target builds.
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, buildTarget, lockstepSourceArgs, main, run } from './lib.mjs';

const TEST_DIR = join(ROOT, 'build-test');

await main(async () => {
  if (!existsSync(join(TEST_DIR, 'CMakeCache.txt')))
    await run('cmake', [
      '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-B', TEST_DIR, ...lockstepSourceArgs(),
    ]);

  await buildTarget(TEST_DIR, 'GrazerTests');
  await run('ctest', ['--test-dir', TEST_DIR, '--output-on-failure']);
});
