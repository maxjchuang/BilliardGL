#include "ObjLoader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int fail(const char* message)
{
    std::cerr << message << std::endl;
    return EXIT_FAILURE;
}

bool writeTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path.c_str());
    output << content;
    return output.good();
}

}  // namespace

int main()
{
    ObjLoader missing("/tmp/billiardgl-missing-file.obj");
    if (missing.isValid()) {
        return fail("missing OBJ file should be invalid");
    }
    if (missing.error().empty()) {
        return fail("missing OBJ file should report an error");
    }

    const std::string malformedObj = "/tmp/billiardgl-malformed-face.obj";
    if (!writeTextFile(malformedObj,
        "v 0 0 0\n"
        "vn 0 1 0\n"
        "f 1//1 2//1 3//1\n")) {
        return fail("test should write malformed OBJ fixture");
    }
    ObjLoader malformed(malformedObj);
    if (malformed.isValid()) {
        return fail("OBJ with out-of-range face indices should be invalid");
    }

    const std::string malformedNumericObj = "/tmp/billiardgl-malformed-numeric.obj";
    if (!writeTextFile(malformedNumericObj,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vn 0 0 1\n"
        "f 1abc//1 2//1 3//1\n")) {
        return fail("test should write malformed numeric OBJ fixture");
    }
    ObjLoader malformedNumeric(malformedNumericObj);
    if (malformedNumeric.isValid()) {
        return fail("OBJ with partial numeric face indices should be invalid");
    }

    const std::string malformedMtlObj = "/tmp/billiardgl-malformed-mtl.obj";
    const std::string malformedMtl = "/tmp/billiardgl-bad.mtl";
    if (!writeTextFile(malformedMtlObj,
        "mtllib billiardgl-bad.mtl\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vn 0 0 1\n"
        "usemtl mat\n"
        "f 1//1 2//1 3//1\n")) {
        return fail("test should write malformed MTL OBJ fixture");
    }
    if (!writeTextFile(malformedMtl, "Ka 1 1 1\n")) {
        return fail("test should write malformed MTL fixture");
    }
    ObjLoader badMaterial(malformedMtlObj);
    if (badMaterial.isValid()) {
        return fail("MTL properties before newmtl should be invalid");
    }

    const std::string validObj = "/tmp/billiardgl-valid-loader.obj";
    if (!writeTextFile(validObj,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vn 0 0 1\n"
        "f 1//1 2//1 3//1\n")) {
        return fail("test should write valid OBJ fixture");
    }
    ObjLoader valid(validObj);
    if (!valid.isValid()) {
        return fail("minimal valid OBJ should load");
    }
    if (valid.vertices.size() != 3) {
        return fail("minimal valid OBJ should produce three vertices");
    }

    return EXIT_SUCCESS;
}
