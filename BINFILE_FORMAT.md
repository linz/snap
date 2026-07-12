SNAP BINARY FILE FORMAT
========================

This describes the `.bin` file `snap` writes and `snaplist`/`snapplot`/`snapspec` read back.
As of `feat/portable-bin-file-format`, the format is portable: a `.bin` file written by the
Windows build is byte-identical in structure to one written by the Linux build for the same
input. See the "Verification" section below for how that's actually proven, not just claimed.

Container format
-----------------

Every `.bin` file starts with a fixed signature string (`BINFILE_SIGNATURE`,
`src/snaplib/snap/filenames.h`) followed by a short binary trailer. `open_binary_file`
(`src/snaplib/util/binfile.cpp`) compares this byte-for-byte and returns `NULL` on any
mismatch - so bumping this string is how a breaking format change gets cleanly rejected by
older/newer builds, rather than silently misread. While a file is being *written*, the
signature's first byte is temporarily zeroed and only restored once `close_binary_file` runs to
completion - a file that was being written when the process crashed or was aborted fails the
signature check on its next open, rather than being read as if it were complete. This doesn't
apply to opening a file for reading, which never modifies it at all.

After the header, the file is a singly-linked list of named sections. Each section is:

- an 8-byte placeholder for the file offset of the *next* section (filled in when this section
  ends)
- the section name, as a null-terminated string
- an optional section version (int32), present only if the file's own container version
  (`BF_VERSION`, unrelated to `BINFILE_SIGNATURE` and unchanged by this portability work) is
  greater than zero
- the section's own payload bytes
- an `End_of_Section` marker

File positions throughout the file - this offset chain, and record locations elsewhere such as
survey-data records and notes - are read and written via `ftell64`/`fseek64` (`snapconfig.h`), so
the format now correctly handles files larger than 2GB on all platforms.

`find_section` (`binfile.cpp`) always searches from the start of the file, following the
next-offset chain - so sections can be located in any order, and locating one section doesn't
depend on what's already been read. `check_end_section` verifies the very next bytes really are
the `End_of_Section` marker, which is how a reader that consumed the wrong number of bytes for a
section gets caught immediately, rather than silently misreading everything after it.

A complete `.bin` file has these named sections, written by `snap` in this order (see
`dump_binary_data`/`snapmain.cpp`), plus one nested inside another:

| Section                                 | Written when                     |
|------------------------------------------|-----------------------------------|
| `OBSERVATIONS`                            | always, if `output binary_file`  |
| `CHOLESKI_DECOMPOSITION`                  | `output decomposition`           |
| `FULL_COVARIANCE`                         | `output full_covariance_matrix`  |
| `SNAP_GLOBALS`                             | always                           |
| `STNADJ` (with `Network` nested inside)   | always                            |
| `STATION_COVARIANCES`                     | always, except `data_check` mode |
| `STATION_RELATIVE_COVARIANCES`            | `relative_covariances`           |
| `DATA_FILES`                              | always                           |
| `OBS_CLASSES`                              | always                           |
| `RFTRANSFORMATIONS`                       | always                           |
| `MISCPARAMS`                              | always                           |

Portable fixed-width struct dumps
-----------------------------------

Several sections used to write an in-memory struct straight to disk (`fwrite(&x, sizeof(x), ...)`).
That bakes two compiler-specific things into the file:

- `long`, which is 8 bytes on Linux (LP64) but only 4 on Windows/MSVC (LLP64)
- compiler-specific struct padding, and - for `rfTransformation`'s bitfields - C bitfield packing
  order, which the standard leaves implementation-defined

The fix, used consistently for `station`, `rfTransformation`, `param`, and `survdata`: each
struct has its own table (`STATION_DISK_FIELDS`, `RFTRANS_DISK_FIELDS`, `PARAM_DISK_FIELDS`,
`SURVDATA_DISK_FIELDS` - `netstns1.cpp`/`rftrndmp.cpp`/`genparam.cpp`/`bindata.cpp` respectively)
listing each field's disk-fixed type (`FieldKind`: `Int32`/`UInt32`/`Int8`/`UInt8`/`Float64`) and
byte offset. Both the write and read functions walk the same table (`for_each_disk_field`,
`util/binfile.h`), so the two directions can't drift from each other; a compile-time
`static_assert` per table checks the table itself has no gap relative to the real struct layout,
so a field added, removed, or reordered in the struct without updating its table fails to
compile rather than silently misreading. Fields deliberately excluded from a table (pointers,
bitfields) are called out in the comment next to that table, along with how each is actually
handled - see the tables themselves for the exact field lists and exclusions; this doc describes
the mechanism once rather than duplicating each table's contents.

