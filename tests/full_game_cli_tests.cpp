#include "full_game_cli.h"
#include "full_game_case_registry.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool value, const char* message)
{
    if (!value) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

fullgame::FullGameCommand parse(std::initializer_list<const char*> values)
{
    std::vector<std::string> storage;
    std::vector<const char*> arguments;
    for (const char* value : values) storage.push_back(value);
    for (const std::string& value : storage) arguments.push_back(value.c_str());
    return fullgame::parseFullGameCommand(
        static_cast<int>(arguments.size()), arguments.data());
}

bool rejects(std::initializer_list<const char*> values)
{
    try {
        parse(values);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

}  // namespace

int main()
{
    const fullgame::FullGameCommand list = parse({"stress", "--list-cases"});
    expect(list.mode == fullgame::FullGameMode::ListCases,
        "list mode should parse");
    const fullgame::FullGameCommand one = parse({
        "stress", "--case", "legacy_break_stress", "--seed", "42"});
    expect(one.mode == fullgame::FullGameMode::SingleCase &&
        one.caseId == "legacy_break_stress" && one.seed == 42u,
        "single-case mode should retain its case and seed");
    const fullgame::FullGameCommand matrix = parse({
        "stress", "--matrix", "matrix.json", "--write", "out"});
    expect(matrix.mode == fullgame::FullGameMode::Matrix &&
        matrix.matrixPath == "matrix.json" && matrix.writeDirectory == "out",
        "matrix mode should require its output directory");
    expect(rejects({"stress"}), "empty commands should fail");
    expect(rejects({"stress", "--case", "legacy_break_stress"}),
        "single-case mode should require a seed");
    expect(rejects({"stress", "--case", "unknown", "--seed", "1"}),
        "unknown cases should fail");
    expect(rejects({"stress", "--case", "legacy_break_stress", "--seed", "-1"}),
        "negative seeds should fail");
    expect(rejects({"stress", "--case", "legacy_break_stress", "--seed",
        "4294967296"}), "overflowing seeds should fail");
    expect(rejects({"stress", "--list-cases", "unused"}),
        "unused arguments should fail");
    expect(rejects({"stress", "--matrix", "matrix.json"}),
        "matrix mode should require --write");

    const std::vector<std::string> ids = fullgame::fullGameCaseIds();
    expect(!ids.empty() && std::is_sorted(ids.begin(), ids.end()) &&
        ids.size() == std::set<std::string>(ids.begin(), ids.end()).size(),
        "case IDs should be unique and lexically sorted");
    return 0;
}
