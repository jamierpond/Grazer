// Configure (if needed) + build Grazer.exe with CMake (default VS generator).
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { BUILD_DIR, cpmSourceArgs, main, run } from './lib.mjs';

await main(async () => {
  if (!existsSync(join(BUILD_DIR, 'CMakeCache.txt')))
    await run('cmake', ['-B', BUILD_DIR, ...cpmSourceArgs()]);

  await run('cmake', ['--build', BUILD_DIR, '--config', 'Debug']);
});
