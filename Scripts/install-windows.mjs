// Build Release + install to %LOCALAPPDATA%\Programs\Grazer\Grazer.exe.
//
// Grazer.exe is the whole app: the web UI is embedded and WebView2's loader is
// linked statically. A running installed Grazer is closed first (and the new
// one launched after); dev builds are left alone. If the old exe is still
// locked it is renamed aside to Grazer.exe.old, which Windows allows, and
// removed on the next install.
//
// Then the Windows side of what LaunchServices does on macOS: Desktop and
// Start Menu shortcuts, and Grazer registered under HKCU as a browser for
// http/https, so it is offered under Settings > Default apps and links open
// in it as `Grazer.exe "<url>"`. Windows lets only the user pick the default.
//
// The Release tree is always reconfigured, so CPM refetches branch-pinned
// packages (Rudder@main) and the install has the latest Rudder client.
import { execFileSync, spawn } from 'node:child_process';
import { copyFileSync, existsSync, mkdirSync, renameSync, rmSync } from 'node:fs';
import { join } from 'node:path';
import { RELEASE_BUILD_DIR, cpmSourceArgs, main, run } from './lib.mjs';

const DIR = join(process.env.LOCALAPPDATA, 'Programs', 'Grazer');
const EXE = join(DIR, 'Grazer.exe');
const OLD = `${EXE}.old`;

// Run a PowerShell script without a shell in between to mangle its quoting.
function powershell(script) {
  const quiet = `$ProgressPreference = 'SilentlyContinue'\n${script}`;
  const encoded = Buffer.from(quiet, 'utf16le').toString('base64');
  return execFileSync('powershell.exe',
    ['-NoProfile', '-NonInteractive', '-EncodedCommand', encoded],
    { encoding: 'utf8' }).trim();
}

// Close whatever runs from EXE: a window close first, so the session is
// saved as on quit, then a kill for one that won't go. Returns whether one
// was running.
function quitInstalled() {
  const ran = powershell(`
    $p = @(Get-Process Grazer -ErrorAction SilentlyContinue |
           Where-Object { $_.Path -eq '${EXE}' })
    $p | ForEach-Object { [void]$_.CloseMainWindow() }
    $p | ForEach-Object { if (-not $_.WaitForExit(5000)) { $_.Kill(); $_.WaitForExit() } }
    $p.Count`);

  return Number(ran) > 0;
}

function copyExe(built) {
  mkdirSync(DIR, { recursive: true });
  rmSync(OLD, { force: true });

  try {
    copyFileSync(built, EXE);
  } catch {
    renameSync(EXE, OLD);
    copyFileSync(built, EXE);
  }
}

function addShortcuts() {
  powershell(`
    $shell = New-Object -ComObject WScript.Shell
    foreach ($folder in [Environment]::GetFolderPath('Desktop'),
                        [Environment]::GetFolderPath('Programs')) {
      $link = $shell.CreateShortcut((Join-Path $folder 'Grazer.lnk'))
      $link.TargetPath = '${EXE}'
      $link.WorkingDirectory = '${DIR}'
      $link.IconLocation = '${EXE}'
      $link.Description = 'Grazer'
      $link.Save()
    }`);

  execFileSync('ie4uinit.exe', ['-show']);
}

function setRegistry(key, name, value) {
  execFileSync('reg.exe',
    ['add', `HKCU\\${key}`, ...(name ? ['/v', name] : ['/ve']), '/d', value, '/f'],
    { stdio: 'ignore' });
}

function registerAsBrowser() {
  const progId = 'Software\\Classes\\GrazerURL';
  const client = 'Software\\Clients\\StartMenuInternet\\Grazer';
  const capabilities = `${client}\\Capabilities`;
  const icon = `${EXE},0`;

  setRegistry(progId, null, 'Grazer URL');
  setRegistry(`${progId}\\DefaultIcon`, null, icon);
  setRegistry(`${progId}\\shell\\open\\command`, null, `"${EXE}" "%1"`);

  setRegistry(client, null, 'Grazer');
  setRegistry(`${client}\\DefaultIcon`, null, icon);
  setRegistry(`${client}\\shell\\open\\command`, null, `"${EXE}"`);

  setRegistry(capabilities, 'ApplicationName', 'Grazer');
  setRegistry(capabilities, 'ApplicationIcon', icon);
  setRegistry(capabilities, 'ApplicationDescription',
    'A keyboard-first browser: Vim keys, one fuzzy palette.');
  setRegistry(`${capabilities}\\URLAssociations`, 'http', 'GrazerURL');
  setRegistry(`${capabilities}\\URLAssociations`, 'https', 'GrazerURL');

  setRegistry('Software\\RegisteredApplications', 'Grazer', capabilities);
}

await main(async () => {
  await run('cmake', ['-B', RELEASE_BUILD_DIR, ...cpmSourceArgs()]);
  await run('cmake',
    ['--build', RELEASE_BUILD_DIR, '--config', 'Release', '--target', 'Grazer']);

  quitInstalled();
  copyExe(join(RELEASE_BUILD_DIR, 'Release', 'Grazer.exe'));
  addShortcuts();
  registerAsBrowser();

  spawn(EXE, [], { cwd: DIR, detached: true, stdio: 'ignore' }).unref();

  console.log(`Installed ${EXE}`);
  console.log('Pick Grazer as the default browser in Settings > Default apps.');
});
