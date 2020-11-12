#!/usr/bin/env python3
#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2020, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms
# of the BSD 2-Clause license.  Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

#
# make_cpio --- create a newc-style cpio(5) archive without metadata
#
# previously, in an attempt to create a ``reproducible'' cpio(5) archive
# (i.e., without any especially-variable metadata), and as described in
# `cmake-tool/helpers/cpio.cmake':
#
#     > Try and generate reproducible cpio meta-data as we do this:
#     > - touch -d @0 file sets the modified time to 0
#     > - --owner=root:root sets user and group values to 0:0
#     > - --reproducible creates reproducible archives with consistent
#     >   inodes and device numbering
#
# that is, for every file to be archived: a copy was made, to archive to
# throw away its partially- or fully-qualified path name; a GNU
# extension to touch(1) threw its timestamps away, and, via a GNU
# extension to cpio(1), its owner, group, device, and inode number were
# thrown away.
#
# there must be a better way!
#
#     % make_cpio archive.cpio ../kernel.elf ../kernel.dtb
#
# (I wrote this in C first, and then was defeated by the cmake build
# system in my attempt to add it to the build.  this is a very literal
# translation of that C into Python, which I don't know very well.)
#
# 2020-09-03	Jashank Jeremy <jashank.jeremy@unsw.edu.au>
#

import ctypes
import ctypes.util
from ctypes import c_char_p, c_int, c_int64, c_longlong, c_size_t, c_ssize_t, c_uint, c_void_p, c_wchar_p
import os
import stat
import sys
from typing import Any, List, NoReturn, Optional

# constants from <sysexits.h>:
EX_USAGE    : int = 64
EX_SOFTWARE : int = 70

# constants from <archive.h>
ARCHIVE_EOF    =   1            # Found end of archive.
ARCHIVE_OK     =   0            # Operation was successful.
ARCHIVE_RETRY  = -10            # Retry might succeed.
ARCHIVE_WARN   = -20            # Partial success.
ARCHIVE_FAILED = -25            # Current operation cannot complete.
ARCHIVE_FATAL  = -30            # No more operations are possible.

def main (args: List[str]) -> int:
	if len(args) < 2:
		errx(EX_USAGE, "usage: make_cpio archive-file file...")
	argv0, archive_file, *files = args

	A = load_libarchive()

	ark = A.archive_write_new()
	if ark is None:
		err (EX_SOFTWARE, "couldn't write archive object")

	if A.archive_write_set_format_cpio_newc(ark) != ARCHIVE_OK or \
	   A.archive_write_open_filename_w(ark, archive_file) != ARCHIVE_OK:
		warnx(f'{archive_file}: {A.archive_error_string(ark)}')
		A.archive_write_fail(ark)
		A.archive_write_free(ark)
		return -1

	for i, file in enumerate(files):
		basename_idx = file.rfind('/')
		if basename_idx == -1 or file[basename_idx:] == '/':
			warnx(f'skipping {file}: nonsense filename')
			continue

		basename = file[basename_idx + 1:]
		entry = A.archive_entry_new()
		if entry is None:
			warnx(f"skipping {file}: couldn't make archive entry object")
			continue

		fd = os.open(file, os.O_RDONLY)
		if fd == -1:
			warnx(f"skipping {file}: couldn't open")
			A.archive_entry_free(entry)
			continue

		sb = os.fstat(fd)
		# what if it failed, Python?

		if not stat.S_ISREG(sb.st_mode):
			warnx(f'skipping {file}: not a regular file')
			A.archive_entry_free(entry)
			os.close(fd)
			continue

		#
		# Surprise!  `os.fstat' returns a `stat_result', which is some
		# weirdo class that doesn't derive from `ctypes.Structure' ---
		# which it *should* --- and therefore you cannot pass it around
		# as if it were a `struct stat *'.
		#
		# This means we can't just use `archive_entry_copy_stat(3)',
		# because we cannot pass it the results of the `fstat(2)' we
		# just did.  Arrrrgh!
		#
		# A.archive_entry_copy_stat (entry, &sbuf);

		A.archive_entry_set_size (entry, sb.st_size);
		A.archive_entry_set_mode (entry, sb.st_mode);
		A.archive_entry_update_pathname_utf8 (entry, c_char_p(basename.encode('utf8')));
		A.archive_entry_set_uid (entry, 0);
		A.archive_entry_set_gid (entry, 0);
		A.archive_entry_unset_ctime (entry);
		A.archive_entry_unset_birthtime (entry);
		A.archive_entry_unset_mtime (entry);
		A.archive_entry_unset_atime (entry);
		A.archive_entry_set_dev (entry, 0);
		A.archive_entry_set_ino64 (entry, 0);

		if A.archive_write_header (ark, entry) != ARCHIVE_OK:
			warnx(f"abandoning {file}: couldn't write header: {A.archive_error_string(ark)}")
			A.archive_entry_free(entry)
			os.close(fd)
			continue

		bufsiz = 4096
		while True:
			buf = os.read(fd, bufsiz)
			if not buf or len(buf) == 0: break
			assert len(buf) > 0
			A.archive_write_data(ark, buf, len(buf))

		if A.archive_write_finish_entry(ark) != ARCHIVE_OK:
			warnx(f"finishing '{file}': {A.archive_error_string(ark)}");
		os.close(fd)
		A.archive_entry_free(entry)

	A.archive_write_close(ark)
	A.archive_write_free(ark)
	return 0


