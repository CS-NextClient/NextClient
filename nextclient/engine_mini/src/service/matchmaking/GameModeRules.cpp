#include "GameModeRules.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <utility>

#include <easylogging++.h>
#include <strtools.h>
#include <tao/json.hpp>
#include <tao/json/events/limit_nesting_depth.hpp>

#include "common/utf8.h"
#include "service/matchmaking/master/MasterServerEntry.h"

namespace
{
    // The parser recurses once per nesting level; a rules file nests four deep: document, rules, rule, list
    constexpr size_t kMaxJsonNestingDepth = 8;

    constexpr char kWordSeparator = ' ';
    // a dot between two word characters stays in the word, so domain names and version numbers are single words
    constexpr char kWordJoiner = '.';
    constexpr char kKeywordWildcard = '*';

    struct Utf8SequenceForm
    {
        // the lead byte bits that tell the form, and the value they take
        uint8_t lead_mask{};
        uint8_t lead_bits{};
        size_t length{};
    };

    constexpr uint8_t kUtf8TwoByteLeadBits = 0xC0;

    // sequences of two, three and four bytes start with 110xxxxx, 1110xxxx and 11110xxx
    constexpr Utf8SequenceForm kUtf8SequenceForms[] = {
        {0xE0, kUtf8TwoByteLeadBits, 2},
        {0xF0, 0xE0, 3},
        {0xF8, 0xF0, 4},
    };

    constexpr char32_t kMaxAsciiChar = 0x7F;
    constexpr char32_t kMaxTwoByteChar = 0x7FF;
    constexpr char32_t kReplacementChar = 0xFFFD;

    struct CharRange
    {
        char32_t first{};
        char32_t last{};
    };

    // ASCII digits and letters, then whole blocks: the Latin-1 Supplement letters, Latin Extended-A and -B, Greek and
    // Cyrillic, the few signs among them included
    constexpr CharRange kWordCharRanges[] = {
        {U'0', U'9'},
        {U'A', U'Z'},
        {U'a', U'z'},
        {0x00C0, 0x024F},
        {0x0370, 0x04FF},
    };

    // the signs among the Latin-1 Supplement letters
    constexpr char32_t kMultiplicationSign = 0x00D7;
    constexpr char32_t kDivisionSign = 0x00F7;

    // a word character is appended to the words as UTF-8 of at most two bytes
    static_assert(std::ranges::all_of(kWordCharRanges, [](const CharRange& range) { return range.last <= kMaxTwoByteChar; }));

    struct CaseRange
    {
        char32_t first_capital{};
        char32_t last_capital{};
        // distance from a capital letter to its small letter
        char32_t offset{};
    };

    // capital letters of ASCII, the Latin-1 Supplement and basic Cyrillic
    constexpr CaseRange kCaseRanges[] = {
        {U'A', U'Z', 0x20},
        {0x00C0, 0x00DE, 0x20},
        {0x0400, 0x040F, 0x50},
        {0x0410, 0x042F, 0x20},
    };

    // Decodes the character at pos and moves pos past it; a byte that starts no complete sequence decodes as U+FFFD
    // and moves pos by one
    char32_t DecodeUtf8(std::string_view text, size_t& pos)
    {
        uint8_t lead = static_cast<uint8_t>(text[pos]);
        pos++;

        if (lead <= kMaxAsciiChar)
        {
            return lead;
        }

        for (const Utf8SequenceForm& form : kUtf8SequenceForms)
        {
            if ((lead & form.lead_mask) != form.lead_bits)
            {
                continue;
            }

            size_t end = pos + form.length - 1;
            char32_t cp = lead & static_cast<uint8_t>(~form.lead_mask);

            for (size_t i = pos; i < end; i++)
            {
                uint8_t byte = i < text.size() ? static_cast<uint8_t>(text[i]) : 0;

                if ((byte & kUtf8ContinuationMask) != kUtf8ContinuationBits)
                {
                    return kReplacementChar;
                }

                cp = (cp << kUtf8ContinuationPayloadBits) | (byte & static_cast<uint8_t>(~kUtf8ContinuationMask));
            }

            pos = end;

            return cp;
        }

        return kReplacementChar;
    }

