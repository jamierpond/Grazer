// Configure (if needed) + build Grazer.app with CMake/Ninja.
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { BUILD_DIR, cpmSourceArgs, main, run } from './lib.mjs';

await main(async () => {
  if (!existsSync(join(BUILD_DIR, 'build.ninja')))
    await run('cmake', [
      '-G', 'Ninja', '-B', BUILD_DIR, '-DCMAKE_BUILD_TYPE=Debug', ...cpmSourceArgs(),
    ]);

  await run('cmake', ['--build', BUILD_DIR]);
});
