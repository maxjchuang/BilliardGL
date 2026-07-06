#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readFile(const std::string& path)
{
    std::ifstream input(path.c_str());
    assert(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void assertNotContains(const std::string& content, const std::string& needle)
{
    assert(content.find(needle) == std::string::npos);
}

}

int main()
{
    const std::string billiards = readFile(std::string(BILLIARDGL_SOURCE_ROOT) + "/src/Billiards/billiards.cpp");

    assertNotContains(billiards, "GLuint texture[");
    assertNotContains(billiards, "emitter *e[");
    assertNotContains(billiards, "bool Fired[");
    assertNotContains(billiards, "bool AllFired");
    assertNotContains(billiards, "new emitter(");

    return 0;
}
