//
// Copyright (c) 2017-2020 the rbfx project.
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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DrawCommandQueue.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Zone.h"
#include "../IO/Log.h"
#include "../RenderPipeline/BatchRenderer.h"
#include "../RenderPipeline/DrawableProcessor.h"
#include "../RenderPipeline/InstancingBufferCompositor.h"
#include "../RenderPipeline/LightProcessor.h"
#include "../RenderPipeline/PipelineBatchSortKey.h"
#include "../Scene/Scene.h"

#include <EASTL/sort.h>

#include "../DebugNew.h"

namespace Urho3D
{

namespace
{

/// Return shader parameter for camera depth mode.
Vector4 GetCameraDepthModeParameter(const Camera& camera)
{
    Vector4 depthMode = Vector4::ZERO;
    if (camera.IsOrthographic())
    {
        depthMode.x_ = 1.0f;
#ifdef URHO3D_OPENGL
        depthMode.z_ = 0.5f;
        depthMode.w_ = 0.5f;
#else
        depthMode.z_ = 1.0f;
#endif
    }
    else
        depthMode.w_ = 1.0f / camera.GetFarClip();
    return depthMode;
}

/// Return shader parameter for camera depth reconstruction.
Vector4 GetCameraDepthReconstructParameter(const Camera& camera)
{
    const float nearClip = camera.GetNearClip();
    const float farClip = camera.GetFarClip();
    return {
        farClip / (farClip - nearClip),
        -nearClip / (farClip - nearClip),
        camera.IsOrthographic() ? 1.0f : 0.0f,
        camera.IsOrthographic() ? 0.0f : 1.0f
    };
}

/// Return shader parameter for zone fog.
Vector4 GetFogParameter(const Camera& camera)
{
    const float farClip = camera.GetFarClip();
    float fogStart = Min(camera.GetEffectiveFogStart(), farClip);
    float fogEnd = Min(camera.GetEffectiveFogStart(), farClip);
    if (fogStart >= fogEnd * (1.0f - M_LARGE_EPSILON))
        fogStart = fogEnd * (1.0f - M_LARGE_EPSILON);
    const float fogRange = Max(fogEnd - fogStart, M_EPSILON);
    return {
        fogEnd / farClip,
        farClip / fogRange,
        0.0f,
        0.0f
    };
}

/// Batch renderer to command queue.
// TODO(renderer): Add template parameter to avoid branching?
class DrawCommandCompositor : public NonCopyable
{
public:
    /// Initialized
    /// @{
    DrawCommandCompositor(DrawCommandQueue& drawQueue, const BatchRendererSettings& settings,
        const DrawableProcessor& drawableProcessor, InstancingBufferCompositor& instancingBuffer,
        const Camera* renderCamera, BatchRenderFlags flags, const ShadowSplitProcessor* outputShadowSplit)
        : drawQueue_(drawQueue)
        , settings_(settings)
        , drawableProcessor_(drawableProcessor)
        , instancingBuffer_(instancingBuffer)
        , frameInfo_(drawableProcessor_.GetFrameInfo())
        , scene_(*frameInfo_.scene_)
        , lights_(drawableProcessor_.GetLightProcessors())
        , camera_(*renderCamera)
        , cameraNode_(*camera_.GetNode())
        , outputShadowSplit_(outputShadowSplit)
        , enabled_(flags)
    {
    }

    void SetGBufferParameters(const Vector4& offsetAndScale, const Vector2& invSize)
    {
        geometryBufferOffsetAndScale_ = offsetAndScale;
        geometryBufferInvSize_ = invSize;
    }

    void SetGlobalResources(ea::span<const ShaderResourceDesc> globalResources)
    {
        globalResources_ = globalResources;
    }
    /// @}

    /// Process batches
    /// @{
    void ProcessSceneBatch(const PipelineBatch& pipelineBatch)
    {
        ProcessBatch(pipelineBatch, pipelineBatch.GetSourceBatch());
    }

    void ProcessLightVolumeBatch(const PipelineBatch& pipelineBatch)
    {
        Light* light = lights_[pipelineBatch.lightIndex_]->GetLight();
        lightVolumeHelpers_.transform_ = light->GetVolumeTransform(&camera_);
        lightVolumeHelpers_.sourceBatch_.worldTransform_ = &lightVolumeHelpers_.transform_;
        ProcessBatch(pipelineBatch, lightVolumeHelpers_.sourceBatch_);
    }

