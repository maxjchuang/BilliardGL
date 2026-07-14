#include "full_game_cli.h"

#include "full_game_case_registry.h"

#include <charconv>
#include <stdexcept>

namespace fullgame {
namespace {

std::uint32_t parseSeed(const std::string& text)
{
    std::uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc() ||
        parsed.ptr != text.data() + text.size()) {
        throw std::invalid_argument("seed must be an unsigned 32-bit integer");
    }
    return value;
}

}  // namespace

FullGameCommand parseFullGameCommand(int argc, const char* const* argv)
{
    if (argc < 2) throw std::invalid_argument("a command is required");
    FullGameCommand result;
    const std::string mode = argv[1];
    int index = 2;
    if (mode == "--list-cases") {
        result.mode = FullGameMode::ListCases;
        if (index != argc) throw std::invalid_argument("unused argument");
        return result;
    }
    if (mode == "--case") {
        result.mode = FullGameMode::SingleCase;
        if (index >= argc) throw std::invalid_argument("missing case ID");
        result.caseId = argv[index++];
        if (findFullGameCase(result.caseId) == nullptr)
            throw std::invalid_argument("unknown case ID");
        if (index + 1 >= argc || std::string(argv[index]) != "--seed")
            throw std::invalid_argument("missing --seed");
        result.seed = parseSeed(argv[index + 1]);
        result.hasSeed = true;
        index += 2;
        if (index < argc && std::string(argv[index]) == "--write") {
            if (index + 1 >= argc) throw std::invalid_argument("missing write directory");
            result.writeDirectory = argv[index + 1];
            index += 2;
        }
        if (index != argc) throw std::invalid_argument("unused argument");
        return result;
    }
    if (mode == "--matrix") {
        result.mode = FullGameMode::Matrix;
        if (index >= argc) throw std::invalid_argument("missing matrix path");
        result.matrixPath = argv[index++];
        if (index + 1 >= argc || std::string(argv[index]) != "--write")
            throw std::invalid_argument("matrix mode requires --write");
        result.writeDirectory = argv[index + 1];
        index += 2;
        if (index != argc) throw std::invalid_argument("unused argument");
        return result;
    }
    throw std::invalid_argument("unknown command");
}

}  // namespace fullgame
