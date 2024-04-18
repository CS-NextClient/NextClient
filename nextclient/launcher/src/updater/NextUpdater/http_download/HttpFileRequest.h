#pragma once

#include <string>

class HttpFileRequest
{
    std::string filename_;
    std::string hash_;
    size_t size_{};

public:
    explicit HttpFileRequest(std::string filename, std::string hash, size_t size) :
        filename_(std::move(filename)),
        hash_(std::move(hash)),
        size_(size)
    { }

    bool operator==(const HttpFileRequest& rhs) const
    {
        return filename_ == rhs.filename_ &&
               hash_ == rhs.hash_ &&
               size_ == rhs.size_;
    }

    bool operator!=(const HttpFileRequest& rhs) const
    {
        return !(rhs == *this);
    }

    [[nodiscard]] const std::string& get_filename() const { return filename_; }
    [[nodiscard]] const std::string& get_hash() const { return hash_; }
    [[nodiscard]] size_t get_size() const { return size_; }
};