    void FlushDrawCommands()
    {
        if (instancingGroup_.count_ > 0)
            DrawObjectInstanced();
    }
    /// @}

private:
    /// Extract and process batch state w/o any changes in DrawQueue
    /// @{
    void CheckDirtyCommonState(const PipelineBatch& pipelineBatch)
    {
        dirty_.pipelineState_ = current_.pipelineState_ != pipelineBatch.pipelineState_;
        current_.pipelineState_ = pipelineBatch.pipelineState_;

        const float constantDepthBias = current_.pipelineState_->GetDesc().constantDepthBias_;
        if (current_.constantDepthBias_ != constantDepthBias)
        {
            current_.constantDepthBias_ = constantDepthBias;
            dirty_.cameraConstants_ = true;
        }

        dirty_.material_ = current_.material_ != pipelineBatch.material_;
        current_.material_ = pipelineBatch.material_;

        dirty_.geometry_ = current_.geometry_ != pipelineBatch.geometry_;
        current_.geometry_ = pipelineBatch.geometry_;
    }

    void CheckDirtyPixelLight(const PipelineBatch& pipelineBatch)
    {
        dirty_.pixelLightConstants_ = current_.pixelLightIndex_ != pipelineBatch.lightIndex_;
        if (!dirty_.pixelLightConstants_)
            return;

        current_.pixelLightIndex_ = pipelineBatch.lightIndex_;
        current_.pixelLightEnabled_ = current_.pixelLightIndex_ != M_MAX_UNSIGNED;
        if (current_.pixelLightEnabled_)
        {
            current_.pixelLightParams_ = &lights_[current_.pixelLightIndex_]->GetShaderParams();
            dirty_.pixelLightTextures_ = current_.pixelLightRamp_ != current_.pixelLightParams_->lightRamp_
                || current_.pixelLightShape_ != current_.pixelLightParams_->lightShape_
                || current_.pixelLightShadowMap_ != current_.pixelLightParams_->shadowMap_;
            if (dirty_.pixelLightTextures_)
            {
                current_.pixelLightRamp_ = current_.pixelLightParams_->lightRamp_;
                current_.pixelLightShape_ = current_.pixelLightParams_->lightShape_;
                current_.pixelLightShadowMap_ = current_.pixelLightParams_->shadowMap_;
            }
        }
    }

    void CheckDirtyVertexLight(const LightAccumulator& lightAccumulator)
    {
        const auto previousVertexLights = current_.vertexLights_;
        current_.vertexLights_ = lightAccumulator.GetVertexLights();
        dirty_.vertexLightConstants_ = previousVertexLights != current_.vertexLights_;
        if (!dirty_.vertexLightConstants_)
            return;

        static const LightShaderParameters nullVertexLight{};
        for (unsigned i = 0; i < MAX_VERTEX_LIGHTS; ++i)
        {
            const LightShaderParameters& params = current_.vertexLights_[i] != M_MAX_UNSIGNED
                ? lights_[current_.vertexLights_[i]]->GetShaderParams() : nullVertexLight;
            const Vector3& color = params.GetColor(settings_.gammaCorrection_);

            current_.vertexLightsData_[i * 3] = { color, params.invRange_ };
            current_.vertexLightsData_[i * 3 + 1] = { params.direction_, params.cutoff_ };
            current_.vertexLightsData_[i * 3 + 2] = { params.position_, params.invCutoff_ };
        }
    }

    void CheckDirtyLightmap(const SourceBatch& sourceBatch)
    {
        dirty_.lightmapConstants_ = current_.lightmapScaleOffset_ != sourceBatch.lightmapScaleOffset_;
        if (!dirty_.lightmapConstants_)
            return;

        current_.lightmapScaleOffset_ = sourceBatch.lightmapScaleOffset_;

        Texture2D* lightmapTexture = current_.lightmapScaleOffset_
            ? scene_.GetLightmapTexture(sourceBatch.lightmapIndex_)
            : nullptr;

        dirty_.lightmapTextures_ = current_.lightmapTexture_ != lightmapTexture;
        current_.lightmapTexture_ = lightmapTexture;
    }

