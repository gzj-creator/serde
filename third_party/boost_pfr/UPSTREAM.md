# Boost.PFR

Standalone headers from `boost-1.89.0`, licensed under Boost Software License 1.0.

- Archive: `https://codeload.github.com/boostorg/pfr/tar.gz/refs/tags/boost-1.89.0`
- SHA-256: `749e15549f4bda76cbca2106e0f5c4864bbbc89c9680a64a4183cc24a2b886b3`

Verify the checksum before extracting the archive. Copy `LICENSE_1_0.txt` unchanged.
For each header under the upstream `include/` tree, preserve the path and apply these
GNU sed substitutions to prevent mixing serde's copy with a consumer's Boost headers,
and normalize trailing whitespace and trailing blank lines:

```sh
sed -e 's|<boost/pfr|<third_party/boost_pfr/include/boost/pfr|g' \
    -e 's/\bnamespace boost\b/namespace serde_boost_pfr/g' \
    -e 's/\bboost::pfr\b/serde_boost_pfr::pfr/g' \
    -e 's/\bBOOST_PFR_/SERDE_BOOST_PFR_/g' \
    -e 's/\bBOOST_USE_MODULES\b/SERDE_BOOST_PFR_USE_MODULES/g' \
    -e 's/\bBOOST_MAGIC_GET_REGISTER_TYPE\b/SERDE_BOOST_PFR_REGISTER_TYPE/g' \
    -e 's/[[:blank:]]\+$//' "$header" |
awk '{ lines[NR] = $0; if (NF) last = NR } END {
    for (number = 1; number <= last; ++number) print lines[number]
}'
```

No system Boost includes, configure-time downloads, or additional link libraries are needed.
