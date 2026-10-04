#pragma once
#include <array>
#include <filesystem>
#include <exception>
#include <initializer_list>
#include <string>
#include <vector>
#include <tao/json.hpp>

namespace plugins
{
    using Json = tao::json::value;
    struct Relation
    {
        std::string id, range, reason;
    };
    struct Manifest
    {
        std::string id, name, author, description, version, sdk;
        Json translations = tao::json::empty_object;
        unsigned abi{}, api{}, revision{};
        uint32_t permissions{};
        std::vector<Relation> required, conflicts, before, after;
    };
    struct Item
    {
        std::string file, hash, error, warning;
        Manifest manifest;
        bool approved{}, enabled{}, running{};
        bool consent{};
    };
    Json parse(const std::string& text);
    int64_t integer(const Json& value, int64_t minimum, int64_t maximum);
    Manifest manifest(const std::string& text);
    std::string pe_manifest(const std::vector<unsigned char>& bytes);
    std::string sha256(const std::vector<unsigned char>& bytes);
    bool matches(const std::string& version, const std::string& range);
    Json display_name(const Item& item);
    uint32_t permission_mask(const Json& permissions);
    Json permission_names(uint32_t mask);
    // Internal diagnostics stay structured until GameUI resolves localization tokens.
    Json message_value(const char* token, std::initializer_list<Json> args = {});
    std::string message(const char* token, std::initializer_list<Json> args = {});
    std::string error_message(const std::exception& error);
    void append_message(std::string& messages, const std::string& next);
    // Validate only enabled entries; caller supplies the intended order.
    std::string validate(const std::vector<Item>& items);
    std::vector<size_t> recommend(const std::vector<Item>& items, std::string& warning);
    std::string ordering_warnings(const std::vector<Item>& items);
} // namespace plugins