    void ExtractObjectConstants(const SourceBatch& sourceBatch, const LightAccumulator* lightAccumulator)
    {
        if (enabled_.ambientLighting_)
        {
            if (settings_.ambientMode_ == AmbientMode::Flat)
            {
                const Vector3 ambient = lightAccumulator->sphericalHarmonics_.EvaluateAverage();
                if (settings_.gammaCorrection_)
                    object_.ambient_ = Vector4(ambient, 1.0f);
                else
                    object_.ambient_ = Color(ambient).LinearToGamma().ToVector4();
            }
            else if (settings_.ambientMode_ == AmbientMode::Directional)
            {
                object_.sh_ = &lightAccumulator->sphericalHarmonics_;
            }
        }

        object_.geometryType_ = sourceBatch.geometryType_;
        object_.worldTransform_ = sourceBatch.worldTransform_;
        object_.numWorldTransforms_ = sourceBatch.numWorldTransforms_;
    }
    /// @}

    /// Commit changes to DrawQueue
    /// @{
    void UpdateDirtyConstants()
    {
        if (dirty_.pipelineState_)
        {
            drawQueue_.SetPipelineState(current_.pipelineState_);
            dirty_.pipelineState_ = false;
        }

        if (drawQueue_.BeginShaderParameterGroup(SP_FRAME, dirty_.frameConstants_))
        {
            AddFrameConstants();
            drawQueue_.CommitShaderParameterGroup(SP_FRAME);
            dirty_.frameConstants_ = false;
        }

        if (drawQueue_.BeginShaderParameterGroup(SP_CAMERA, dirty_.cameraConstants_))
        {
            AddCameraConstants(current_.constantDepthBias_);
            drawQueue_.CommitShaderParameterGroup(SP_CAMERA);
            dirty_.cameraConstants_ = false;
        }

        // Commit pixel light constants once during shadow map rendering to support normal bias
        if (outputShadowSplit_ != nullptr)
        {
            if (drawQueue_.BeginShaderParameterGroup(SP_LIGHT, false))
            {
                const LightShaderParameters& params = outputShadowSplit_->GetLightProcessor()->GetShaderParams();
                AddPixelLightConstants(params);
                drawQueue_.CommitShaderParameterGroup(SP_LIGHT);
            }
        }
        else if (enabled_.anyLighting_)
        {
            const bool lightConstantsDirty = dirty_.pixelLightConstants_ || dirty_.vertexLightConstants_;
            if (drawQueue_.BeginShaderParameterGroup(SP_LIGHT, lightConstantsDirty))
            {
                if (enabled_.vertexLighting_)
                    AddVertexLightConstants();
                if (current_.pixelLightEnabled_)
                    AddPixelLightConstants(*current_.pixelLightParams_);
                drawQueue_.CommitShaderParameterGroup(SP_LIGHT);
            }
        }

        if (drawQueue_.BeginShaderParameterGroup(SP_MATERIAL, dirty_.material_ || dirty_.lightmapConstants_))
        {
            const auto& materialParameters = current_.material_->GetShaderParameters();
            for (const auto& parameter : materialParameters)
                drawQueue_.AddShaderParameter(parameter.first, parameter.second.value_);

            if (enabled_.ambientLighting_ && current_.lightmapScaleOffset_)
                drawQueue_.AddShaderParameter(VSP_LMOFFSET, *current_.lightmapScaleOffset_);

            drawQueue_.CommitShaderParameterGroup(SP_MATERIAL);
        }
    }

    void UpdateDirtyResources()
    {
        const bool resourcesDirty = dirty_.material_ || dirty_.lightmapTextures_ || dirty_.pixelLightTextures_;
        if (resourcesDirty)
        {
            for (const ShaderResourceDesc& desc : globalResources_)
                drawQueue_.AddShaderResource(desc.unit_, desc.texture_);

            const auto& materialTextures = current_.material_->GetTextures();
            for (const auto& texture : materialTextures)
            {
                if (!current_.lightmapTexture_ || texture.first != TU_EMISSIVE)
                    drawQueue_.AddShaderResource(texture.first, texture.second);
            }

            if (current_.lightmapTexture_)
                drawQueue_.AddShaderResource(TU_EMISSIVE, current_.lightmapTexture_);
            if (current_.pixelLightRamp_)
                drawQueue_.AddShaderResource(TU_LIGHTRAMP, current_.pixelLightRamp_);
            if (current_.pixelLightShape_)
                drawQueue_.AddShaderResource(TU_LIGHTSHAPE, current_.pixelLightShape_);
            if (current_.pixelLightShadowMap_)
                drawQueue_.AddShaderResource(TU_SHADOWMAP, current_.pixelLightShadowMap_);

            drawQueue_.CommitShaderResources();
        }
    }

