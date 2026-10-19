#pragma once
#include <filesystem>
#include <random>
#include <stdexcept>
namespace vsl_test {
    class TemporaryDirectory {
        std::filesystem::path path_;

    public:
        TemporaryDirectory() {
            std::random_device random;
            for (int attempt = 0; attempt < 100; ++attempt) {
                auto candidate =
                    std::filesystem::temp_directory_path() / ("aho-vsl-test-" + std::to_string(random()));
                if (std::filesystem::create_directory(candidate)) {
                    path_ = candidate;
                    return;
                }
            }
            throw std::runtime_error("Cannot create unique test directory");
        }
        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        ~TemporaryDirectory() {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
        const std::filesystem::path& path() const {
            return path_;
        }
    };
} // namespace vsl_test
