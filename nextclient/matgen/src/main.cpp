#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "BakeEdges.h"
#include "Check.h"
#include "UvTemplate.h"

namespace
{
    void PrintUsage()
    {
        std::printf(
            "matgen - authoring helpers for NextClient weapon materials\n"
            "\n"
            "  matgen uv <model.mdl> [--out <dir>] [--size <pixels>] [--skin <family>]\n"
            "      Writes one PNG per texture of the model with its own unwrap drawn over it,\n"
            "      so a 2D editor is enough and nothing has to be decompiled.\n"
            "\n"
            "  matgen check <model.mat> [--game <dir>]\n"
            "      Parses the document, checks it against the program it names, and checks that\n"
            "      every image it references is there, decodable and within the client's limit.\n"
            "      Prints what the client would print. Paths inside a material are relative to\n"
            "      the mod directory, which is --game, defaulting to the current directory.\n"
            "\n"
            "  matgen bake edges <model.mdl> [--out <dir>] [--size <pixels>] [--skin <family>]\n"
            "      Bakes a wear threshold map from the model's own geometry: where the surface\n"
            "      turns sharply it wears first. No supplied mask can do this, because it is\n"
            "      about this model's edges rather than about a pattern.\n"
        );
    }

    const char* ArgumentAfter(const std::vector<std::string>& arguments, const char* flag)
    {
        for (size_t i = 0; i + 1 < arguments.size(); i++)
        {
            if (arguments[i] == flag)
            {
                return arguments[i + 1].c_str();
            }
        }

        return nullptr;
    }

    int IntegerAfter(const std::vector<std::string>& arguments, const char* flag, int fallback)
    {
        const char* value = ArgumentAfter(arguments, flag);

        return value != nullptr ? std::atoi(value) : fallback;
    }

    // The first argument that is not a flag and is not the value of one.
    const char* Positional(const std::vector<std::string>& arguments, size_t index)
    {
        size_t seen = 0;

        for (size_t i = 0; i < arguments.size(); i++)
        {
            if (arguments[i].rfind("--", 0) == 0)
            {
                i++;
                continue;
            }

            if (seen == index)
            {
                return arguments[i].c_str();
            }

            seen++;
        }

        return nullptr;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> arguments(argv + 2, argv + argc);

    // "bake edges" reads as two words and is one command; the subject is stripped here so
    // the rest of the parsing does not have to know which commands have one.
    if (command == "bake")
    {
        const char* subject = Positional(arguments, 0);

        if (subject == nullptr || std::string(subject) != "edges")
        {
            PrintUsage();
            return 1;
        }

        arguments.erase(std::find(arguments.begin(), arguments.end(), "edges"));
        command = "bake edges";
    }

    if (command == "uv" || command == "bake edges")
    {
        const char* model = Positional(arguments, 0);

        if (model == nullptr)
        {
            PrintUsage();
            return 1;
        }

        const char* out = ArgumentAfter(arguments, "--out");
        int size = IntegerAfter(arguments, "--size", 0);
        int skin = IntegerAfter(arguments, "--skin", 0);

        if (command == "bake edges")
        {
            matgen::BakeOptions options;
            options.out_dir = out != nullptr ? out : "";
            options.size = size;
            options.skin_family = skin;

            return matgen::RunBakeEdges(model, options);
        }

        matgen::UvOptions options;
        options.out_dir = out != nullptr ? out : "";
        options.size = size;
        options.skin_family = skin;

        return matgen::RunUv(model, options);
    }

    if (command == "check")
    {
        const char* material = Positional(arguments, 0);

        if (material == nullptr)
        {
            PrintUsage();
            return 1;
        }

        matgen::CheckOptions options;
        options.game_dir = ArgumentAfter(arguments, "--game") != nullptr ? ArgumentAfter(arguments, "--game") : "";

        return matgen::RunCheck(material, options);
    }

    PrintUsage();

    return 1;
}