    void AddFrameConstants()
    {
        drawQueue_.AddShaderParameter(VSP_DELTATIME, frameInfo_.timeStep_);
        drawQueue_.AddShaderParameter(VSP_ELAPSEDTIME, scene_.GetElapsedTime());
    }

    void AddCameraConstants(float constantDepthBias)
    {
        drawQueue_.AddShaderParameter(VSP_GBUFFEROFFSETS, geometryBufferOffsetAndScale_);

        const Matrix3x4 cameraEffectiveTransform = camera_.GetEffectiveWorldTransform();
        drawQueue_.AddShaderParameter(VSP_CAMERAPOS, cameraEffectiveTransform.Translation());
        drawQueue_.AddShaderParameter(VSP_VIEWINV, cameraEffectiveTransform);
        drawQueue_.AddShaderParameter(VSP_VIEW, camera_.GetView());

        const float nearClip = camera_.GetNearClip();
        const float farClip = camera_.GetFarClip();
        drawQueue_.AddShaderParameter(VSP_NEARCLIP, nearClip);
        drawQueue_.AddShaderParameter(VSP_FARCLIP, farClip);

        if (outputShadowSplit_)
        {
            const LightShaderParameters& lightParams = outputShadowSplit_->GetLightProcessor()->GetShaderParams();
            drawQueue_.AddShaderParameter(VSP_NORMALOFFSETSCALE,
                lightParams.shadowNormalBias_[outputShadowSplit_->GetSplitIndex()]);
        }

        drawQueue_.AddShaderParameter(VSP_DEPTHMODE, GetCameraDepthModeParameter(camera_));
        drawQueue_.AddShaderParameter(PSP_DEPTHRECONSTRUCT, GetCameraDepthReconstructParameter(camera_));

        Vector3 nearVector, farVector;
        camera_.GetFrustumSize(nearVector, farVector);
        drawQueue_.AddShaderParameter(VSP_FRUSTUMSIZE, farVector);

        drawQueue_.AddShaderParameter(VSP_VIEWPROJ, camera_.GetEffectiveGPUViewProjection(constantDepthBias));

        const Color ambientColorGamma = camera_.GetEffectiveAmbientColor() * camera_.GetEffectiveAmbientBrightness();
        drawQueue_.AddShaderParameter(PSP_AMBIENTCOLOR,
            settings_.gammaCorrection_ ? ambientColorGamma.GammaToLinear() : ambientColorGamma);
        drawQueue_.AddShaderParameter(PSP_FOGCOLOR, camera_.GetEffectiveFogColor());
        drawQueue_.AddShaderParameter(PSP_FOGPARAMS, GetFogParameter(camera_));
    }

    void AddVertexLightConstants()
    {
        drawQueue_.AddShaderParameter(VSP_VERTEXLIGHTS, ea::span<const Vector4>{ current_.vertexLightsData_ });
    }

