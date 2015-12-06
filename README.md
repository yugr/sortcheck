# What is this?

SortChecker is a proof-of-concept tool to detect violations
of ordering axioms in comparison functions passed to qsort
or bsearch routines. For complex data structures it's very
easy to violate one of the requirements. Such violations cause
undefined behavior and may in practice result in all sorts
of runtime errors.

The tool works by intercepting qsort and friends through LD\_PRELOAD
and performing various checks prior to passing control to libc.
It could be applied to both C and C++ programs although for the
latter std::sort is more typical (which would probably require
compile-time instrumentation?).

The tool is a proof-of-concept so it's hacky and slower than
necessary.

# What are current results?

I've done some basic testing of Ubuntu 14.04 distro under
SortChecker (open file/web browsers, navigate system menus, etc.).

The tool has found errors in many programs.  Here are some examples:
* [Libxt6: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93273)
* [Libharfbuzz: Invalid comparison function](https://bugs.freedesktop.org/show_bug.cgi?id=93274)
* [Libharfbuzz: Unsorted array used in bsearch](https://bugs.freedesktop.org/show_bug.cgi?id=93275)

There are also reports for GCC, Firefox (libxul.so) and other heavyweight stuff
(nautilus, Unity's unit-blah-blah, etc.)

I haven't seen a noticeable slowdown when working in checked Ubuntu
although CPU-intensive tests (e.g. building a C++ project) seem to have a ~15% slowdown.

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
* max\_errors - maximum number of errors to report
* debug - print debug info
* print\_to\_syslog - print warnings to syslog (instead of stderr)

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
To enable debug builds, pass DEBUG=1 to make.
To enable AddressSanitizer, pass SANITIZE=1 to make
(note that libasan.so will then need to be added to LD\_PRELOAD
together with libsortcheck.so).

To test the tool, run make check. Note that I've myself only
tested SortChecker on Ubuntu.

# Known issues

* SortChecker does not detect dlopen/dlclose so addresses from
  dynamically loaded libs will not be pretty-printed.

# TODO

Various TODOs are scattered all over the codebase.
High-level stuff:
* (!!) investigate remaining errors found in Ubuntu
* (!!) verify that Ubuntu is stable under libsortcheck
* (!) print array elements which triggered errors (i.e. hex dumps)
* (!) write code comments
* intercept dlopen/dlclose to be able to pretty-print their addresses
* print backtraces (rather than just address of the caller)
* do not report repetative errors for some comparison function
* filter out trivial stuff (strcmp, short sizes are likely to be ints, etc.)

