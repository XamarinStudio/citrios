// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <functional>
#include <span>
#include "common/hash.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/rasterizer_cache/types.h"

namespace OpenGL {

struct FormatTuple {
    int internal_format;
    u32 format;
    u32 type;
};

const FormatTuple& GetFormatTuple(PixelFormat pixel_format);

struct HostTextureTag {
    PixelFormat format{};
    u32 width = 0;
    u32 height = 0;

    auto operator<=>(const HostTextureTag&) const noexcept = default;

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(HostTextureTag));
    }
};

struct TextureCubeConfig {
    PAddr px;
    PAddr nx;
    PAddr py;
    PAddr ny;
    PAddr pz;
    PAddr nz;
    u32 width;
    Pica::TexturingRegs::TextureFormat format;

    auto operator<=>(const TextureCubeConfig&) const noexcept = default;

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(TextureCubeConfig));
    }
};

class SurfaceParams;

void SwizzleTexture(const SurfaceParams& params, u32 flush_start, u32 flush_end,
                    std::span<std::byte> source_linear, std::span<std::byte> dest_tiled);

void UnswizzleTexture(const SurfaceParams& params, u32 load_start, u32 load_end,
                      std::span<std::byte> source_tiled, std::span<std::byte> dest_linear);

[[nodiscard]] ClearValue MakeClearValue(SurfaceType type, PixelFormat format, const u8* fill_data);

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::HostTextureTag> {
    std::size_t operator()(const OpenGL::HostTextureTag& tag) const noexcept {
        return tag.Hash();
    }
};

template <>
struct hash<OpenGL::TextureCubeConfig> {
    std::size_t operator()(const OpenGL::TextureCubeConfig& config) const noexcept {
        return config.Hash();
    }
};
} // namespace std