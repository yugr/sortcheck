[![License](http://img.shields.io/:license-MIT-blue.svg)](https://github.com/yugr/sortcheck/blob/master/LICENSE.txt)
[![Build Status](https://github.com/yugr/sortcheck/actions/workflows/ci.yml/badge.svg)](https://github.com/yugr/sortcheck/actions)
[![Codecov coverage](https://img.shields.io/codecov/c/github/yugr/sortcheck.svg)](https://codecov.io/gh/yugr/sortcheck)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/yugr/sortcheck.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/yugr/sortcheck/alerts/)
[![Coverity Scan](https://scan.coverity.com/projects/19944/badge.svg)](https://scan.coverity.com/projects/yugr-sortcheck)

# What is this?

SortChecker is a tool for detecting violations
of [ordering axioms](http://pubs.opengroup.org/onlinepubs/009695399/functions/qsort.html)
in comparison functions passed to `qsort`
(also `bsearch`, `lfind`, etc.). For complex data structures it's very
easy to violate one of the requirements. Such violations cause
undefined behavior and may lead to all sorts of runtime
errors (including [unexpected results](https://groups.google.com/d/topic/golang-checkins/w4YWUgBhjJ0),
[inconsistent results across different platforms](https://gcc.gnu.org/ml/gcc/2017-07/msg00078.html)
or even [aborts](https://bugzilla.samba.org/show_bug.cgi?id=3959))
(also [here](https://stackoverflow.com/questions/2441045/bewildering-segfault-involving-stl-sort-algorithm),
see [this answer](https://stackoverflow.com/a/24048654/2170527),
[Qualys analysis](https://www.openwall.com/lists/oss-security/2024/01/30/7) and
[my slides](https://github.com/yugr/CppRussia/blob/master/2023/EN.pdf) for explanations).

The tool works by intercepting `qsort` and friends through `LD_PRELOAD`
and performing various checks prior to passing control to libc.
It could be applied to both C and C++ programs although for the
latter `std::sort` and `std::binary_search` are more typical
(use my [SortChecker++](https://github.com/yugr/sortcheckxx) tool
to diagnose errors in them).

The tool is quite robust - I've successfully
booted stock Ubuntu 14, Fedora 22 and Debian chroot and bootstrapped
GCC 4.9.

The project is MIT-licensed. It has no fancy dependencies,
just Glibc and Bash.

# What are current results?

I've done some basic testing of Ubuntu 14.04 and Fedora 22 distro
under SortChecker (open file/web browsers, navigate system menus,
install various apps, etc.).

The tool has found errors in many programs.  Here are some trophies:
* [Libxt6: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93273)
* [Libharfbuzz: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93274) (fixed)
* [Libharfbuzz: Unsorted array used in bsearch](https://bugs.freedesktop.org/show_bug.cgi?id=93275) (fixed)
* [Cpio: HOL\_ENTRY\_PTRCMP triggers undefined behavior](http://savannah.gnu.org/bugs/index.php?46638)
* [GCC: reload\_pseudo\_compare\_func violates qsort requirements](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68988) (fixed)
* [GCC: libbacktrace: bsearch over unsorted array in unit\_addrs\_search](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69050) (intentional)
* [GCC: Fix intransitive comparison in dr\_group\_sort\_cmp](https://gcc.gnu.org/ml/gcc-patches/2015-12/msg02141.html) ([was already fixed on trunk](https://gcc.gnu.org/ml/gcc-patches/2015-11/msg02444.html))
* [GCC: Fix qsort ordering violation in tree-vrp.c](https://gcc.gnu.org/ml/gcc-patches/2017-07/msg00882.html) ([confirmed](https://gcc.gnu.org/ml/gcc-patches/2017-07/msg00897.html))
* [dpkg: pkg\_sorter\_by\_listfile\_phys\_offs violates qsort requirements](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=808912) (fixed)
* [Fontforge: line\_pt\_cmp violates qsort ordering axioms](https://github.com/fontforge/fontforge/issues/2602)
* [Flexible I/O Tester: Invalid comparison function](https://github.com/axboe/fio/issues/140) (fixed)
* [Infernal: Inconsistent results from qsort callback](https://github.com/EddyRivasLab/infernal/issues/11) (fixed)
* [Graphicsmagick: Inconsistent results from qsort callback](https://sourceforge.net/p/graphicsmagick/bugs/562/) (fixed)
* [PostGIS: Inconsistent results from qsort callback](https://trac.osgeo.org/postgis/ticket/4093) (fixed)
* [Grass: Inconsistent results from qsort callback in g.mkfontcap](https://trac.osgeo.org/grass/ticket/3564) (fixed)

I haven't seen a noticeable slowdown when working in a fully checked
distro or building C++ projects with a checked compiler.

# Usage

You do not need to rebuild your app to test it under SortChecker.
Just run with preloaded `libsortcheck.so`:

```
$ LD_PRELOAD=libsortcheck.so myapp ...
```

(you'll probably want to combine this with some kind of regression
or random/fuzz testing to achieve good coverage,
also see the `shuffle` and `start` options below).

You could also use a helper script `sortcheck` to do this for you:

```
$ sortcheck myapp ...
```

To debug the issue, you can run with

```
$ SORTCHECK_OPTIONS=raise=1 sortcheck myapp ...
```

and then examine generated coredump in `gdb`.

By default SortChecker enables a set of common checks which should
be enough for most users. You can also customize it's behavior
through `SORTCHECK_OPTIONS` environment variable which is
a colon-separated list of option assignments e.g.

```
$ export SORTCHECK_OPTIONS=debug=1:max_errors=10
```

You can also put option string to `/SORTCHECK_OPTIONS` file
(this is particularly useful for testing of daemon processes).

Supported options are
* `max_errors` - maximum number of errors to report (default 10)
* `debug` - print debug info (default false)
* `print_to_file` - print warnings to specified file (rather
than default stderr)
* `print_to_syslog` - print warnings to syslog instead of stderr
(default false)
* `do_report_error` - print reports (only used for benchmarking,
default true)
* `raise` - raise signal on detecting violation (useful for
inspecting issues in debugger)
* `sleep` - sleep for N seconds before printing error and continuing
(may be useful for attaching with gdb and examining the situation)
* `check` - comma-separated list of checks to perform;
available options are
  * `default` - default set of checks (see below)
  * `basic` - check that comparison functions return stable results
  and does not modify inputs (enabled by default)
  * `sorted` - check that arrays passed to bsearch are sorted (enabled
  by default)
  * `symmetry` - check that cmp(x,y) == -cmp(y,x) (enabled by default)
  * `transitivity` - check that if x < y && y < z, then x < z
  (enabled by default)
  * `reflexivity` - check that cmp(x,x) == 0 (usually not very important
  so disabled by default, on the other hand may trigger on otherwise
  undetected asymmetry bugs)
  * `unique` - check that cmp does not compare different objects
  as equal (to avoid [random orderings on different platforms](https://gcc.gnu.org/ml/gcc/2017-07/msg00078.html))
  * `good_bsearch` - bsearch uses a restricted (non-symmetric) form
  of comparison function so some checks are not generally applicable;
  this option tells SortChecker that it should test bsearch more
  aggressively (unsafe so disabled by default). Note that this
  option may cause runtime errors or crashes if applied
  inappropriately.
  * for each option `XYZ` there's a dual `no_XYZ` (which disables
  corresponding check)
* `shuffle` - reshuffle array before checking with given seed;
  a value of `rand` will use random seed
  (helps find bugs which are not located at start of array)
* `start` - check the `start`-th group of 32 leading elements (default 0);
  a value of `rand` will select random group

# Applying to full distribution

You can run full Linux distro under SortChecker:
* add full path to `libsortcheck.so` to `/etc/ld.so.preload`
* create a global config:

  ```
  $ echo print_to_syslog=1:check=default:shuffle=rand | sudo tee /SORTCHECK_OPTIONS 
  $ sudo chmod a+r /SORTCHECK_OPTIONS
  ```

* reboot

Due to randomized order of checks it makes sense to check for errors and
reboot several times to detect more errors.

Disclaimer: in this mode libsortcheck.so will be preloaded to
all your processes so any malfunction may permanently break your
system. It's highly recommended to backup the disk or make
VM snapshot.

# Build

To build the tool, simply run make from project top directory.
Makefile supports various candies (e.g. AddressSanitizer,
debug build, etc.) - run `make help` for mode details.

If you enable AddressSanitizer you'll need to add libasan.so
to `LD_PRELOAD` (before `libsortcheck.so`).

To test the tool, run `make check`. Note that I've myself only
tested SortChecker on Ubuntu and Fedora.

# Known issues

* SortChecker is not fully thread-safe yet (should be easy to fix though)
* SortChecker is currently Linux-only (relies on `LD_PRELOAD`)

# Future plans

The tool only supports C now which rules out most of C++ code
because it uses (inline) `std::sort` and `std::binary_search`
(and other similar APIs). For those see another tool
[SortChecker++](https://github.com/yugr/sortcheckxx)
which does a simple compile-time instrumentation via Clang.

It would be great to make SortChecker a part of standard debuggin tool
like UBsan. Here's a [discussion](http://lists.llvm.org/pipermail/llvm-dev/2016-January/093835.html)
in LLVM mailing list which unfortunately didn't go too far.

It may also make sense to check other popular sorting APIs:
* `qsort_s`, `bsearch_s` (are they availabile/used?)
* `fts_open`, `scandir`
* Berkeley DB's `set_bt_compare`, `set_dup_compare`, etc.
* Glib2's `g_qsort_with_data` and other users of GCompareFunc/GCompareDataFunc
* Gnulib's `gl_listelement_compar_fn` and friends
* Libiberty's `splay_tree` API
* OpenSSL's `objects.h` API
* etc.

Here's less high-level stuff (sorted by priority):
* ensure that code is thread-safe (may need lots of platform-dependent code for atomics...)
* print complete backtrace rather than just address of caller (libunwind?)
* autotest on BSDs (via [vmactions](https://github.com/vmactions/netbsd-vm))
* other minor TODO/FIXME are scattered all over the codebase