`OBSERVATIONS`
---------------

Each `OBSERVATIONS` record holds one of two things, distinguished by the record's `bintype` (`snap/bindata.h`):

- **`SURVDATA`** - a `survdata` (`src/snaplib/snapdata/survdata.h`): a 16-field fixed-width
  header (via `SURVDATA_DISK_FIELDS`, same mechanism as above), followed by a raw fixed-stride
  array of per-observation records. Which of three variant layouts that array holds - `obsdata`,
  `vecdata`, or `pntdata`, all sharing a common `trgtdata` prefix - is decided by the header's
  own `format`/`obssize` fields, not by anything table-driven; see `survdata.h` for the three
  structs' exact fields. After the observation array come a `classdata` array and a `syserrdata`
  array (both sized by the header's `nclass`/`nsyserr`, shared across every observation in the
  record, not one per observation), and then, only for vector-format records with `ncvr > 0`:
  three flat, packed lower-triangular covariance matrices in fixed order (`cvr`, `calccvr`,
  `rescvr` - `util/symmatrx.h`'s `ltmat`), each holding `ncvr*(ncvr+1)/2` doubles.
- **`NOTEDATA`** - a free-text note attached to an observation (`#note` command in a data file,
  `save_note`/`snap/notedata.cpp`), not a struct at all: one flag byte (`' '` if this note
  continues the previous one, `'\n'` if it starts a new one), the note's text verbatim, then a
  trailing `'\n'` and a NUL.

`DATA_FILES`
-------------

File paths and the context strings used to reconstruct relative file locations
(`context_definition`/`recreate_context`, `util/fileutil.cpp`) are always written with `/` as the
separator on both platforms (`portable_path`/`dump_filepath`), regardless of the writing
platform's native separator. Reading is separator-agnostic (`reload_string`), so this is a
write-side-only normalization.

Verification
-------------

`src/test/binroundtrip/` is a test-only tool (not a shipped program) that reloads every section
of a `.bin` file into real in-memory objects using this codebase's own `reload_*` functions,
then either re-dumps it as a new `.bin` (round-trip mode) or as human-readable text, one value
per line (`--dump` mode) - the latter for a tolerance-aware comparison that can tell a real
regression apart from ordinary floating-point differences between compilers, which a raw byte
comparison can't.

`regression_tests/binroundtrip/run_test.py` runs two comparisons against a committed golden
`.bin` fixture: a fresh `snap` run's output vs. golden (via `--dump` and `compare_dump.py`,
tolerant of floating-point noise - including a per-field tolerance override for `vecdata.vsres`,
a standardised residual that can amplify ordinary rounding noise through a near-singular
covariance Cholesky decomposition, see the override's own comment in `compare_dump.py`) and a
round trip of the golden copy vs. itself (a plain byte comparison - no floating-point
recomputation involved, so no tolerance needed). Both are run on Linux by `testall.pl`, and have
been run and passed on a Windows build against the same, Linux-produced golden fixture - proving
the two platforms agree, not just that each platform agrees with itself. The
golden fixture (`regression_tests/binroundtrip/in/binroundtrip.snp`/`.crd`/`.dat`/`.bin`)
exercises every section listed above, including a `NOTEDATA` record via one `#note` line.

Not yet audited
-----------------

This portability effort covers every section written by a real `snap` run, but one thing is
worth calling out rather than silently implying a blanket guarantee: `CHOLESKI_DECOMPOSITION`/
`FULL_COVARIANCE` (`dump_bltmatrix`/`dump_bltmatrix_dense`, `util/bltmatrx.cpp`) are proven
correct by the round-trip and `--dump` verification above, but unlike `station`/
`rfTransformation`/`param`/`survdata`, their write function isn't table-driven with a
compile-time contiguity check - a future field added to `bltmatrix` wouldn't be caught by a
`static_assert` the way it would be for the other four.
