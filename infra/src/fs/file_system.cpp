#include "dawn/infra/fs/file_system.h"

#include <fstream>
#include <system_error>

namespace dawn::infra::fs {

bool ensure_directory(const std::filesystem::path& path, std::string* error) {
    std::error_code ec;
    if (path.empty()) {
        return true;
    }

    if (std::filesystem::exists(path, ec)) {
        if (ec) {
            if (error) {
                *error = "failed to check path existence: " + ec.message();
            }
            return false;
        }
        ec.clear();
        const bool isDir = std::filesystem::is_directory(path, ec);
        if (ec) {
            if (error) {
                *error = "failed to check path type: " + ec.message();
            }
            return false;
        }
        if (isDir) {
            return true;
        }
        if (error) {
            *error = "path exists and is not a directory: " + path.string();
        }
        return false;
    }

    if (ec) {
        if (error) {
            *error = "failed to check path existence: " + ec.message();
        }
        return false;
    }

    ec.clear();
    if (std::filesystem::create_directories(path, ec)) {
        return true;
    }

    if (!ec) {
        ec.clear();
        const bool exists = std::filesystem::exists(path, ec);
        if (!ec && exists) {
            ec.clear();
            const bool isDir = std::filesystem::is_directory(path, ec);
            if (!ec && isDir) {
                return true;
            }
        }
    }

    if (error) {
        *error = ec ? ec.message() : "failed to create directory";
    }
    return false;
}

bool ensure_parent_directory(const std::filesystem::path& path, std::string* error) {
    const auto parent = path.parent_path();
    if (parent.empty()) {
        return true;
    }
    return ensure_directory(parent, error);
}

std::uintmax_t file_size(const std::filesystem::path& path, std::string* error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return 0;
    }
    if (error) {
        error->clear();
    }
    return size;
}

bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string* error) {
    if (!ensure_parent_directory(path, error)) {
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for writing: " + path.string();
        }
        return false;
    }

    stream << text;
    if (!stream.good()) {
        if (error) {
            *error = "failed to write file: " + path.string();
        }
        return false;
    }

    return true;
}

bool write_binary_file(const std::filesystem::path& path, const std::string& data, std::string* error) {
    if (!ensure_parent_directory(path, error)) {
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for writing: " + path.string();
        }
        return false;
    }

    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream.good()) {
        if (error) {
            *error = "failed to write file: " + path.string();
        }
        return false;
    }

    return true;
}

bool append_binary_file(const std::filesystem::path& path, const std::string& data, std::string* error) {
    if (!ensure_parent_directory(path, error)) {
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::app);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for appending: " + path.string();
        }
        return false;
    }

    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream.good()) {
        if (error) {
            *error = "failed to append file: " + path.string();
        }
        return false;
    }

    return true;
}

bool read_text_file(const std::filesystem::path& path, std::string* text, std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for reading: " + path.string();
        }
        return false;
    }

    std::string contents;
    stream.seekg(0, std::ios::end);
    contents.resize(static_cast<std::size_t>(stream.tellg()));
    stream.seekg(0, std::ios::beg);
    stream.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!stream.good() && !stream.eof()) {
        if (error) {
            *error = "failed to read file: " + path.string();
        }
        return false;
    }

    if (text) {
        *text = std::move(contents);
    }
    return true;
}

std::vector<std::filesystem::path> list_files(const std::filesystem::path& path, const std::string& extension) {
    std::vector<std::filesystem::path> result;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!extension.empty() && entry.path().extension() != extension) {
            continue;
        }
        result.push_back(entry.path());
    }
    return result;
}

} // namespace dawn::infra::fs
