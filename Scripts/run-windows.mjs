// Build, then launch Grazer.exe (multi-config VS layout, single-config fallback).
// Detached and not waited on, like `open` on macOS: `just run` returns once
// Grazer is up, and Grazer outlives the terminal.
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { join } from 'node:path';
import { BUILD_DIR, ROOT, main, runNode } from './lib.mjs';

await main(async () => {
  await runNode(join(ROOT, 'Scripts', 'compile-windows.mjs'));

  const candidates = [join(BUILD_DIR, 'Debug', 'Grazer.exe'), join(BUILD_DIR, 'Grazer.exe')];
  const exe = candidates.find(existsSync);
  if (!exe) throw new Error(`Grazer.exe not found in ${BUILD_DIR}`);

  spawn(exe, [], { cwd: ROOT, detached: true, stdio: 'ignore' }).unref();
});
