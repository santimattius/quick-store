# Golden Files — quick-store Fase 0

## MMKV Version
- **MMKV**: v2.4.0 (tag `v2.4.0`)
- **Format version**: 4 (`MMKVVersionFlag = 4`)
- **Platform**: POSIX (macOS / Linux host)

## How to generate

```bash
# 1. Build MMKV for POSIX
git clone --branch v2.4.0 https://github.com/Tencent/MMKV.git /tmp/mmkv
cd /tmp/mmkv/POSIX && mkdir build && cd build
cmake .. && make -j

# 2. Build the generator (from quick-store repo root)
mkdir -p core/tests/golden/_generator/build
cd core/tests/golden/_generator/build
cmake -DMMKV_DIR=/tmp/mmkv/POSIX/build ..
make -j

# 3. Run
./generate_golden /path/to/quick-store/core/tests/golden

# 4. Commit the generated binaries
git add core/tests/golden/
git commit -m "chore: add golden files (MMKV v2.4.0)"
```

## Scenarios

| Directory | Keys | Types | Special |
|-----------|------|-------|---------|
| `single_bool/` | b=true | bool | baseline |
| `negative_int32/` | i=-1 | int32 | 10-byte varint |
| `negative_int64/` | l=INT64_MIN | int64 | 10-byte varint |
| `float_double/` | f=NaN, d=-Inf | float, double | IEEE-754 edge |
| `string_unicode/` | s="hello 世界 🌍" | string | UTF-8 multibyte |
| `bytes_large/` | raw=4096×0x00 | bytes | large value |
| `overwrite_same_key/` | k=3→42→99 | int32 | last write wins |
| `tombstone/` | k=7 then deleted | int32 | valueLen=0 |
| `all_types/` | one per type | all | comprehensive |
| `many_keys/` | key_0..key_999 | string | 1000 entries |
| `recovery_corrupt/` | a=1, b=2, c=3 (corrupted) | int32 | lastConfirmed path |

## Notes on randomness

**ItemSizeHolder** and **IV** are random on each write. Re-running the generator
produces files with different bytes at those positions. This is expected and OK —
semantic content (values, allKeys, count) is stable; byte-exact comparison
must skip those ranges (see §13.4 of the spec).

The tests in `test_golden_files.cpp` validate semantics only, not raw bytes.
