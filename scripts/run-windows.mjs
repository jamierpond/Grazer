// Build, then launch Grazer.exe.
// Detached and not waited on, like `open` on macOS: `just run` returns once
// Grazer is up, and Grazer outlives the terminal.
import { spawn } from 'node:child_process';
import { join } from 'node:path';
import { BUILD_DIR, ROOT, main, runNode } from './lib.mjs';

await main(async () => {
  await runNode(join(ROOT, 'scripts', 'build-windows.mjs'));

  spawn(join(BUILD_DIR, 'Grazer.exe'), [], { cwd: ROOT, detached: true, stdio: 'ignore' }).unref();
});