    void AppendUtf8(char32_t cp, std::string& out)
    {
        if (cp <= kMaxAsciiChar)
        {
            out += static_cast<char>(cp);
            return;
        }

        out += static_cast<char>(kUtf8TwoByteLeadBits | (cp >> kUtf8ContinuationPayloadBits));
        out += static_cast<char>(kUtf8ContinuationBits | (cp & static_cast<uint8_t>(~kUtf8ContinuationMask)));
    }

    bool IsWordChar(char32_t cp)
    {
        if (cp == kMultiplicationSign || cp == kDivisionSign)
        {
            return false;
        }

        return std::ranges::any_of(kWordCharRanges, [cp](const CharRange& range) { return cp >= range.first && cp <= range.last; });
    }

    char32_t FoldCase(char32_t cp)
    {
        for (const CaseRange& range : kCaseRanges)
        {
            if (cp >= range.first_capital && cp <= range.last_capital && cp != kMultiplicationSign)
            {
                return cp + range.offset;
            }
        }

        return cp;
    }

    // The lower-case words of text, each preceded and followed by one space
    std::string SplitWords(std::string_view text)
    {
        std::string words(1, kWordSeparator);
        size_t pos = 0;

        while (pos < text.size())
        {
            char32_t cp = DecodeUtf8(text, pos);
            bool in_word = words.back() != kWordSeparator;
            size_t next = pos;

            if (IsWordChar(cp))
            {
                AppendUtf8(FoldCase(cp), words);
            }
            else if (cp == static_cast<char32_t>(kWordJoiner) && in_word && next < text.size() && IsWordChar(DecodeUtf8(text, next)))
            {
                words += kWordJoiner;
            }
            else if (in_word)
            {
                words += kWordSeparator;
            }
        }

        if (words.back() != kWordSeparator)
        {
            words += kWordSeparator;
        }

        return words;
    }

    // A leading '*' sets open_start, a trailing one open_end
    std::optional<GameModeKeyword> ParseKeyword(std::string_view text)
    {
        GameModeKeyword keyword;
        keyword.open_start = text.starts_with(kKeywordWildcard);
        keyword.open_end = text.ends_with(kKeywordWildcard);

        // the wildcard is no word character, so the split drops it
        std::string words = SplitWords(text);

        if (words.size() == 1)
        {
            return std::nullopt;
        }

        keyword.words = words.substr(1, words.size() - 2);

        return keyword;
    }

    // Whether the keyword occurs in SplitWords output
    bool MatchesKeyword(const std::string& words, const GameModeKeyword& keyword)
    {
        for (size_t pos = words.find(keyword.words); pos != std::string::npos; pos = words.find(keyword.words, pos + 1))
        {
            // words begins and ends with a separator and the keyword with neither, so both neighbours exist
            bool start_matches = keyword.open_start || words[pos - 1] == kWordSeparator;
            bool end_matches = keyword.open_end || words[pos + keyword.words.size()] == kWordSeparator;

            if (start_matches && end_matches)
            {
                return true;
            }
        }

        return false;
    }

    // The words arguments are SplitWords output
    bool MatchesRule(
        const GameModeRule& rule,
        const std::string& description_words,
        const std::string& name_words,
        const std::string& map_words
    )
    {
        bool keywords_match =
            rule.keywords.empty() || std::ranges::any_of(rule.keywords, [&description_words, &name_words](const GameModeKeyword& keyword) {
                return MatchesKeyword(description_words, keyword) || MatchesKeyword(name_words, keyword);
            });

        bool maps_match = rule.maps.empty() || std::ranges::any_of(rule.maps, [&map_words](const GameModeKeyword& keyword) {
                              return MatchesKeyword(map_words, keyword);
                          });

        return keywords_match && maps_match;
    }

    // The priority of a rule member that is an integer within the int range; nullopt for any other value
    std::optional<int> ReadPriority(const tao::json::value& value)
    {
        if (value.is_signed() && value.get_signed() >= std::numeric_limits<int>::min() &&
            value.get_signed() <= std::numeric_limits<int>::max())
        {
            return static_cast<int>(value.get_signed());
        }

        if (value.is_unsigned() && value.get_unsigned() <= static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            return static_cast<int>(value.get_unsigned());
        }

        return std::nullopt;
    }

