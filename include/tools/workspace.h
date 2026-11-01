#pragma once
#include <filesystem>
#include <string>
#include <string_view>

class Workspace {
public:

    explicit Workspace(std::filesystem::path root);

    const std::filesystem::path& root() const noexcept;

    bool resolve(std::string_view user_path,
                 std::filesystem::path* out,
                 std::string* reason) const;

    bool check_symlinks(const std::filesystem::path& resolved,
                        std::string* reason) const;

private:
    std::filesystem::path root_;
};
