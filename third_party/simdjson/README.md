# Vendored simdjson

This directory vendors the single-header simdjson v4.6.9 release from
https://github.com/simdjson/simdjson. The matching `simdjson.cpp` translation
unit is compiled directly into the `serde` static library by mcpp, so consumers
do not need a host simdjson installation.

## GCC 16 module adaptation

Known issue: upstream v4.6.9 still fails when this header is expanded from a
GCC 16 named-module interface; upgrading from the distro's v3.6.4 does not
remove the CMI serialization error. The vendored copy therefore carries the
small linkage-only patch described below.

GCC 16 serializes the bodies of exported templates when it writes a named
module interface. The upstream formatter helpers `escape_sequence` and
`fast_itoa` were declared in an anonymous namespace. A formatter template that
mentions those helpers therefore exposed a translation-unit-local entity and
GCC refused to write the CMI. Galay keeps the helpers in
`simdjson::internal`, changes the two `fast_itoa` overloads to ordinary
`inline` functions, and leaves all parsing, formatting, layout, and public API
behavior unchanged.

The adaptation is intentionally limited to linkage spelling. Keep this note
and the upstream license inventory in `LICENSE.md` when updating the vendored
release.

## Integration

`mcpp.toml` lists `third_party/simdjson/simdjson.cpp` in `[build].sources` and
sets `SIMDJSON_EXCEPTIONS=0` and `nssv_CONFIG_NO_EXCEPTIONS=1` to match the JSON
module and benchmark. The implementation remains a regular translation unit,
outside the JSON module interface. No wrapper under `src/`, pkg-config lookup,
or host library is needed.
