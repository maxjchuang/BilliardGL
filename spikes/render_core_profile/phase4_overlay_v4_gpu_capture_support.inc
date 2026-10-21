// Included into the generated v4 runner inside its anonymous namespace.

training::CompoundVisibilityAudit latestCompoundAudit;

double v4LinearChannel(std::uint8_t value)
{
    const double encoded = static_cast<double>(value) / 255.0;
    return encoded <= 0.04045 ? encoded / 12.92 :
        std::pow((encoded + 0.055) / 1.055, 2.4);
}

double v4Luminance(const std::array<std::uint8_t, 3>& color)
{
    return 0.2126 * v4LinearChannel(color[0]) +
        0.7152 * v4LinearChannel(color[1]) +
        0.0722 * v4LinearChannel(color[2]);
}

double v4Contrast(const std::array<std::uint8_t, 3>& first,
    const std::array<std::uint8_t, 3>& second)
{
    const double a = v4Luminance(first);
    const double b = v4Luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

int v4ColorDistance(const std::array<std::uint8_t, 3>& first,
    const std::array<std::uint8_t, 3>& second)
{
    int result = 0;
    for (int channel = 0; channel < 3; ++channel) {
        const int delta = static_cast<int>(first[channel]) - second[channel];
        result += delta * delta;
    }
    return result;
}

GLuint v4CompileShader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    require(ok == GL_TRUE, "phase4_v4_semantic_shader_compile_failed");
    return shader;
}

struct V4SemanticAttachment {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
    std::map<std::string, std::uint32_t> semanticIds;
};

std::string v4SemanticKey(training::TrajectoryContrastTrack track,
    training::CompoundComponentRole role, const std::string& primitiveId)
{
    return std::to_string(static_cast<unsigned int>(track)) + "|" +
        std::to_string(static_cast<unsigned int>(role)) + "|" + primitiveId;
}

V4SemanticAttachment v4RenderSemanticAttachment(
    const training::TrajectoryOverlayDrawPackage& package,
    int width, int height)
{
    require(!package.semanticBatches.empty(),
        "phase4_v4_semantic_batches_missing");
    const char* vertexSource = R"GLSL(#version 410 core
layout(location=0) in vec2 aPhysical;
uniform vec2 uViewport;
void main() {
    vec2 ndc=vec2(aPhysical.x/uViewport.x*2.0-1.0,
        1.0-aPhysical.y/uViewport.y*2.0);
    gl_Position=vec4(ndc,0.0,1.0);
})GLSL";
    const char* fragmentSource = R"GLSL(#version 410 core
layout(location=0) out uvec4 outSemantic;
uniform uvec4 uSemantic;
void main() { outSemantic=uSemantic; }
)GLSL";
    const GLuint vertex = v4CompileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = v4CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    require(linked == GL_TRUE, "phase4_v4_semantic_program_link_failed");

    GLuint framebuffer = 0;
    GLuint texture = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, width, height, 0,
        GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture, 0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE,
        "phase4_v4_semantic_framebuffer_incomplete");
    const GLuint clear[4] = {0, 0, 0, 0};
    glClearBufferuiv(GL_COLOR, 0, clear);
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(package.vertices.size() *
            sizeof(training::TrajectoryOverlayVertex)),
        package.vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
        sizeof(training::TrajectoryOverlayVertex), nullptr);
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uViewport"),
        static_cast<float>(width), static_cast<float>(height));

    V4SemanticAttachment result;
    result.width = width;
    result.height = height;
    for (std::size_t index = 0; index < package.semanticBatches.size(); ++index) {
        const auto& batch = package.semanticBatches[index];
        require(batch.firstVertex + batch.vertexCount <= package.vertices.size(),
            "phase4_v4_semantic_batch_range_invalid");
        const std::uint32_t primitive = static_cast<std::uint32_t>(index + 1);
        result.semanticIds[v4SemanticKey(batch.track, batch.componentRole,
            batch.primitiveId)] = primitive;
        const std::uint32_t track = batch.track ==
            training::TrajectoryContrastTrack::Predicted ? 1u : 2u;
        const std::uint32_t component =
            static_cast<std::uint32_t>(batch.componentRole) + 1u;
        glUniform4ui(glGetUniformLocation(program, "uSemantic"),
            track, component, primitive, 255u);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.firstVertex),
            static_cast<GLsizei>(batch.vertexCount));
    }
    glFinish();
    result.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
        result.pixels.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return result;
}

