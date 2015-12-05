# What is this?

SortChecker is a proof-of-concept tool to detect violations
of order axioms in comparison functions passed to qsort
or bsearch routines. Such violations have undefined behavior
and may in practice cause all sorts of runtime errors.

The tool works by intercepting qsort and friends through LD\_PRELOAD
and performing various checks prior to passing control to libc.

The tool is a proof-of-concept so it's hacky and slow.

# What are current results?

tbd + compiler instrumentation in C++ world

# Usage

Run your app with preloaded libsortcheck.so:
 $ LD\_PRELOAD=libsortcheck.so myapp ...

You can customize behavior through SORTCHECK\_OPTIONS environment
variable - a comma-separated list of option assignments e.g.
 $ export SORTCHECK\_OPTIONS=debug=1:max\_errors=10
Supported options are
* max\_errors - maximum number of errors to report
* debug - print debug info
* print\_to\_syslog - print warnings to syslog (instead of stderr)

# Build

To build the tool, simply run make from project top directory.

# Test

To test the tool, run test/test.sh from project top directory.

# TODO

Various TODOs are scattered all over the codebase.
High-level stuff:
* apply to real distribution (e.g. boot)
* print array elements which triggered errors
* write comments
* filter out trivial stuff (strcmp, short sizes are likely to be ints, etc.)

