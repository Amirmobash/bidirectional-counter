"""Compile the exact committed firmware alternatives in isolated sketch folders."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', action='store_true', help='install the pinned core and libraries first')
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    spec = json.loads((root / 'tools/firmware.json').read_text())
    cli = [os.environ.get('ARDUINO_CLI', 'arduino-cli')]
    if spec.get('url'):
        cli += ['--additional-urls', spec['url']]
    if args.install:
        subprocess.run(cli + ['core', 'update-index'], check=True)
        subprocess.run(cli + ['core', 'install', spec['core']], check=True)
        if spec['libs']:
            subprocess.run(cli + ['lib', 'update-index'], check=True)
            subprocess.run(cli + ['lib', 'install', '--no-deps', *spec['libs']], check=True)
    failed = []
    for index, relative in enumerate(spec['sources']):
        source = root / relative
        with tempfile.TemporaryDirectory(prefix='arduino-check-') as temporary:
            sketch = Path(temporary) / f'sketch_{index}'
            sketch.mkdir()
            # .cpp stays C++; Arduino sketch sources retain preprocessing. Do
            # not concatenate alternative programs that each define setup/loop.
            if source.suffix == '.cpp':
                (sketch / f'{sketch.name}.ino').write_text('// See firmware.cpp\n')
                shutil.copyfile(source, sketch / 'firmware.cpp')
            else:
                shutil.copyfile(source, sketch / f'{sketch.name}.ino')
            for header in source.parent.glob('*.h'):
                shutil.copyfile(header, sketch / header.name)
            result = subprocess.run(cli + ['compile', '--fqbn', spec['fqbn'],
                '--jobs', str(args.jobs), '--warnings', 'default', str(sketch)])
            print(f'{relative}: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
            if result.returncode:
                failed.append(relative)
    if failed:
        raise SystemExit('Compilation failed: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
