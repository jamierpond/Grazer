// Refetch branch-pinned CPM packages (Rudder@main, ...) in every configured
// build tree. Re-running configure is what makes FetchContent's update step
// fetch and check out the branch's new head; nothing is built, so a running
// dev Grazer isn't touched. The next `just build` / `just run` compiles it.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { ROOT, cpmSourceArgs, main, run } from './lib.mjs';

await main(async () => {
  // Only trees configured from this checkout: a tree left over from before a
  // move (or from another worktree) won't reconfigure here.
  const ownTree = (name) => {
    const cache = join(ROOT, name, 'CMakeCache.txt');
    return existsSync(cache)
      && readFileSync(cache, 'utf8')
        .split('\n')
        .some((line) => line === `CMAKE_HOME_DIRECTORY:INTERNAL=${ROOT}`);
  };

  const trees = readdirSync(ROOT)
    .filter((name) => name.startsWith('build') && ownTree(name));

  for (const tree of trees) {
    console.log(`[update-deps] reconfiguring ${tree}`);
    await run('cmake', ['-B', join(ROOT, tree), ...cpmSourceArgs()]);
  }
});
