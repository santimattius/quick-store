// Golden file generator for quick-store byte-compat tests.
// Uses MMKV v2.4.0 to produce reference files that are committed to the repo.
//
// This binary is NOT run in CI — golden files are pre-generated and committed.
// See core/tests/golden/README.md for build and run instructions.

#include "MMKV/MMKV.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void init_and_run(const fs::path& dir,
                         const std::function<void(MMKV*)>& fn) {
    fs::create_directories(dir);
    std::string dir_str = dir.string();
    MMKV::initializeMMKV(dir_str);
    auto* kv = MMKV::mmkvWithID("mmkv", MMKV_SINGLE_PROCESS, nullptr, &dir_str);
    fn(kv);
    kv->close();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <output_golden_dir>\n";
        std::cerr << "  output_golden_dir should be core/tests/golden/ in the repo.\n";
        return 1;
    }
    const fs::path out(argv[1]);

    init_and_run(out / "single_bool", [](MMKV* kv) {
        kv->set(true, "b");
    });

    init_and_run(out / "negative_int32", [](MMKV* kv) {
        kv->set(int32_t(-1), "i");
    });

    init_and_run(out / "negative_int64", [](MMKV* kv) {
        kv->set(std::numeric_limits<int64_t>::min(), "l");
    });

    init_and_run(out / "float_double", [](MMKV* kv) {
        kv->set(std::numeric_limits<float>::quiet_NaN(), "f");
        kv->set(-std::numeric_limits<double>::infinity(), "d");
    });

    init_and_run(out / "string_unicode", [](MMKV* kv) {
        // "hello 世界 🌍" in UTF-8
        kv->set(std::string("hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8c\x8d"), "s");
    });

    init_and_run(out / "bytes_large", [](MMKV* kv) {
        std::vector<uint8_t> zeros(4096, 0);
        kv->set(zeros, "raw");
    });

    init_and_run(out / "overwrite_same_key", [](MMKV* kv) {
        kv->set(int32_t(3),  "k");
        kv->set(int32_t(42), "k");
        kv->set(int32_t(99), "k");
    });

    init_and_run(out / "tombstone", [](MMKV* kv) {
        kv->set(int32_t(7), "k");
        kv->removeValueForKey("k");
    });

    init_and_run(out / "all_types", [](MMKV* kv) {
        kv->set(false,                                         "bool_f");
        kv->set(true,                                          "bool_t");
        kv->set(int32_t(-42),                                  "i32");
        kv->set(int64_t(-1),                                   "i64");
        kv->set(uint64_t(std::numeric_limits<uint64_t>::max()), "u64");
        kv->set(3.14f,                                         "flt");
        kv->set(2.718281828,                                   "dbl");
        kv->set(std::string("utf8"),                           "str");
        kv->set(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}, "raw");
    });

    init_and_run(out / "many_keys", [](MMKV* kv) {
        for (int i = 0; i < 1000; ++i) {
            kv->set(std::string("val_") + std::to_string(i),
                    std::string("key_") + std::to_string(i));
        }
    });

    // recovery_corrupt: write a=1, b=2, force sync to commit lastConfirmed,
    // then write c=3 and corrupt the last append in the data file.
    {
        auto dir = out / "recovery_corrupt";
        fs::create_directories(dir);
        std::string dir_str = dir.string();
        MMKV::initializeMMKV(dir_str);
        auto* kv = MMKV::mmkvWithID("mmkv", MMKV_SINGLE_PROCESS, nullptr, &dir_str);
        kv->set(int32_t(1), "a");
        kv->set(int32_t(2), "b");
        kv->sync(); // commits lastConfirmedMetaInfo = {a=1, b=2}
        kv->set(int32_t(3), "c"); // updates m_actualSize + m_crcDigest
        kv->close();
        // Corrupt the last 3 bytes of the data file to simulate a crash
        // after append but before the CRC commit was durable.
        std::string data_path = (dir / "mmkv").string();
        std::fstream f(data_path, std::ios::in | std::ios::out | std::ios::binary);
        if (f) {
            f.seekp(-3, std::ios::end);
            char garbage[] = {'\xFF', '\xFF', '\xFF'};
            f.write(garbage, 3);
        }
        // Result: checkDataValid falls back to lastConfirmed → {a=1, b=2}, no "c"
    }

    std::cout << "Golden files written to: " << out << "\n";
    std::cout << "Commit them with: git add core/tests/golden/ && git commit\n";
    return 0;
}
