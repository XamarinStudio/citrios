// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_shader.h"

namespace Vulkan {

constexpr TBuiltInResource DefaultTBuiltInResource = {.maxLights = 32,
                                                      .maxClipPlanes = 6,
                                                      .maxTextureUnits = 32,
                                                      .maxTextureCoords = 32,
                                                      .maxVertexAttribs = 64,
                                                      .maxVertexUniformComponents = 4096,
                                                      .maxVaryingFloats = 64,
                                                      .maxVertexTextureImageUnits = 32,
                                                      .maxCombinedTextureImageUnits = 80,
                                                      .maxTextureImageUnits = 32,
                                                      .maxFragmentUniformComponents = 4096,
                                                      .maxDrawBuffers = 32,
                                                      .maxVertexUniformVectors = 128,
                                                      .maxVaryingVectors = 8,
                                                      .maxFragmentUniformVectors = 16,
                                                      .maxVertexOutputVectors = 16,
                                                      .maxFragmentInputVectors = 15,
                                                      .minProgramTexelOffset = -8,
                                                      .maxProgramTexelOffset = 7,
                                                      .maxClipDistances = 8,
                                                      .maxComputeWorkGroupCountX = 65535,
                                                      .maxComputeWorkGroupCountY = 65535,
                                                      .maxComputeWorkGroupCountZ = 65535,
                                                      .maxComputeWorkGroupSizeX = 1024,
                                                      .maxComputeWorkGroupSizeY = 1024,
                                                      .maxComputeWorkGroupSizeZ = 64,
                                                      .maxComputeUniformComponents = 1024,
                                                      .maxComputeTextureImageUnits = 16,
                                                      .maxComputeImageUniforms = 8,
                                                      .maxComputeAtomicCounters = 8,
                                                      .maxComputeAtomicCounterBuffers = 1,
                                                      .maxVaryingComponents = 60,
                                                      .maxVertexOutputComponents = 64,
                                                      .maxGeometryInputComponents = 64,
                                                      .maxGeometryOutputComponents = 128,
                                                      .maxFragmentInputComponents = 128,
                                                      .maxImageUnits = 8,
                                                      .maxCombinedImageUnitsAndFragmentOutputs = 8,
                                                      .maxCombinedShaderOutputResources = 8,
                                                      .maxImageSamples = 0,
                                                      .maxVertexImageUniforms = 0,
                                                      .maxTessControlImageUniforms = 0,
                                                      .maxTessEvaluationImageUniforms = 0,
                                                      .maxGeometryImageUniforms = 0,
                                                      .maxFragmentImageUniforms = 8,
                                                      .maxCombinedImageUniforms = 8,
                                                      .maxGeometryTextureImageUnits = 16,
                                                      .maxGeometryOutputVertices = 256,
                                                      .maxGeometryTotalOutputComponents = 1024,
                                                      .maxGeometryUniformComponents = 1024,
                                                      .maxGeometryVaryingComponents = 64,
                                                      .maxTessControlInputComponents = 128,
                                                      .maxTessControlOutputComponents = 128,
                                                      .maxTessControlTextureImageUnits = 16,
                                                      .maxTessControlUniformComponents = 1024,
                                                      .maxTessControlTotalOutputComponents = 4096,
                                                      .maxTessEvaluationInputComponents = 128,
                                                      .maxTessEvaluationOutputComponents = 128,
                                                      .maxTessEvaluationTextureImageUnits = 16,
                                                      .maxTessEvaluationUniformComponents = 1024,
                                                      .maxTessPatchComponents = 120,
                                                      .maxPatchVertices = 32,
                                                      .maxTessGenLevel = 64,
                                                      .maxViewports = 16,
                                                      .maxVertexAtomicCounters = 0,
                                                      .maxTessControlAtomicCounters = 0,
                                                      .maxTessEvaluationAtomicCounters = 0,
                                                      .maxGeometryAtomicCounters = 0,
                                                      .maxFragmentAtomicCounters = 8,
                                                      .maxCombinedAtomicCounters = 8,
                                                      .maxAtomicCounterBindings = 1,
                                                      .maxVertexAtomicCounterBuffers = 0,
                                                      .maxTessControlAtomicCounterBuffers = 0,
                                                      .maxTessEvaluationAtomicCounterBuffers = 0,
                                                      .maxGeometryAtomicCounterBuffers = 0,
                                                      .maxFragmentAtomicCounterBuffers = 1,
                                                      .maxCombinedAtomicCounterBuffers = 1,
                                                      .maxAtomicCounterBufferSize = 16384,
                                                      .maxTransformFeedbackBuffers = 4,
                                                      .maxTransformFeedbackInterleavedComponents =
                                                          64,
                                                      .maxCullDistances = 8,
                                                      .maxCombinedClipAndCullDistances = 8,
                                                      .maxSamples = 4,
                                                      .maxMeshOutputVerticesNV = 256,
                                                      .maxMeshOutputPrimitivesNV = 512,
                                                      .maxMeshWorkGroupSizeX_NV = 32,
                                                      .maxMeshWorkGroupSizeY_NV = 1,
                                                      .maxMeshWorkGroupSizeZ_NV = 1,
                                                      .maxTaskWorkGroupSizeX_NV = 32,
                                                      .maxTaskWorkGroupSizeY_NV = 1,
                                                      .maxTaskWorkGroupSizeZ_NV = 1,
                                                      .maxMeshViewCountNV = 4,
                                                      .maxDualSourceDrawBuffersEXT = 1,
                                                      .limits = TLimits{
                                                          .nonInductiveForLoops = 1,
                                                          .whileLoops = 1,
                                                          .doWhileLoops = 1,
                                                          .generalUniformIndexing = 1,
                                                          .generalAttributeMatrixVectorIndexing = 1,
                                                          .generalVaryingIndexing = 1,
                                                          .generalSamplerIndexing = 1,
                                                          .generalVariableIndexing = 1,
                                                          .generalConstantMatrixVectorIndexing = 1,
                                                      }};

EShLanguage ToEshShaderStage(vk::ShaderStageFlagBits stage) {
    switch (stage) {
    case vk::ShaderStageFlagBits::eVertex:
        return EShLanguage::EShLangVertex;
    case vk::ShaderStageFlagBits::eGeometry:
        return EShLanguage::EShLangGeometry;
    case vk::ShaderStageFlagBits::eFragment:
        return EShLanguage::EShLangFragment;
    case vk::ShaderStageFlagBits::eCompute:
        return EShLanguage::EShLangCompute;
    default:
        LOG_CRITICAL(Render_Vulkan, "Unkown shader stage");
        UNREACHABLE();
    }

    return EShLanguage::EShLangVertex;
}

bool InitializeCompiler() {
    static bool glslang_initialized = false;

    if (glslang_initialized) {
        return true;
    }

    if (!glslang::InitializeProcess()) {
        LOG_CRITICAL(Render_Vulkan, "Failed to initialize glslang shader compiler");
        return false;
    }

    std::atexit([]() { glslang::FinalizeProcess(); });

    glslang_initialized = true;
    return true;
}

vk::ShaderModule Compile(std::string_view code, vk::ShaderStageFlagBits stage, vk::Device device,
                         ShaderOptimization level) {
    if (!InitializeCompiler()) {
        return VK_NULL_HANDLE;
    }

    EProfile profile = ECoreProfile;
    EShMessages messages =
        static_cast<EShMessages>(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
    EShLanguage lang = ToEshShaderStage(stage);

    int default_version = 450;
    const char* pass_source_code = code.data();
    int pass_source_code_length = static_cast<int>(code.size());

    auto shader = std::make_unique<glslang::TShader>(lang);
    shader->setEnvTarget(glslang::EShTargetSpv,
                         glslang::EShTargetLanguageVersion::EShTargetSpv_1_3);
    shader->setStringsWithLengths(&pass_source_code, &pass_source_code_length, 1);

    glslang::TShader::ForbidIncluder includer;
    if (!shader->parse(&DefaultTBuiltInResource, default_version, profile, false, true, messages,
                       includer)) {
        LOG_CRITICAL(Render_Vulkan, "Shader Info Log:\n{}\n{}", shader->getInfoLog(),
                     shader->getInfoDebugLog());
        return VK_NULL_HANDLE;
    }

    // Even though there's only a single shader, we still need to link it to generate SPV
    auto program = std::make_unique<glslang::TProgram>();
    program->addShader(shader.get());
    if (!program->link(messages)) {
        LOG_CRITICAL(Render_Vulkan, "Program Info Log:\n{}\n{}", program->getInfoLog(),
                     program->getInfoDebugLog());
        return VK_NULL_HANDLE;
    }

    glslang::TIntermediate* intermediate = program->getIntermediate(lang);
    std::vector<u32> out_code;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions options;

    // Compile the SPIR-V module without optimizations for easier debugging in RenderDoc.
    if (level == ShaderOptimization::Debug) {
        intermediate->addSourceText(pass_source_code, pass_source_code_length);
        options.generateDebugInfo = true;
        options.disableOptimizer = true;
        options.optimizeSize = false;
        options.disassemble = false;
        options.validate = true;
    } else {
        options.disableOptimizer = false;
        options.stripDebugInfo = true;
    }

    glslang::GlslangToSpv(*intermediate, out_code, &logger, &options);

    const std::string spv_messages = logger.getAllMessages();
    if (!spv_messages.empty()) {
        LOG_INFO(Render_Vulkan, "SPIR-V conversion messages: {}", spv_messages);
    }

    const vk::ShaderModuleCreateInfo shader_info = {.codeSize = out_code.size() * sizeof(u32),
                                                    .pCode = out_code.data()};

    return device.createShaderModule(shader_info);
}

} // namespace Vulkan