def load_libarchive():
	libarchive_path \
		=  os.environ.get('LIBARCHIVE') \
		or ctypes.util.find_library('archive')
	if libarchive_path is None:
		errx(EX_SOFTWARE, "cannot find libarchive")
	libarchive = ctypes.cdll.LoadLibrary(libarchive_path)
	declare_libarchive_types(libarchive)
	return libarchive

#
# the python binding doesn't expose parts of the `libarchive' api.
# because why would you ever want to write python.
#

c_archive_p       = c_void_p
c_archive_entry_p = c_void_p

def declare_libarchive_types(a: ctypes.CDLL):
	def typesig (a: ctypes.CDLL, f: str, argty: List[Any], retty: Any):
		fn = getattr(a, 'archive_' + f)
		fn.argtypes = argty
		fn.restype  = retty
		return fn

	typesig(a, 'write_new', [], c_archive_p)
	typesig(a, 'write_set_format_cpio_newc', [c_archive_p], c_int)
	typesig(a, 'write_open_filename_w', [c_archive_p, c_wchar_p], c_int)
	typesig(a, 'entry_new', [], c_archive_entry_p)
	typesig(a, 'entry_free', [], c_archive_entry_p)
	typesig(a, 'entry_set_size', [c_archive_entry_p, c_longlong], None)
	typesig(a, 'entry_set_mode', [c_archive_entry_p, c_int], None)
	typesig(a, 'entry_update_pathname_utf8', [c_archive_entry_p, c_char_p], None)
	typesig(a, 'entry_set_uid', [c_archive_entry_p, c_longlong], None)
	typesig(a, 'entry_set_gid', [c_archive_entry_p, c_longlong], None)
	typesig(a, 'entry_unset_ctime', [c_archive_entry_p], None)
	typesig(a, 'entry_unset_birthtime', [c_archive_entry_p], None)
	typesig(a, 'entry_unset_mtime', [c_archive_entry_p], None)
	typesig(a, 'entry_unset_atime', [c_archive_entry_p], None)
	typesig(a, 'entry_set_dev', [c_archive_entry_p, c_uint], None)
	typesig(a, 'entry_set_ino64', [c_archive_entry_p, c_int64], None)
	typesig(a, 'write_header', [c_archive_p, c_archive_entry_p], c_int)
	typesig(a, 'write_data', [c_archive_p, c_void_p, c_size_t], c_ssize_t)
	typesig(a, 'write_finish_entry', [c_archive_p], c_int)
	typesig(a, 'entry_free', [c_archive_entry_p], None)
	typesig(a, 'error_string', [c_archive_p], c_char_p)
	typesig(a, 'write_close', [c_archive_p], c_int)
	typesig(a, 'write_fail', [c_archive_p], None)
	typesig(a, 'write_free', [c_archive_p], None)


########################################################################

def warnx (message: str) -> None:
	sys.stderr.write(message + "\n")
def warn (message: str) -> None:
	warnx(f'{message}: <errno>')
def errx (code: int, message: str) -> NoReturn:
	warnx(message); sys.exit(code)
def err (code: int, message: str) -> NoReturn:
	warn(message);  sys.exit(code)

if __name__ == "__main__":
	sys.exit(main(sys.argv))
