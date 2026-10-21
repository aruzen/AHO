#pragma once
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace aho_test {
    class TempDirectory {
        inline static std::atomic<unsigned> SEQUENCE{0};

    public:
        std::filesystem::path path;
        TempDirectory() {
#ifdef _WIN32
            const auto process = _getpid();
#else
            const auto process = getpid();
#endif
            for (unsigned attempt = 0; attempt < 100; ++attempt) {
                const auto* configured_root = std::getenv("AHO_TEST_TMPDIR");
                const auto root = configured_root ? std::filesystem::path(configured_root) : std::filesystem::temp_directory_path();
                path = root /
                       ("aho-test-" + std::to_string(process) + "-" + std::to_string(SEQUENCE++));
                if (std::filesystem::create_directory(path))
                    return;
            }
            throw std::runtime_error("Could not create an isolated AHO test directory");
        }
        ~TempDirectory() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;
    };
} // namespace aho_test