    void AddPixelLightConstants(const LightShaderParameters& params)
    {
        drawQueue_.AddShaderParameter(VSP_LIGHTDIR, params.direction_);
        drawQueue_.AddShaderParameter(VSP_LIGHTPOS,
            Vector4{ params.position_, params.invRange_ });
        drawQueue_.AddShaderParameter(PSP_LIGHTCOLOR,
            Vector4{ params.GetColor(settings_.gammaCorrection_), params.specularIntensity_ });

        drawQueue_.AddShaderParameter(PSP_LIGHTRAD, params.radius_);
        drawQueue_.AddShaderParameter(PSP_LIGHTLENGTH, params.length_);

        if (params.numLightMatrices_ > 0)
        {
            ea::span<const Matrix4> shadowMatricesSpan{ params.lightMatrices_, params.numLightMatrices_ };
            drawQueue_.AddShaderParameter(VSP_LIGHTMATRICES, shadowMatricesSpan);
        }

        if (params.shadowMap_)
        {
            drawQueue_.AddShaderParameter(PSP_SHADOWDEPTHFADE, params.shadowDepthFade_);
            drawQueue_.AddShaderParameter(PSP_SHADOWINTENSITY, params.shadowIntensity_);
            drawQueue_.AddShaderParameter(PSP_SHADOWMAPINVSIZE, params.shadowMapInvSize_);
            drawQueue_.AddShaderParameter(PSP_SHADOWSPLITS, params.shadowSplits_);
            drawQueue_.AddShaderParameter(PSP_SHADOWCUBEUVBIAS, params.shadowCubeUVBias_);
            drawQueue_.AddShaderParameter(PSP_SHADOWCUBEADJUST, params.shadowCubeAdjust_);
            drawQueue_.AddShaderParameter(PSP_VSMSHADOWPARAMS, settings_.vsmShadowParams_);
        }
    }

    void AddObjectConstants()
    {
        if (enabled_.ambientLighting_)
        {
            if (settings_.ambientMode_ == AmbientMode::Flat)
                drawQueue_.AddShaderParameter(VSP_AMBIENT, object_.ambient_);
            else if (settings_.ambientMode_ == AmbientMode::Directional)
            {
                const SphericalHarmonicsDot9& sh = *object_.sh_;
                drawQueue_.AddShaderParameter(VSP_SHAR, sh.Ar_);
                drawQueue_.AddShaderParameter(VSP_SHAG, sh.Ag_);
                drawQueue_.AddShaderParameter(VSP_SHAB, sh.Ab_);
                drawQueue_.AddShaderParameter(VSP_SHBR, sh.Br_);
                drawQueue_.AddShaderParameter(VSP_SHBG, sh.Bg_);
                drawQueue_.AddShaderParameter(VSP_SHBB, sh.Bb_);
                drawQueue_.AddShaderParameter(VSP_SHC, sh.C_);
            }
        }

        switch (object_.geometryType_)
        {
        case GEOM_SKINNED:
            drawQueue_.AddShaderParameter(VSP_SKINMATRICES,
                ea::span<const Matrix3x4>(object_.worldTransform_, object_.numWorldTransforms_));
            break;

        case GEOM_BILLBOARD:
            drawQueue_.AddShaderParameter(VSP_MODEL, *object_.worldTransform_);
            if (object_.numWorldTransforms_ > 1)
                drawQueue_.AddShaderParameter(VSP_BILLBOARDROT, object_.worldTransform_[1].RotationMatrix());
            else
                drawQueue_.AddShaderParameter(VSP_BILLBOARDROT, cameraNode_.GetWorldRotation().RotationMatrix());
            break;

        default:
            drawQueue_.AddShaderParameter(VSP_MODEL, *object_.worldTransform_);
            break;
        }
    }

    void AddObjectInstanceData()
    {
        instancingBuffer_.SetElements(object_.worldTransform_, 0, 3);
        if (enabled_.ambientLighting_)
        {
            if (settings_.ambientMode_ == AmbientMode::Flat)
                instancingBuffer_.SetElements(&object_.ambient_, 3, 1);
            else if (settings_.ambientMode_ == AmbientMode::Directional)
                instancingBuffer_.SetElements(object_.sh_, 3, 7);
        }
    }
    /// @}

    /// Draw ops
    /// @{
    void DrawObject()
    {
        IndexBuffer* indexBuffer = current_.geometry_->GetIndexBuffer();
        if (dirty_.geometry_)
        {
            drawQueue_.SetBuffers({ current_.geometry_->GetVertexBuffers(), indexBuffer, nullptr });
            dirty_.geometry_ = false;
        }

        if (indexBuffer != nullptr)
            drawQueue_.DrawIndexed(current_.geometry_->GetIndexStart(), current_.geometry_->GetIndexCount());
        else
            drawQueue_.Draw(current_.geometry_->GetVertexStart(), current_.geometry_->GetVertexCount());
    }

