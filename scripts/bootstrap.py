"""Fetch build dependencies at immutable revisions; requires Python 3.9+ and Git."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'dependencies.lock.json').read_text())


def run(*args):
    subprocess.run(args, check=True)


def checkout(path, spec):
    path.mkdir(parents=True, exist_ok=True)
    if not (path / '.git').exists():
        run('git', 'init', str(path))
        run('git', '-C', str(path), 'remote', 'add', 'origin', spec['url'])
    # No branch/tag resolution and no destructive reset of an existing checkout.
    current = subprocess.run(['git', '-C', str(path), 'rev-parse', '--verify', 'HEAD'],
                             capture_output=True, text=True)
    if current.returncode:
        # The only pre-existing SDK file is iPlug2's tracked README placeholder.
        files = [p.name for p in path.iterdir() if p.name != '.git']
        if files == ['README.md'] and path.name == 'VST3_SDK':
            (path / 'README.md').unlink()
        elif files:
            raise RuntimeError(f'Refusing to overwrite nonempty dependency directory: {path}')
    if current.stdout.strip() != spec['commit']:
        run('git', '-C', str(path), 'fetch', '--depth=1', 'origin', spec['commit'])
        run('git', '-C', str(path), 'checkout', '--detach', spec['commit'])
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', '--verify', 'HEAD'], text=True).strip()
    if actual != spec['commit']:
        raise RuntimeError(f'Wrong dependency revision: {path}')


def main():
    iplug = ROOT / '.deps/iPlug2'
    checkout(iplug, LOCK['iPlug2'])
    sdk = iplug / 'Dependencies/IPlug/VST3_SDK'
    # iPlug2 ships only a README placeholder here, not the SDK itself.
    checkout(sdk, LOCK['VST3_SDK'])
    run('git', '-C', str(sdk), 'submodule', 'update', '--init', '--depth=1',
        'base', 'pluginterfaces', 'public.sdk', 'cmake')
    print('Pinned iPlug2 and VST3 SDK are ready.')


if __name__ == '__main__':
    main()
