// Configure + build Release Grazer with line coverage counters in build-coverage,
// which also puts line-coverage.py in build-coverage/PondLineCoverage.
import { buildReleaseGrazer, main } from './lib.mjs';

await main(() => buildReleaseGrazer({ withLineCoverage: true }));
