// Render Assets/Icon/wim-icon.svg into Wim.icns (+ a 512px PNG for the
// README). Each iconset size is rasterised straight from the SVG, so small
// sizes stay crisp. Needs ImageMagick with its librsvg delegate (`magick`)
// and macOS's `iconutil`. The outputs are committed, so a normal build never
// runs this -- rerun it after editing the SVG.
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { ROOT, main, run } from './lib.mjs';

const ICON_DIR = join(ROOT, 'Assets', 'Icon');
const SVG = join(ICON_DIR, 'wim-icon.svg');

// The SVG is 1024px at 72 dpi, so density scales it to `size` pixels.
const render = (size, out) =>
  run('magick', ['-background', 'none', '-density', String((72 * size) / 1024),
    SVG, '-resize', `${size}x${size}`, out]);

await main(async () => {
  const work = mkdtempSync(join(tmpdir(), 'wim-icon-'));
  const iconset = join(work, 'Wim.iconset');

  try {
    await run('mkdir', [iconset]);

    for (const size of [16, 32, 128, 256, 512]) {
      await render(size, join(iconset, `icon_${size}x${size}.png`));
      await render(size * 2, join(iconset, `icon_${size}x${size}@2x.png`));
    }

    await run('iconutil', ['-c', 'icns', iconset, '-o', join(ICON_DIR, 'Wim.icns')]);
    await render(512, join(ICON_DIR, 'wim-icon.png'));
  } finally {
    rmSync(work, { recursive: true, force: true });
  }
});