    void DrawObjectInstanced()
    {
        assert(instancingGroup_.count_ > 0);
        Geometry* geometry = instancingGroup_.geometry_;
        drawQueue_.SetBuffers({ geometry->GetVertexBuffers(), geometry->GetIndexBuffer(),
            instancingBuffer_.GetVertexBuffer() });
        drawQueue_.DrawIndexedInstanced(geometry->GetIndexStart(), geometry->GetIndexCount(),
            instancingGroup_.start_, instancingGroup_.count_);
        instancingGroup_.count_ = 0;
    }
    /// @}

    void ProcessBatch(const PipelineBatch& pipelineBatch, const SourceBatch& sourceBatch)
    {
        const LightAccumulator* lightAccumulator = enabled_.ambientLighting_ || enabled_.vertexLighting_
            ? &drawableProcessor_.GetGeometryLighting(pipelineBatch.drawableIndex_)
            : nullptr;

        ExtractObjectConstants(sourceBatch, lightAccumulator);

        // Update dirty flags and cached state
        CheckDirtyCommonState(pipelineBatch);
        if (enabled_.pixelLighting_)
            CheckDirtyPixelLight(pipelineBatch);
        if (enabled_.vertexLighting_)
            CheckDirtyVertexLight(*lightAccumulator);
        if (enabled_.ambientLighting_)
            CheckDirtyLightmap(sourceBatch);

        const bool resetInstancingGroup = instancingGroup_.count_ == 0 || dirty_.IsAnyStateDirty();
        if (resetInstancingGroup)
        {
            if (instancingGroup_.count_ > 0)
                DrawObjectInstanced();

            UpdateDirtyConstants();
            UpdateDirtyResources();

            const bool beginInstancingGroup = enabled_.staticInstancing_
                && pipelineBatch.geometryType_ == GEOM_STATIC
                && pipelineBatch.geometry_->GetIndexBuffer() != nullptr;
            if (beginInstancingGroup)
            {
                instancingGroup_.count_ = 1;
                instancingGroup_.start_ = instancingBuffer_.AddInstance();
                instancingGroup_.geometry_ = current_.geometry_;
                AddObjectInstanceData();
            }
            else
            {
                drawQueue_.BeginShaderParameterGroup(SP_OBJECT, true);
                AddObjectConstants();
                drawQueue_.CommitShaderParameterGroup(SP_OBJECT);

                DrawObject();
            }
        }
        else
        {
            ++instancingGroup_.count_;
            instancingBuffer_.AddInstance();
            AddObjectInstanceData();
        }
    }

    /// Destination draw queue.
    DrawCommandQueue& drawQueue_;

    /// External state (required)
    /// @{
    const BatchRendererSettings& settings_;
    const DrawableProcessor& drawableProcessor_;
    InstancingBufferCompositor& instancingBuffer_;
    const FrameInfo& frameInfo_;
    // TODO(renderer): Make it immutable so we can safely execute this code in multiple threads
    Scene& scene_;
    const ea::vector<LightProcessor*>& lights_;
    const Camera& camera_;
    const Node& cameraNode_;
    /// @}

    /// External state (optional)
    /// @{
    const ShadowSplitProcessor* const outputShadowSplit_{};
    Vector4 geometryBufferOffsetAndScale_;
    Vector2 geometryBufferInvSize_;
    ea::span<const ShaderResourceDesc> globalResources_;
    /// @}

    struct EnabledFeatureFlags
    {
        EnabledFeatureFlags(BatchRenderFlags flags)
            : ambientLighting_(flags.Test(BatchRenderFlag::AmbientLight))
            , vertexLighting_(flags.Test(BatchRenderFlag::VertexLights))
            , pixelLighting_(flags.Test(BatchRenderFlag::PixelLight))
            , anyLighting_(ambientLighting_ || vertexLighting_ || pixelLighting_)
            , staticInstancing_(flags.Test(BatchRenderFlag::InstantiateStaticGeometry))
        {
        }

        const bool ambientLighting_;
        const bool vertexLighting_;
        const bool pixelLighting_;
        const bool anyLighting_;
        const bool staticInstancing_;
    } const enabled_;

    struct DirtyStateFlags
    {
        bool pipelineState_{};
        bool frameConstants_{ true };
        bool cameraConstants_{ true };

