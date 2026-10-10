# fast_float

Unmodified headers from fast_float v8.0.2:
https://github.com/fastfloat/fast_float/tree/v8.0.2

Apache-2.0, MIT and Boost-1.0 license files are included. The TOML parser uses
this locale-independent conversion only on macOS, where the supported Apple
libc++ lacks floating-point overloads. Other platforms use std::from_chars.
