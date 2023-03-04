# Testing the library

Shnet aims for the highest code coverage possible, preferably 100%.

That's possible with the testing module at [`<shnet/test.h>`](../docs/c/test.md)
with which erroneous code paths can be triggered. Code paths that are very
unlikely to happen (because of very precise timing, race conditions, etc) and
don't contain any code that could cause failure (like code that `goto`'s to the
beginning of the function) might not be tested due to the sheer luck in trying
to get them to execute and/or complexity in reaching them in the first place.
Additionally, some modules serve the purpose of utility, and not every function
in them might be used and exposed in the header, which is an excuse to not test
these functions. Everything else **must** be covered.

But shnet's tests don't only aim for a 100% code coverage. In fact, some tests
are larger than the source code of the module they test, because various
combinations of functions under many circumstances are tested as well, and
functions that exist in both thread-safe and unsafe forms are tested up to 3
times. Locks are put where they are not needed, apparently redundant
`assert()`'s may be lying around, obviously correct code may be tested multiple
times, but this is all needed to cover all of the worst types of bugs - that are
hidden, and they don't exist until proven otherwise, at which point its too
late. You will not find these bugs by just staring at the code, but testing
that is so extensive it's dumb might just cover them, and already has a few
times.

All tests have a 3-digit number suffix in front of the name of the module they
are testing (or a subsection of it). They need to be tested in the right order,
or otherwise a bug-introducing change will lead to the 10th test failing, but in
fact the 1st (executed after the 10th due to their first letter of module
difference) will be the bug introducer. Wasting time on debugging is the number
1 fun ruiner in programming.

Some ancient test suites that might not even have their respective module still
in the library can be found under the `archive` directory. You may be lucky
enough to find their source in the `src/archive` folder, and their documentation
in `docs/archive`.