        bool pixelLightConstants_{};
        bool pixelLightTextures_{};

        bool vertexLightConstants_{};

        bool lightmapConstants_{};
        bool lightmapTextures_{};

        bool material_{};
        bool geometry_{};

        bool IsAnyStateDirty() const
        {
            return pipelineState_ || frameConstants_ || cameraConstants_
                || pixelLightConstants_ || pixelLightTextures_ || vertexLightConstants_
                || lightmapConstants_ || lightmapTextures_ || material_ || geometry_;
        }
    } dirty_;

    struct CachedSharedState
    {
        PipelineState* pipelineState_{};
        float constantDepthBias_{};

        unsigned pixelLightIndex_{ M_MAX_UNSIGNED };
        bool pixelLightEnabled_{};
        const LightShaderParameters* pixelLightParams_{};
        Texture* pixelLightRamp_{};
        Texture* pixelLightShape_{};
        Texture* pixelLightShadowMap_{};

        LightAccumulator::VertexLightContainer vertexLights_{};
        Vector4 vertexLightsData_[MAX_VERTEX_LIGHTS * 3]{};

        Texture* lightmapTexture_{};
        const Vector4* lightmapScaleOffset_{};

        Material* material_{};
        Geometry* geometry_{};
    } current_;

    struct ObjectState
    {
        const SphericalHarmonicsDot9* sh_{};
        Vector4 ambient_;
        GeometryType geometryType_{};
        const Matrix3x4* worldTransform_{};
        unsigned numWorldTransforms_{};
    } object_;

    struct InstancingGroupState
    {
        Geometry* geometry_{};
        unsigned start_{};
        unsigned count_{};
    } instancingGroup_;

    struct LightVolumeBatchHelpers
    {
        Matrix3x4 transform_;
        SourceBatch sourceBatch_;
    } lightVolumeHelpers_;
};

}

BatchRenderer::BatchRenderer(Context* context, const DrawableProcessor* drawableProcessor,
    InstancingBufferCompositor* instancingBuffer)
    : Object(context)
    , renderer_(context_->GetSubsystem<Renderer>())
    , drawableProcessor_(drawableProcessor)
    , instancingBuffer_(instancingBuffer)
{
}

void BatchRenderer::SetSettings(const BatchRendererSettings& settings)
{
    settings_ = settings;
}

void BatchRenderer::RenderBatches(DrawCommandQueue& drawQueue, const Camera* camera, BatchRenderFlags flags,
    ea::span<const PipelineBatchByState> batches, const ShadowSplitProcessor* outputShadowSplit)
{
    DrawCommandCompositor compositor(drawQueue, settings_,
        *drawableProcessor_, *instancingBuffer_, camera, flags, outputShadowSplit);

    for (const auto& sortedBatch : batches)
        compositor.ProcessSceneBatch(*sortedBatch.pipelineBatch_);
    compositor.FlushDrawCommands();
}

void BatchRenderer::RenderBatches(DrawCommandQueue& drawQueue, const Camera* camera, BatchRenderFlags flags,
    ea::span<const PipelineBatchBackToFront> batches, const ShadowSplitProcessor* outputShadowSplit)
{
    DrawCommandCompositor compositor(drawQueue, settings_,
        *drawableProcessor_, *instancingBuffer_, camera, flags, outputShadowSplit);

    for (const auto& sortedBatch : batches)
        compositor.ProcessSceneBatch(*sortedBatch.pipelineBatch_);
    compositor.FlushDrawCommands();
}

void BatchRenderer::RenderLightVolumeBatches(DrawCommandQueue& drawQueue, Camera* camera,
    const LightVolumeRenderContext& ctx, ea::span<const PipelineBatchByState> batches)
{
    DrawCommandCompositor compositor(drawQueue, settings_,
        *drawableProcessor_, *instancingBuffer_, camera, BatchRenderFlag::PixelLight, nullptr);

    compositor.SetGBufferParameters(ctx.geometryBufferOffsetAndScale_, ctx.geometryBufferInvSize_);
    compositor.SetGlobalResources(ctx.geometryBuffer_);

    for (const auto& sortedBatch : batches)
        compositor.ProcessLightVolumeBatch(*sortedBatch.pipelineBatch_);
    compositor.FlushDrawCommands();
}

}
