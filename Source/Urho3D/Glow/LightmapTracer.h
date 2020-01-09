//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

/// \file

#pragma once

#include "../Glow/LightmapCharter.h"
#include "../Glow/LightmapGeometryBaker.h"
#include "../Graphics/LightProbeGroup.h"

namespace Urho3D
{

class EmbreeScene;
class TetrahedralMesh;
struct LightProbeCollection;

/// Preprocess geometry buffer. Fix shadow bleeding.
URHO3D_API void PreprocessGeometryBuffer(LightmapChartGeometryBuffer& geometryBuffer,
    const EmbreeScene& embreeScene, const ea::vector<unsigned>& geometryBufferToEmbree,
    const LightmapTracingSettings& settings);

/// Direct light accumulated for given lightmap chart.
struct LightmapChartBakedDirect
{
    /// Construct default.
    LightmapChartBakedDirect() = default;
    /// Construct valid.
    LightmapChartBakedDirect(unsigned width, unsigned height)
        : width_(width)
        , height_(height)
        , realWidth_(static_cast<float>(width_))
        , realHeight_(static_cast<float>(height_))
        , directLight_(width_ * height_, Vector4(0.0f, 0.0f, 0.0f, 1.0f))
        , surfaceLight_(width_ * height_)
    {
    }
    /// Return nearest point location by UV.
    IntVector2 GetNearestLocation(const Vector2& uv) const
    {
        const int x = FloorToInt(ea::min(uv.x_ * realWidth_, realWidth_ - 1.0f));
        const int y = FloorToInt(ea::min(uv.y_ * realHeight_, realHeight_ - 1.0f));
        return { x, y };
    }
    /// Return surface light by location.
    const Vector3& GetSurfaceLight(const IntVector2& location) const
    {
        const unsigned index = location.x_ + location.y_ * width_;
        return surfaceLight_[index];
    }

    /// Width of the chart.
    unsigned width_{};
    /// Height of the chart.
    unsigned height_{};
    /// Width of the chart as float.
    float realWidth_{};
    /// Height of the chart as float.
    float realHeight_{};
    /// Incoming direct light from completely backed lights, to be baked in lightmap.
    /// W-component is unused and is needed for alignment since this array is copied to GPU.
    ea::vector<Vector4> directLight_;
    /// Incoming direct light from all static lights multiplied with albedo, used to calculate indirect lighting.
    ea::vector<Vector3> surfaceLight_;
};

/// Indirect light accumulated for given lightmap chart.
struct LightmapChartBakedIndirect
{
    /// Construct default.
    LightmapChartBakedIndirect() = default;
    /// Construct valid.
    LightmapChartBakedIndirect(unsigned width, unsigned height)
        : width_(width)
        , height_(height)
        , light_(width_ * height_)
        , lightSwap_(width_ * height_)
    {
    }
    /// Normalize collected light.
    void NormalizeLight()
    {
        for (Vector4& value : light_)
        {
            if (value.w_ > 0)
                value /= value.w_;
        }
    }

    /// Width of the chart.
    unsigned width_{};
    /// Height of the chart.
    unsigned height_{};
    /// Indirect light. W component represents normalization weight.
    ea::vector<Vector4> light_;
    /// Swap buffer for indirect light. Used by filters.
    ea::vector<Vector4> lightSwap_;
};

/// Directional light parameters.
struct DirectionalLightParameters
{
    /// Direction of the light.
    Vector3 direction_;
    /// Color of the light.
    Color color_;
    /// Whether to bake direct light.
    bool bakeDirect_{};
    /// Whether to collect indirect light.
    bool bakeIndirect_{};
};

/// Accumulate direct light from directional light.
URHO3D_API void BakeDirectionalLight(LightmapChartBakedDirect& bakedDirect, const LightmapChartGeometryBuffer& geometryBuffer,
    const EmbreeScene& embreeScene, const ea::vector<unsigned>& geometryBufferToEmbree,
    const DirectionalLightParameters& light, const LightmapTracingSettings& settings);

/// Accumulate indirect light for charts.
URHO3D_API void BakeIndirectLightForCharts(LightmapChartBakedIndirect& bakedIndirect,
    const ea::vector<const LightmapChartBakedDirect*>& bakedDirect, const LightmapChartGeometryBuffer& geometryBuffer,
    const TetrahedralMesh& lightProbesMesh, const LightProbeCollection& lightProbesData,
    const EmbreeScene& embreeScene, const ea::vector<unsigned>& geometryBufferToEmbree,
    const LightmapTracingSettings& settings);

/// Accumulate indirect light for light probes.
URHO3D_API void BakeIndirectLightForLightProbes(LightProbeCollection& collection,
    const ea::vector<const LightmapChartBakedDirect*>& bakedDirect,
    const EmbreeScene& embreeScene, const LightmapTracingSettings& settings);

/// Parameters for indirect light filtering.
struct IndirectFilterParameters
{
    /// Kernel radius.
    int kernelRadius_{ 2 };
    /// Upscale factor for offsets.
    int upscale_{ 1 };
    /// Color weight. The lesser value is, the more color details are preserved on flat surface.
    float luminanceSigma_{ 10.0f };
    /// Normal weight. The higher value is, the more color details are preserved on normal edges.
    float normalPower_{ 4.0f };
    /// Position weight. The lesser value is, the more color details are preserved on position edges.
    float positionSigma_{ 1.0f };
};

/// Filter indirect light.
URHO3D_API void FilterIndirectLight(LightmapChartBakedIndirect& bakedIndirect, const LightmapChartGeometryBuffer& geometryBuffer,
    const IndirectFilterParameters& params, unsigned numTasks);


}
