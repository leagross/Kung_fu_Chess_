# `third_party/argon2` — the Argon2 reference implementation

The password hash. Vendored from
[P-H-C/phc-winner-argon2](https://github.com/P-H-C/phc-winner-argon2), tag
`20190702` — the reference implementation by the authors of the algorithm that
won the Password Hashing Competition. Dual-licensed CC0 / Apache-2.0; see
`LICENSE`.

## Why this is committed rather than downloaded

Every other dependency here comes through `FetchContent`. This one does not, for
two reasons:

**It has no CMake build of its own.** The upstream repository ships a Makefile,
so `FetchContent_MakeAvailable` has nothing to configure. The library has to be
declared by hand either way — see the root `CMakeLists.txt`, which is where the
`kfc_argon2` target lives.

**A password hash should not depend on the network to build.** Everything else
being downloadable is a convenience; a build that silently cannot produce the
credential-hashing code is not a convenience, and it is 126 KB.

## What was taken, and what was left

Only what a portable, single-threaded build needs:

| Kept | Why |
|------|-----|
| `include/argon2.h` | the public API |
| `src/argon2.c`, `core.c`, `encoding.c` | the algorithm and its PHC-string encoding |
| `src/ref.c` | the **portable** compression function |
| `src/blake2/` (minus `blamka-round-opt.h`) | Argon2's internal BLAKE2b |
| `src/thread.c/.h` | required by `core.c`; compiled to nothing under `ARGON2_NO_THREADS` |

Left out: `opt.c` and `blamka-round-opt.h` (SSE/AVX paths — they need
architecture detection this project has no other reason to carry, and `ref.c` is
correct everywhere), plus `run.c`, `bench.c`, `genkat.c` and `test.c`, which are
upstream's own command-line tools.

`ARGON2_NO_THREADS` is set in the root `CMakeLists.txt`: we hash with
parallelism 1, so the threading support would only add a pthread dependency for
code that never runs.

## Do not edit these files

They are upstream's, unmodified, so they can be re-fetched at a newer tag by
replacing the folder. Anything this project needs on top of Argon2 belongs in
`database/`, next to the rest of the credential handling.
