#pragma once

#include <string>
#include <random>

class GATestHelpers
{
    public:
        static std::string getRandomString(size_t numberOfCharacters);
        static std::string get80CharsString();
        static std::string get40CharsString();
        static std::string get32CharsString();
};
