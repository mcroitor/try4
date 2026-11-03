# Fathom

Vendored Syzygy tablebase probing code from
<https://github.com/jdart1/Fathom>, retrieved 2026-10-31. No local source
changes were made. The files are compiled as one C++ translation unit
(`tbprobe.c` includes `tbchess.c`) so the Windows build uses
`std::atomic`/`std::mutex` rather than a C11 atomics dependency.

Fathom is MIT-licensed; see [`LICENSE`](LICENSE). The tablebase data files are
separate external data and are not part of this directory.
