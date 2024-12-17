# threads
A simple cross-platform threads.h implementation

Rewrite of https://github.com/tinycthread/tinycthread as a single header and using system `<threads.h>` if available (Linux gcc/clang and MSVC on Windows as of 2024). Implementations provide a fallback Windows implementation for MinGW/Cygwin and a pthread-based implementation for Mac, which does not provide `<threads.h>`.

Released under the zlib license, similar to BSD-3 and other permissive licenses.