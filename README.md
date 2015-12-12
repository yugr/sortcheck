# What is this?

SortChecker is a proof-of-concept tool to detect violations
of ordering axioms in comparison functions passed to qsort
or bsearch routines. For complex data structures it's very
easy to violate one of the requirements. Such violations cause
undefined behavior and may lead to all sorts of runtime
errors in practice.

The tool works by intercepting qsort and friends through LD\_PRELOAD
and performing various checks prior to passing control to libc.
It could be applied to both C and C++ programs although for the
latter std::sort and std::binary\_search are more typical.
Checking those would be more involved and require
compile-time instrumentation (a simple one though).

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

There are also numerous reports for GCC (in progress), Firefox (libxul.so) and
other heavyweight stuff (nautilus, Unity apps, etc.).

I haven't seen a noticeable slowdown when working in checked Ubuntu
or building C++ projects.

# Usage

Run your app with preloaded libsortcheck.so:

```
 $ LD_PRELOAD=libsortcheck.so myapp ...
```

You can customize behavior through SORTCHECK\_OPTIONS environment
variable - a comma-separated list of option assignments e.g.

```
$ export SORTCHECK_OPTIONS=debug=1:max_errors=10
```

Supported options are
* max\_errors - maximum number of errors to report (default 10)
* debug - print debug info (default false)
* print\_to\_syslog - print warnings to syslog instead of stderr
(default false)
* do\_report\_error - print reports (only used for benchmarking,
default true)
* good\_bsearch - some programs (e.g. GCC) use restricted form of
bsearch which does not support all kinds of checks that SortChecker
has so for safety we have to disable some agressive checks by default;
setting this option to 1 enables these checks

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

* SortChecker does not detect dlopen/dlclose; this causes
addresses from dynamically loaded libs to not be pretty-printed.

# TODO

Various TODOs are scattered all over the codebase.
Here's the high-level stuff, sorted by priority:
* (!!) fix and report errors in GCC
* (!!) intercept dlopen/dlclose to be able to pretty-print addresses in plugins
* (!!) use it to debug remaining errors in Ubuntu
* (!!) verify that Ubuntu is stable under libsortcheck
* (!) do not report repetative errors for some comparison function
* (!) print complete backtrace rather than just address of caller (libunwind?)
* write code comments
* ensure that code is thread-safe
* print array elements which triggered errors (i.e. hex dumps)
* more intelligent error suppression
* provide flag(s) to tune aggressiveness of the checker (e.g. how many elements to consider, etc.)
* filter out trivial stuff (strcmp, short sizes are likely to be ints, etc.)

