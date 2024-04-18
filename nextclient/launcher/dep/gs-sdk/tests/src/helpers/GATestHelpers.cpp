#include "GATestHelpers.h"

std::string GATestHelpers::getRandomString(size_t numberOfCharacters)
{
    auto letters = "abcdefghijklmpqrstuvwxyzABCDEFGHIJKLMPQRSTUVWXYZ0123456789";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(letters));

    std::string ret = "";
    for (int i=0; i<numberOfCharacters; i++)
    {
        ret.push_back( letters[dis(gen)] );
    }
    return ret;
}

std::string GATestHelpers::get80CharsString()
{
    std::string ret = "80charstring123456789012345678901234567840charstring1234567890123456789012345678";
    return ret;
}

std::string GATestHelpers::get40CharsString()
{
    std::string ret = "40charstring1234567890123456789012345678";
    return ret;
}

std::string GATestHelpers::get32CharsString()
{
    std::string ret = "32charstring12345678901234567890";
    return ret;
}
