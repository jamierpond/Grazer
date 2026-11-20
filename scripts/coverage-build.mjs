// Configure + build Release Grazer with line coverage counters in build-coverage,
// which also puts line-coverage.py in build-coverage/PondLineCoverage.
import { buildReleaseGrazer, main, readPasskeySigningSettings } from './lib.mjs';

await main(() => buildReleaseGrazer({
  withLineCoverage: true,
  passkeySigning: readPasskeySigningSettings(process.env),
}));
