#include "graphics/render/shader_includes.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace
{
    constexpr size_t kMaximumIncludeDepth = 16;
    constexpr size_t kMaximumExpandedBytes = 1024 * 1024;

    std::string_view Trim(std::string_view text)
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        {
            text.remove_suffix(1);
        }

        return text;
    }

    bool IsRelativePath(std::string_view path)
    {
        if (path.empty())
        {
            return false;
        }

        for (char character : path)
        {
            if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '-' && character != '.' &&
                character != '/')
            {
                return false;
            }
        }

        while (!path.empty())
        {
            size_t slash = path.find('/');
            std::string_view segment = path.substr(0, slash);

            if (segment.empty() || segment == "." || segment == "..")
            {
                return false;
            }

            if (slash == std::string_view::npos)
            {
                return true;
            }

            path.remove_prefix(slash + 1);
        }

        return false;
    }

    std::string RemoveComments(std::string_view line, bool& in_block)
    {
        std::string result;
        bool in_string = false;

        for (size_t index = 0; index < line.size(); index++)
        {
            char current = line[index];
            char next = index + 1 < line.size() ? line[index + 1] : '\0';

            if (in_block)
            {
                if (current == '*' && next == '/')
                {
                    in_block = false;
                    result += ' ';
                    index++;
                }

                continue;
            }

            if (!in_string && current == '/' && next == '/')
            {
                break;
            }

            if (!in_string && current == '/' && next == '*')
            {
                in_block = true;
                result += ' ';
                index++;
                continue;
            }

            if (current == '"' && (index == 0 || line[index - 1] != '\\'))
            {
                in_string = !in_string;
            }

            result += current;
        }

        return result;
    }

    class ShaderExpansion
    {
    private:
        const render::ShaderSourceLoader_t& loader_;
        std::vector<std::string> stack_;
        std::string output_;
        std::string error_;

    public:
        explicit ShaderExpansion(const render::ShaderSourceLoader_t& loader) :
            loader_(loader)
        {}

        ncl_utils::ResultT<std::string> Expand(std::string_view name)
        {
            if (!ReadFile(std::string(name)))
            {
                return ncl_utils::ResultError(std::move(error_));
            }

            return std::move(output_);
        }

    private:
        bool Fail(const std::string& name, const char* reason)
        {
            error_ = name + ": " + reason;
            return false;
        }

        bool Append(const std::string& name, std::string_view text)
        {
            if (text.size() > kMaximumExpandedBytes - output_.size())
            {
                return Fail(name, "expanded shader exceeds 1 MiB");
            }

            output_.append(text);
            return true;
        }

        bool ReadFile(const std::string& name)
        {
            if (!IsRelativePath(name))
            {
                return Fail(name, "invalid relative shader path");
            }

            if (stack_.size() >= kMaximumIncludeDepth)
            {
                return Fail(name, "shader include depth exceeds 16");
            }

            if (std::find(stack_.begin(), stack_.end(), name) != stack_.end())
            {
                return Fail(name, "recursive shader include");
            }

            std::string source;

            if (!loader_(name, source))
            {
                return Fail(name, "shader source could not be loaded");
            }

            if (source.size() > kMaximumExpandedBytes)
            {
                return Fail(name, "shader source exceeds 1 MiB");
            }

            stack_.push_back(name);
            bool in_block = false;

            for (size_t offset = 0; offset < source.size();)
            {
                size_t end = source.find('\n', offset);
                size_t count = end == std::string::npos ? source.size() - offset : end - offset + 1;
                std::string_view original(source.data() + offset, count);
                offset += count;
                bool was_in_block = in_block;
                std::string visible = RemoveComments(original, in_block);
                std::string_view directive = Trim(visible);

                if (directive.starts_with('#'))
                {
                    directive = Trim(directive.substr(1));
                    size_t keyword_size = directive.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
                    std::string_view keyword = directive.substr(0, keyword_size);

                    if (keyword == "version" && stack_.size() > 1)
                    {
                        return Fail(name, "included shader must not declare #version");
                    }

                    if (keyword == "include")
                    {
                        std::string_view operand = Trim(directive.substr(keyword.size()));

                        if (operand.size() < 2 || operand.front() != '"' || operand.find('"', 1) != operand.size() - 1)
                        {
                            return Fail(name, "expected #include followed by one quoted relative path");
                        }

                        std::string_view relative = operand.substr(1, operand.size() - 2);

                        if (!IsRelativePath(relative))
                        {
                            return Fail(name, "invalid shader include path");
                        }

                        size_t slash = name.rfind('/');
                        std::string dependency =
                            slash == std::string::npos ? std::string(relative) : name.substr(0, slash + 1) + std::string(relative);

                        if (was_in_block && !Append(name, "*/\n"))
                        {
                            return false;
                        }

                        if (!ReadFile(dependency))
                        {
                            return false;
                        }

                        if (!output_.empty() && output_.back() != '\n' && original.ends_with('\n') && !Append(name, "\n"))
                        {
                            return false;
                        }

                        if (in_block && !Append(name, "/*\n"))
                        {
                            return false;
                        }

                        continue;
                    }
                }

                if (!Append(name, original))
                {
                    return false;
                }
            }

            stack_.pop_back();
            return true;
        }
    };
} // namespace

ncl_utils::ResultT<std::string> render::ShaderIncludes_Expand(std::string_view name, const ShaderSourceLoader_t& loader)
{
    return ShaderExpansion(loader).Expand(name);
}
