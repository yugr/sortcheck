# What is this?

SortChecker is a proof-of-concept tool to detect violations
of [ordering axioms](http://pubs.opengroup.org/onlinepubs/009695399/functions/qsort.html)
in comparison functions passed to qsort
(also bsearch, lfind, etc.). For complex data structures it's very
easy to violate one of the requirements. Such violations cause
undefined behavior and may lead to all sorts of runtime
errors in practice (including [unexpected results](https://groups.google.com/d/topic/golang-checkins/w4YWUgBhjJ0)
or even [aborts](https://bugzilla.samba.org/show_bug.cgi?id=3959)).

The tool works by intercepting qsort and friends through LD\_PRELOAD
and performing various checks prior to passing control to libc.
It could be applied to both C and C++ programs although for the
latter std::sort and std::binary\_search are more typical
(see [Future plans](#future-plans)).

The tool is a proof-of-concept so it's hacky and slower than
necessary. Still it's quite robust - I've successfully
booted stock Ubuntu 14 and bootstrapped GCC 4.9 under it.

# What are current results?

I've done some basic testing of Ubuntu 14.04 distro under
SortChecker (open file/web browsers, navigate system menus, etc.).

The tool has found errors in many programs.  Here are some trophies:
* [Libxt6: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93273)
* [Libharfbuzz: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93274) (fixed)
* [Libharfbuzz: Unsorted array used in bsearch](https://bugs.freedesktop.org/show_bug.cgi?id=93275) (fixed)
* [Cpio: HOL\_ENTRY\_PTRCMP triggers undefined behavior](http://savannah.gnu.org/bugs/index.php?46638)
* [GDB: Invalid comparison functions](https://sourceware.org/bugzilla/show_bug.cgi?id=19361)
* [GCC: reload\_pseudo\_compare\_func violates qsort requirements](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68988) (confirmed)
* [GCC: libbacktrace: bsearch over unsorted array in unit\_addrs\_search](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69050) (intentional)
* [dpkg: pkg\_sorter\_by\_listfile\_phys\_offs violates qsort requirements](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=808912) (fixed)
* [Fontforge: line\_pt\_cmp violates qsort ordering axioms](https://github.com/fontforge/fontforge/issues/2602)
* [Flexible I/O Tester: Invalid comparison function](https://github.com/axboe/fio/issues/140) (fixed)

I haven't seen a noticeable slowdown when working in checked Ubuntu
or building C++ projects with a checked compiler.

# Usage

You do not need to rebuild your app to test it under SortChecker.
Just run with preloaded libsortcheck.so:

```
$ LD_PRELOAD=libsortcheck.so myapp ...
```

(you'll probably want to combine this with some kind of regression
or random/fuzz testing to achieve good coverage).

You could also use a helper script sortcheck to do this for you:

```
$ sortcheck myapp ...
```

By default SortChecker enables a set of common checks which should
be enough for most users. You can also customize it's behavior
through SORTCHECK\_OPTIONS environment variable which is
a colon-separated list of option assignments e.g.

```
$ export SORTCHECK_OPTIONS=debug=1:max_errors=10
```

Supported options are
* max\_errors - maximum number of errors to report (default 10)
* debug - print debug info (default false)
* print\_to\_file - print warnings to specified file (rather
than default stderr)
* print\_to\_syslog - print warnings to syslog instead of stderr
(default false)
* do\_report\_error - print reports (only used for benchmarking,
default true)
* check - comma-separated list of checks to perform;
available options are
  * basic - check that comparison functions return stable results
  and does not modify inputs (enabled by default)
  * sorted - check that arrays passed to bsearch are sorted (enabled
  by default)
  * symmetry - check that cmp(x,y) == -cmp(y,x) (enabled by default)
  * transitivity - check that if x < y && y < z, then x < z
  (enabled by default)
  * reflexivity - check that cmp(x,x) == 0 (usually not very important
  so disabled by default, on the other hand may trigger on otherwise
  undetected asymmetry bugs)
  * good\_bsearch - bsearch uses a restricted (non-symmetric) form
  of comparison function so some checks are not generally applicable;
  this option tells SortChecker that it should test bsearch more
  aggressively (unsafe so disabled by default). Note that this
  option may cause runtime errors or crashes if applied
  inappropriately),
  * for each option XYZ there's a dual no\_XYZ (which disables
  corresponding check)

# Applying to full distribution

You can run full Linux distro under SortChecker:
* add full path to libsortcheck.so to /etc/ld.so.preload
* set options in /etc/profile:

  ```
  export SORTCHECK_OPTIONS=print_to_syslog=1
  ```

* reboot

Disclaimer: in this mode libsortcheck.so will be preloaded to
all your processes so any malfunction may permanently break your
system. It's highly recommended to backup the disk or make
snapshot of VM.

# Build

To build the tool, simply run make from project top directory.
Makefile supports various candies (e.g. AddressSanitizer,
debug build, etc.) - run make help for mode details.

If you enable AddressSanitizer you'll need to add libasan.so
to LD\_PRELOAD (prior to libsortcheck.so).

To test the tool, run make check. Note that I've myself only
tested SortChecker on Ubuntu.

# Known issues

* SortChecker is not fully thread-safe yet
* SortChecker is currently Linux-only (relies on LD\_PRELOAD)

# Future plans

The tool only supports C now which rules out most of C++ code
because it uses (inline) std::sort and std::binary_search
(and other similar APIs). To check C++, we need a simple
compile-time instrumentation. This would also help with inline
implementations of bsearch in modern Glibc.

It may also make sense to check other popular sorting APIs:
* Berkeley DB's set\_bt\_compare, set\_dup\_compare, etc.
* Glib2's g\_qsort\_with\_data and other users of GCompareFunc/GCompareDataFunc
* Gnulib's gl\_listelement\_compar\_fn and friends
* Libiberty's splay\_tree API
* OpenSSL's objects.h API
* etc.

Here's less high-level stuff (sorted by priority):
* do not report repetative errors for same comparison function
* ensure that code is thread-safe (may need lot's of platform-dependent code...)
* print complete backtrace rather than just address of caller (libunwind?)
* print array elements which triggered errors (i.e. hex dumps)
* detect aborts when running comparison functions by intercepting SEGV?
* other minor TODO/FIXME are scattered all over the codebase

