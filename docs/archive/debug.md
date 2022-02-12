A module mainly used in testing at the moment, but the header can be added to any code wishing to log some stuff to a text file.
Run `sudo make dynamic/static debug=true` to enable logging capabilities of the `debug()` function. If you wish to always be able to log something and not depend on the flag, use `_debug()`.

Upon first usage, these functions will open a new file called `logs.txt` in the current directory and append new logs to the end of it.
If you need to free all memory upon exit, including the pointer to the file, run `debug_free()`.

The syntax is as follows:
```c
void debug(string, should_it_appear_on_stdout_boolean, any_string_args...);
```
If the second argument is `1`, the logged string will also appear in the `stdout`. Note that there's no formatting in files, so if you want your `stdout` to look pretty, consider logging formatted text in the console using `printf()` and not formatted to the text file using `debug()`.
```c
_debug("critical error, errno %d", 0, errno);

debug("a new connection on the server", 1);
```
File logging also has timestamps next to each new entry. They are not included in `stdout` logs if the bool is `1`. The formatting at the moment supports up to 1000 days of non stop logging, after which the formatting will break. It is designed to count time since the first log (first call to `debug()` or `_debug()`) and not since the epoch or show current date. Minimum timestamp accuracy is 1 microsecond, allowing for benchmarking or other time-precise processes.