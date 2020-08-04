#!/usr/bin/env python3

import glob
import os
import argparse
from pathlib import Path
from string import Template
import itertools
import subprocess
from tempfile import NamedTemporaryFile
import multiprocessing

# should be fine to just hardcode these, IMO
BUILD_DIR = 'build'
BIN_DIR = BUILD_DIR
OBJ_DIR = os.path.join(BUILD_DIR, 'obj')
EXE_EXT = '.exe' if os.name == 'nt' else ''
# TODO: after the jam, split compiler and VM into two executables
# For now, we'll keep compiler source in 'src/compiler' and VM source in 'src/vm', with shared stuff in 'src/' directly.
BIN_NAME = 'coyote' + EXE_EXT

DEFAULT_FLAGS = {'-Wall', '-pedantic'}
DEBUG_FLAGS = {'-g', '-Og'}
# pixelherodev's personal flag set :P I'm insane, I know.
PIXELS_DEVEL_FLAGS = { '-Werror', '-Wextra', '-pedantic', '-march=native', '-mtune=native', '-falign-functions=32' }
RELEASE_FLAGS = {'-O2'}

# these *do* need to be in order, so no sets!
DEFAULT_LDFLAGS = ['-static', '-static-libgcc']

parser = argparse.ArgumentParser(description='Builds coyote.')
parser.add_argument('--pixel', dest='cflags', action='append_const',
                    const=PIXELS_DEVEL_FLAGS,
                    help='Enable additional development flags because you\'re pixelherodev and you make bad life choices')
parser.add_argument('--release', dest='cflags_mode', action='store_const',
                    const=RELEASE_FLAGS, default=DEBUG_FLAGS,
                    help='Enable release build')
parser.add_argument('-B', dest='force_rebuild', action='store_true',
                    help='Force rebuilding the project')
parser.add_argument('-r', dest='dynamic', action='store_true',
                    help='Build a dynamic executable')
parser.add_argument('-j', dest='num_threads', nargs='?',
                    type=int, default=max(multiprocessing.cpu_count() // 2, 1),
                    help='Number of threads to run `make` with')

args = parser.parse_args()
CFLAGS = DEFAULT_FLAGS | args.cflags_mode | (set(itertools.chain(*args.cflags)) if args.cflags else set())
LDFLAGS = DEFAULT_LDFLAGS
if args.dynamic:
    LDFLAGS = []

sources = glob.glob('src/**/*.c', recursive=True) + ['test/main.c']

def obj_from_c(cname, skipn=0):
    return os.path.join(OBJ_DIR, *Path(cname).with_suffix('.o').parts[skipn:])

sources_src = glob.glob('src/**/*.c', recursive=True)
sources_test = ['test/main.c']
sources = sources_src + sources_test
headers = glob.glob('src/**/*.h', recursive=True)
objects = {
    **{src: obj_from_c(src, 1) for src in sources_src},
    **{test: obj_from_c(test, 0) for test in sources_test}
}

object_dirs = {os.path.dirname(obj) for obj in objects.values()}
for dir in object_dirs:
	try:
		os.makedirs(dir)
	except OSError:
		pass

t = Template('''\
CFLAGS+=${CFLAGS}
LDFLAGS=${LDFLAGS}
INCLUDES=-Isrc -Ilib

# for easy run in repl.it
default: test

test: ${bin_path}
\t./${bin_path}

build: ${bin_path}

${bin_path}: ${objects}
\t$$(CC) $$(CFLAGS) $$(LDFLAGS) -o $$@ $$^

${OBJ_DIR}/%.o: src/%.c ${headers}
\t$$(CC) $$(CFLAGS) $$(INCLUDES) $$< -c -o $$@

${OBJ_DIR}/test/%.o: test/%.c ${headers}
\t$$(CC) $$(CFLAGS) $$(INCLUDES) $$< -c -o $$@
''').substitute(
    CFLAGS=' '.join(sorted(CFLAGS)),
    LDFLAGS=' '.join(LDFLAGS),
    BUILD_DIR=BUILD_DIR,
    BIN_DIR=BIN_DIR,
    OBJ_DIR=OBJ_DIR,
    EXE_EXT=EXE_EXT,
    BIN_NAME=BIN_NAME,
    bin_path=os.path.join(BIN_DIR, BIN_NAME),
    sources=sources,
    headers=' '.join(sorted(headers)),
    objects=' '.join(sorted(objects.values())),
)
print('-' * 10)
if '-Werror' in CFLAGS:
    print("Makefile generated.")
else:
    print(t.rstrip())
print('-' * 10)

with NamedTemporaryFile('w', prefix='coy-build-', suffix='.mk', delete=False) as f:
    f.write(t)

extra = []
if args.force_rebuild:
    extra.append('-B')
if args.num_threads is not ...:
    extra.append('-j%u' % args.num_threads if args.num_threads else '-j')
return_code = subprocess.call(['make'] + extra + ['-f', f.name, '--no-print-directory'])
os.unlink(f.name)
exit(return_code)