    void ReadKeywords(const tao::json::value& rule, const char* key, std::vector<GameModeKeyword>& out)
    {
        const tao::json::value* list = rule.find(key);

        if (list == nullptr)
        {
            return;
        }

        if (!list->is_array())
        {
            LOG(WARNING) << "[GameModeRules] Rule member " << key << " is not an array";
            return;
        }

        for (const tao::json::value& item : list->get_array())
        {
            std::optional<GameModeKeyword> keyword = item.is_string() ? ParseKeyword(item.get_string()) : std::nullopt;

            if (!keyword)
            {
                LOG(WARNING) << "[GameModeRules] Skipped an entry of " << key << " that is no string with a word";
                continue;
            }

            out.push_back(std::move(*keyword));
        }
    }
} // namespace

bool GameModeRules::Parse(std::string_view json)
{
    rules_.clear();
    default_game_mode_.clear();
    loaded_ = false;

    if (json.starts_with(kUtf8ByteOrderMark))
    {
        json.remove_prefix(kUtf8ByteOrderMark.size());
    }

    tao::json::events::limit_nesting_depth<tao::json::events::to_value, kMaxJsonNestingDepth> document;

    try
    {
        tao::json::events::from_string(document, json);
    }
    catch (const std::exception& e)
    {
        LOG(WARNING) << "[GameModeRules] Malformed JSON: " << e.what();
        return false;
    }

    const tao::json::value* rules = document.value.is_object() ? document.value.find("rules") : nullptr;

    if (rules == nullptr || !rules->is_array())
    {
        LOG(WARNING) << "[GameModeRules] The document has no rules array";
        return false;
    }

    const tao::json::value* default_game_mode = document.value.find("default_game_mode");

    if (default_game_mode != nullptr)
    {
        std::string_view identifier = default_game_mode->is_string() ? default_game_mode->get_string() : std::string_view{};

        if (MasterDetails_IsGameModeId(identifier))
        {
            default_game_mode_ = identifier;
        }
        else
        {
            LOG(WARNING) << "[GameModeRules] The default_game_mode is no identifier, servers no rule names stay unknown";
        }
    }

    for (const tao::json::value& item : rules->get_array())
    {
        const tao::json::value* game_mode = item.is_object() ? item.find("game_mode") : nullptr;

        if (game_mode == nullptr || !game_mode->is_string())
        {
            LOG(WARNING) << "[GameModeRules] Skipped a rule without a game_mode string";
            continue;
        }

        GameModeRule rule;
        rule.game_mode = game_mode->get_string();

        if (!rule.game_mode.empty() && !MasterDetails_IsGameModeId(rule.game_mode))
        {
            LOG(WARNING) << "[GameModeRules] Skipped a rule with the invalid game_mode " << rule.game_mode;
            continue;
        }

        const tao::json::value* priority = item.find("priority");
        std::optional<int> priority_value = priority != nullptr ? ReadPriority(*priority) : std::optional<int>(0);

        if (!priority_value)
        {
            LOG(WARNING) << "[GameModeRules] Skipped a rule of " << rule.game_mode << " whose priority is no integer";
            continue;
        }

        rule.priority = *priority_value;

        ReadKeywords(item, "keywords", rule.keywords);
        ReadKeywords(item, "maps", rule.maps);

        if (rule.keywords.empty() && rule.maps.empty())
        {
            LOG(WARNING) << "[GameModeRules] Skipped a rule of " << rule.game_mode << " without keywords or maps";
            continue;
        }

        rules_.push_back(std::move(rule));
    }

    loaded_ = true;

    return true;
}

bool GameModeRules::is_loaded() const
{
    return loaded_;
}

const char* GameModeRules::Detect(std::string_view game_description, std::string_view server_name, std::string_view map_name) const
{
    std::string description_words = SplitWords(game_description);
    std::string name_words = SplitWords(server_name);
    std::string map_words = SplitWords(map_name);

    // a matching rule of the highest priority so far, and whether a rule of that priority names another mode
    const GameModeRule* decision = nullptr;
    bool ambiguous = false;

    for (const GameModeRule& rule : rules_)
    {
        if (!MatchesRule(rule, description_words, name_words, map_words))
        {
            continue;
        }

        if (decision == nullptr || rule.priority > decision->priority)
        {
            decision = &rule;
            ambiguous = false;
        }
        else if (rule.priority == decision->priority && V_stricmp(rule.game_mode.c_str(), decision->game_mode.c_str()) != 0)
        {
            ambiguous = true;
        }
    }

    return decision != nullptr && !ambiguous ? decision->game_mode.c_str() : default_game_mode_.c_str();
}
