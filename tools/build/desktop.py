"""Generate a desktop entry for a native Linux build without installing it."""

import argparse
from pathlib import Path


def value(text):
    return (str(text).replace('\\', '\\\\').replace('\n', '\\n')
            .replace('\r', '\\r').replace('\t', '\\t'))


def command_argument(text):
    # Exec quoting is a second layer inside the desktop string value. This is
    # deliberately independent of shell quoting: no shell launches this entry.
    escaped = ''.join('\\' + char if char in '\\"`$' else char for char in str(text))
    return value('"' + escaped.replace('%', '%%') + '"')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--working-dir', required=True, type=Path)
    parser.add_argument('--icon', required=True, type=Path)
    parser.add_argument('--name', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.write_text(
        '[Desktop Entry]\nType=Application\nVersion=1.0\n'
        f'Name={value(args.name)}\n'
        f'Exec={command_argument(args.executable.resolve())}\n'
        f'Path={value(args.working_dir.resolve())}\n'
        f'Icon={value(args.icon.resolve())}\n'
        'Terminal=false\nCategories=Game;\nStartupNotify=true\n', encoding='utf-8')


if __name__ == '__main__':
    main()
