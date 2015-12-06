# What is this?

SortChecker is a proof-of-concept tool to detect violations
of order axioms in comparison functions passed to qsort
or bsearch routines. For complex data structures it's very
easy to violate one of the requirements. Such violations cause
undefined behavior and may in practice result in all sorts
of runtime errors.

The tool works by intercepting qsort and friends through LD\_PRELOAD
and performing various checks prior to passing control to libc.
It could be applied to both C and C++ programs although for the
latter std::sort is more typical (which would probably require
compile-time instrumentation?).

The tool is a proof-of-concept so it's hacky and slow.

# What are current results?

tbd + compiler instrumentation in C++ world

# Usage

Run your app with preloaded libsortcheck.so:
 $ LD\_PRELOAD=libsortcheck.so myapp ...

You can customize behavior through SORTCHECK\_OPTIONS environment
variable - a comma-separated list of option assignments e.g.

```
$ export SORTCHECK\_OPTIONS=debug=1:max\_errors=10
```

Supported options are
* max\_errors - maximum number of errors to report
* debug - print debug info
* print\_to\_syslog - print warnings to syslog (instead of stderr)

# Applying to full distribution

You can run full Linux distro under SortChecker: just add
full path to libsortcheck.so to /etc/ld.so.preload and

```
export SORTCHECK_OPTIONS=print_to_syslog=1
```

to /etc/profile and reboot.

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

# Test

To test the tool, run test/test.sh from project top directory.

# TODO

Various TODOs are scattered all over the codebase.
High-level stuff:
* (!!) investigate errors found in Ubuntu
* (!!) verify that Ubuntu is stable under libsortcheck
* (!) print array elements which triggered errors
* (!) write comments
* print backtraces (rather than just address of the caller)
* do not report repetative errors for some comparison function
* filter out trivial stuff (strcmp, short sizes are likely to be ints, etc.)

