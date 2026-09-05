#include "graphics/preview/WavefrontParser.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstring>

namespace
{
    constexpr std::string_view kBlank = " \t\r";

    std::string_view NextLine(std::string_view& text)
    {
        size_t newline = text.find('\n');
        std::string_view line = text.substr(0, newline);
        text.remove_prefix(newline == std::string_view::npos ? text.size() : newline + 1);

        return line;
    }

    // The next blank-separated word of a line, empty at its end.
    std::string_view NextWord(std::string_view& line)
    {
        size_t start = line.find_first_not_of(kBlank);
        if (start == std::string_view::npos)
        {
            line = {};
            return {};
        }

        line.remove_prefix(start);

        size_t end = line.find_first_of(kBlank);
        std::string_view word = line.substr(0, end);
        line.remove_prefix(end == std::string_view::npos ? line.size() : end);

        return word;
    }

    bool ParseFloat(std::string_view word, float& out)
    {
        std::from_chars_result result = std::from_chars(word.data(), word.data() + word.size(), out);

        return result.ec == std::errc() && result.ptr == word.data() + word.size();
    }

    bool ParseInt(std::string_view word, int& out)
    {
        std::from_chars_result result = std::from_chars(word.data(), word.data() + word.size(), out);

        return result.ec == std::errc() && result.ptr == word.data() + word.size();
    }

    // A vector line: as many components as asked for, the rest of the line left alone.
    bool ParseComponents(std::string_view line, float* out, int count)
    {
        for (int i = 0; i < count; i++)
        {
            if (!ParseFloat(NextWord(line), out[i]))
            {
                return false;
            }
        }

        return true;
    }

    // An index of a face corner, 1-based in the file and negative for counting back from
    // the end; -1 comes back for a field the corner leaves out.
    bool ParseIndex(std::string_view field, size_t count, int& out)
    {
        if (field.empty())
        {
            out = -1;
            return true;
        }

        int index = 0;
        if (!ParseInt(field, index) || index == 0)
        {
            return false;
        }

        index = index > 0 ? index - 1 : static_cast<int>(count) + index;

        if (index < 0 || index >= static_cast<int>(count))
        {
            return false;
        }

        out = index;
        return true;
    }

    struct Corner
    {
        int position;
        int texcoord;
        int normal;
    };

    // v, v/vt, v/vt/vn or v//vn
    bool ParseCorner(std::string_view word, size_t positions, size_t texcoords, size_t normals, Corner& out)
    {
        std::array<std::string_view, 3> fields;
        int field_count = 0;

        while (field_count < 3)
        {
            size_t slash = word.find('/');
            fields[field_count++] = word.substr(0, slash);

            if (slash == std::string_view::npos)
            {
                break;
            }

            word.remove_prefix(slash + 1);
        }

        if (fields[0].empty())
        {
            return false;
        }

        return ParseIndex(fields[0], positions, out.position) && ParseIndex(fields[1], texcoords, out.texcoord) &&
               ParseIndex(fields[2], normals, out.normal);
    }

    // The normal of a counter-clockwise face. A face without area gets one all the same.
    void FaceNormal(const float* a, const float* b, const float* c, float* out)
    {
        float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        float ac[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};

        out[0] = ab[1] * ac[2] - ab[2] * ac[1];
        out[1] = ab[2] * ac[0] - ab[0] * ac[2];
        out[2] = ab[0] * ac[1] - ab[1] * ac[0];

        float length = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
        if (length <= 0.0f)
        {
            out[0] = 0.0f;
            out[1] = 0.0f;
            out[2] = 1.0f;
            return;
        }

        out[0] /= length;
        out[1] /= length;
        out[2] /= length;
    }

    WavefrontBatch& BatchFor(std::vector<WavefrontBatch>& batches, std::string_view material)
    {
        for (WavefrontBatch& batch : batches)
        {
            if (batch.material == material)
            {
                return batch;
            }
        }

        batches.push_back(WavefrontBatch{std::string(material), {}});

        return batches.back();
    }
} // namespace

WavefrontMesh Wavefront_Parse(std::string_view text)
{
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> texcoords;
    std::vector<std::array<float, 3>> normals;
    std::vector<Corner> corners;

    WavefrontMesh mesh{};
    std::string material;

    while (!text.empty())
    {
        std::string_view line = NextLine(text);
        std::string_view keyword = NextWord(line);

        if (keyword == "v")
        {
            std::array<float, 3> position{};
            if (ParseComponents(line, position.data(), 3))
            {
                positions.push_back(position);
            }
        }
        else if (keyword == "vt")
        {
            std::array<float, 2> texcoord{};
            if (ParseComponents(line, texcoord.data(), 2))
            {
                texcoords.push_back(texcoord);
            }
        }
        else if (keyword == "vn")
        {
            std::array<float, 3> normal{};
            if (ParseComponents(line, normal.data(), 3))
            {
                normals.push_back(normal);
            }
        }
        else if (keyword == "usemtl")
        {
            material = NextWord(line);
        }
        else if (keyword == "f")
        {
            corners.clear();
            bool valid = true;

            for (std::string_view word = NextWord(line); !word.empty(); word = NextWord(line))
            {
                Corner corner{};
                valid = valid && ParseCorner(word, positions.size(), texcoords.size(), normals.size(), corner);
                corners.push_back(corner);
            }

            if (!valid || corners.size() < 3)
            {
                mesh.skipped_faces++;
                continue;
            }

            float face_normal[3];
            FaceNormal(positions[corners[0].position].data(), positions[corners[1].position].data(), positions[corners[2].position].data(), face_normal);

            WavefrontBatch& batch = BatchFor(mesh.batches, material);

            for (size_t i = 1; i + 1 < corners.size(); i++)
            {
                for (const Corner& corner : {corners[0], corners[i], corners[i + 1]})
                {
                    WavefrontVertex vertex{};
                    std::memcpy(vertex.position, positions[corner.position].data(), sizeof(vertex.position));

                    const float* normal = corner.normal >= 0 ? normals[corner.normal].data() : face_normal;
                    std::memcpy(vertex.normal, normal, sizeof(vertex.normal));

                    if (corner.texcoord >= 0)
                    {
                        vertex.s = texcoords[corner.texcoord][0];
                        vertex.t = 1.0f - texcoords[corner.texcoord][1];
                    }

                    batch.vertices.push_back(vertex);
                }
            }
        }
    }

    return mesh;
}