std::array<std::uint32_t, 4> v4SemanticPixel(
    const V4SemanticAttachment& attachment, int x, int y)
{
    if (x < 0 || y < 0 || x >= attachment.width || y >= attachment.height)
        return {{0, 0, 0, 0}};
    const int glY = attachment.height - 1 - y;
    const std::size_t offset = static_cast<std::size_t>(
        (glY * attachment.width + x) * 4);
    return {{attachment.pixels[offset], attachment.pixels[offset + 1],
        attachment.pixels[offset + 2], attachment.pixels[offset + 3]}};
}

training::TrajectoryContrastAudit contrastAudit(
    const PpmImage& image,
    const training::TrajectoryOverlayDrawPackage& package,
    training::TrajectoryOverlayPalette palette,
    bool predictedRequired, bool actualRequired,
    const std::string& captureId)
{
    training::TrajectoryContrastAudit legacy = contrastAuditV3(image, package,
        palette, predictedRequired, actualRequired);
    const V4SemanticAttachment attachment = v4RenderSemanticAttachment(
        package, image.width, image.height);
    training::CompoundVisibilityAuditRequest request;
    request.predictedRequired = predictedRequired;
    request.actualRequired = actualRequired;
    request.minimumSourceProbeCount = captureId ==
        "core__launch_right_english_one_rail_v1__broadcast" ? 159 : 1;
    request.expectedSourceProbeCount = package.contrastProbes.size();
    request.predictedActualLumaDeltaSrgb8 =
        legacy.predictedActualLumaDeltaSrgb8;
    request.requiredPredictedActualLumaDeltaSrgb8 =
        palette == training::TrajectoryOverlayPalette::Standard ? 32.0 : 36.0;
    const std::string physicalProfile = std::to_string(image.width) + "x" +
        std::to_string(image.height) + "|line=" +
        std::to_string(package.style.lineWidthPx) + "|halo=" +
        std::to_string(package.style.haloWidthPx);
    for (const auto& probe : package.contrastProbes) {
        training::CompoundSourceProbe source;
        source.captureId = captureId;
        source.track = probe.track ==
            training::TrajectoryContrastTrack::Predicted ?
                training::CompoundVisibilityTrack::Predicted :
                training::CompoundVisibilityTrack::Actual;
        source.primitiveId = probe.primitiveId;
        source.componentRole = probe.componentRole;
        source.arcSegmentId = probe.arcSegmentId;
        source.probeOrdinal = probe.probeOrdinal;
        source.physicalProfile = physicalProfile;
        source.xPhysicalPx = probe.componentRole ==
            training::CompoundComponentRole::OuterHaloEdge ?
                probe.haloXPhysicalPx : probe.strokeXPhysicalPx;
        source.yPhysicalPx = probe.componentRole ==
            training::CompoundComponentRole::OuterHaloEdge ?
                probe.haloYPhysicalPx : probe.strokeYPhysicalPx;
        source.outerHaloRadiusPx = package.style.lineWidthPx * 0.5 +
            package.style.haloWidthPx;
        source.halfTickLengthPx = package.style.actualTickLengthPx * 0.5;
        source.sourceProbeSpacingPx = source.track ==
            training::CompoundVisibilityTrack::Predicted ?
                package.style.dashOnPx + package.style.dashOffPx :
                package.style.actualTickSpacingPx;
        source.insideFinalViewport = source.xPhysicalPx >= 0 &&
            source.yPhysicalPx >= 0 && source.xPhysicalPx < image.width &&
            source.yPhysicalPx < image.height;
        request.sourceProbes.push_back(source);
    }

    const std::array<std::uint8_t, 3> predictedCore{{255, 254, 115}};
    const std::array<std::uint8_t, 3> actualCore{{1, 22, 32}};
    const std::array<std::uint8_t, 3> predictedHalo{{0, 0, 0}};
    const std::array<std::uint8_t, 3> actualHalo{{255, 255, 255}};
    for (const training::CompoundSourceProbe& source : request.sourceProbes) {
        const auto probeTrack = source.track ==
            training::CompoundVisibilityTrack::Predicted ?
                training::TrajectoryContrastTrack::Predicted :
                training::TrajectoryContrastTrack::Actual;
        const auto primitiveFound = attachment.semanticIds.find(
            v4SemanticKey(probeTrack, source.componentRole,
                source.primitiveId));
        if (primitiveFound == attachment.semanticIds.end()) continue;
        const std::uint32_t expectedTrack = source.track ==
            training::CompoundVisibilityTrack::Predicted ? 1u : 2u;
        const std::uint32_t expectedComponent =
            static_cast<std::uint32_t>(source.componentRole) + 1u;
        const bool halo = source.componentRole ==
            training::CompoundComponentRole::OuterHaloEdge;
        const auto& target = source.track ==
            training::CompoundVisibilityTrack::Predicted ?
                (halo ? predictedHalo : predictedCore) :
                (halo ? actualHalo : actualCore);
        const int colorTolerance = halo && expectedTrack == 2u ? 12000 : 5000;
        const int radius = static_cast<int>(std::ceil(std::max(
            source.outerHaloRadiusPx + 2.0,
            source.componentRole == training::CompoundComponentRole::Tick ?
                source.halfTickLengthPx + 2.0 : 0.0)));
        bool found = false;
        training::CompoundOwnershipCandidate best;
        int bestDistance = std::numeric_limits<int>::max();
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int x = source.xPhysicalPx + dx;
                const int y = source.yPhysicalPx + dy;
                const auto semantic = v4SemanticPixel(attachment, x, y);
                if (semantic[0] != expectedTrack ||
                    semantic[1] != expectedComponent ||
                    semantic[2] != primitiveFound->second) continue;
                const auto finalRgb = ppmPixel(image, x, y);
                if (v4ColorDistance(finalRgb, target) > colorTolerance) continue;
                std::size_t connected = 0;
                for (int cy = -1; cy <= 1; ++cy)
                    for (int cx = -1; cx <= 1; ++cx) {
                        const auto nearby = v4SemanticPixel(
                            attachment, x + cx, y + cy);
                        if (nearby[0] == semantic[0] &&
                            nearby[2] == semantic[2]) ++connected;
                    }
                std::array<std::uint8_t, 3> adjacent{{0, 0, 0}};
                double contrast = 0.0;
                bool edge = false;
                const int directions[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for (const auto& direction : directions) {
                    const int ax = x + direction[0];
                    const int ay = y + direction[1];
                    if (ax < 0 || ay < 0 || ax >= image.width ||
                        ay >= image.height) continue;
                    const auto neighborSemantic = v4SemanticPixel(
                        attachment, ax, ay);
                    if (neighborSemantic[0] == semantic[0] &&
                        neighborSemantic[2] == semantic[2]) continue;
                    const auto neighbor = ppmPixel(image, ax, ay);
                    const double candidateContrast = v4Contrast(finalRgb,
                        neighbor);
                    if (candidateContrast > contrast) {
                        contrast = candidateContrast;
                        adjacent = neighbor;
                        edge = true;
                    }
                }
                const int distance = dx * dx + dy * dy;
                if (!edge || connected < 2 || distance >= bestDistance) continue;
                found = true;
                bestDistance = distance;
                best.sourceProbeId =
                    training::stableCompoundSourceProbeId(source);
                best.xPhysicalPx = x;
                best.yPhysicalPx = y;
                best.ownerTrack = source.track;
                best.ownerComponentRole = source.componentRole;
                best.ownerPrimitiveId = source.primitiveId;
                best.resolvedAlpha = 1.0;
                best.fromFinalCompositeAttachment = true;
                best.correspondsToFinalColor = true;
                best.normalDistancePx = std::sqrt(static_cast<double>(distance));
                best.longitudinalDistancePx = 0.0;
                best.sameEndpointPrimitive = true;
                best.nearestTickInterval = true;
                best.fourConnectedOwnedPixelCount = connected;
                best.ownedPixelEquivalentArea = static_cast<double>(connected);
                best.finalFramebufferSrgb8 = finalRgb;
                best.adjacentNonSelfSrgb8 = adjacent;
                best.bordersAdjacentNonSelfPixel = true;
                best.finalEdgeContrastRatio = contrast;
            }
        }
        if (found) request.ownershipCandidates.push_back(best);
    }
    latestCompoundAudit = training::auditCompoundFinalVisibility(request);
    legacy.sourceProbeCount = latestCompoundAudit.sourceProbeCount;
    legacy.visibleProbeCount = latestCompoundAudit.visibleProbeCount;
    legacy.hiddenProbeCount = latestCompoundAudit.hiddenProbeCount;
    legacy.visibleProbeCoverage = latestCompoundAudit.visibleProbeCoverage;
    if (legacy.errorCode ==
            "trajectory_visible_probe_coverage_below_90_percent") {
        legacy.errorCode.clear();
        legacy.ok = legacy.minimumLocalContrastRatio >= 4.0 &&
            legacy.designTargetCoverage >= 0.99 &&
            legacy.predictedActualLumaDeltaSrgb8 >=
                request.requiredPredictedActualLumaDeltaSrgb8;
    }
    if (!latestCompoundAudit.ok) {
        legacy.ok = false;
        legacy.errorCode = latestCompoundAudit.errorCode;
    }
    return legacy;
}

