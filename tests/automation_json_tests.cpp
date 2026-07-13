#include "automation_json.h"

#include <cstdlib>
#include <iostream>

namespace {
void expect(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main()
{
    const billiardgl::json::ParseResult parsed = billiardgl::json::parse(
        "{\"id\":1,\"command\":\"step\",\"params\":{\"ticks\":3},\"tags\":[true,null,\"a\\nb\"]}");
    expect(parsed.ok, "valid JSON should parse");
    expect(parsed.value.at("command").asString() == "step", "string should be readable");
    expect(parsed.value.at("params").at("ticks").asInt() == 3, "number should be readable");
    expect(parsed.value.at("tags").asArray().size() == 3, "array should be readable");
    expect(billiardgl::json::stringify(parsed.value).find("\"command\":\"step\"") != std::string::npos,
        "object should serialize");
    expect(!billiardgl::json::parse("{\"id\":1").ok, "unterminated object should fail");
    expect(!billiardgl::json::parse("{} trailing").ok, "trailing data should fail");
    expect(billiardgl::json::parse("\"\\u4e2d\\u6587\"").value.asString() == "中文",
        "unicode escapes should decode to UTF-8");
    return 0;
}