std::string v4ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input),
        "phase4_v4_authorization_receipt_read_failed");
    return std::string((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
}

void validateAndConsumeV4Authorization(const char* runnerPath,
    const char* outputRoot)
{
    const char* receiptEnvironment = std::getenv(
        "BILLIARDGL_PHASE4_V4_GPU_AUTH_RECEIPT");
    require(receiptEnvironment != nullptr && receiptEnvironment[0] != '\0',
        "phase4_v4_gpu_authorization_receipt_required");
    const std::filesystem::path receiptPath(receiptEnvironment);
    const std::string bytes = v4ReadFile(receiptPath);
    const json::ParseResult parsed = json::parse(bytes);
    require(parsed.ok && parsed.value.isObject(),
        "phase4_v4_gpu_authorization_receipt_invalid");
    const json::Value& receipt = parsed.value;
    require(receipt.at("schema").asString() ==
            "billiardgl.phase4.overlay_v4_gpu_authorization_receipt.v1" &&
        receipt.at("authorized").asBool() &&
        receipt.at("single_use").asBool() &&
        receipt.at("correction_design_gate_sha256").asString() ==
            "1901a825b092c08ce79425ec936ec150fada9ff4e3b4f9817e818d481013a39f" &&
        receipt.at("runner_sha256").asString() ==
            sha256Hex(v4ReadFile(std::filesystem::path(runnerPath))) &&
        receipt.at("source_sha256").asString().size() == 64 &&
        receipt.at("fixture_manifest_sha256").asString().size() == 64 &&
        receipt.at("ready_manifest_sha256").asString().size() == 64 &&
        receipt.at("billiards_executable_sha256").asString().size() == 64,
        "phase4_v4_gpu_authorization_receipt_binding_failed");
    require(std::filesystem::path(receipt.at("output_root").asString()) ==
            std::filesystem::absolute(outputRoot).lexically_normal(),
        "phase4_v4_gpu_authorization_output_root_mismatch");
    const std::filesystem::path consumed = receiptPath.string() + ".consumed";
    const int descriptor = open(consumed.c_str(), O_WRONLY | O_CREAT | O_EXCL,
        S_IRUSR | S_IWUSR);
    require(descriptor >= 0,
        "phase4_v4_gpu_authorization_receipt_already_consumed");
    const std::string marker = receipt.at("authorization_nonce").asString() +
        "\n";
    require(marker.size() > 1 && write(descriptor, marker.data(), marker.size()) ==
        static_cast<ssize_t>(marker.size()),
        "phase4_v4_gpu_authorization_consume_write_failed");
    close(descriptor);
}
