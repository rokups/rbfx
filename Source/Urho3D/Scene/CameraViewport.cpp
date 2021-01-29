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

#include <EASTL/sort.h>

#include "../Core/CoreEvents.h"
#include "../Core/StringUtils.h"
#include "../Engine/PluginApplication.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/RenderPath.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Scene/CameraViewport.h"
#include "../Scene/Node.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"


namespace Urho3D
{

static ResourceRef defaultRenderPath{XMLFile::GetTypeStatic(), "RenderPaths/Forward.xml"};

CameraViewport::CameraViewport(Context* context)
    : Component(context)
    , viewport_(context->CreateObject<Viewport>())
    , rect_(fullScreenViewport)
    , renderPath_(defaultRenderPath)
    , screenRect_{0, 0, 1920, 1080}
{
    if (Graphics* graphics = context_->GetSubsystem<Graphics>())
        screenRect_ = {0, 0, graphics->GetWidth(), graphics->GetHeight()};
}

void CameraViewport::SetNormalizedRect(const Rect& rect)
{
    rect_ = rect;
    IntRect viewportRect(
        static_cast<int>(screenRect_.Left() + screenRect_.Width() * rect.Left()),
        static_cast<int>(screenRect_.Top() + screenRect_.Height() * rect.Top()),
        static_cast<int>(screenRect_.Left() + screenRect_.Width() * rect.Right()),
        static_cast<int>(screenRect_.Top() + screenRect_.Height() * rect.Bottom())
    );
    viewport_->SetRect(viewportRect);

    using namespace CameraViewportResized;
    VariantMap args{};
    args[P_VIEWPORT] = GetViewport();
    args[P_CAMERA] = GetViewport()->GetCamera();
    args[P_SIZE] = viewportRect;
    args[P_SIZENORM] = rect;
    SendEvent(E_CAMERAVIEWPORTRESIZED, args);
}

void CameraViewport::RegisterObject(Context* context)
{
    context->RegisterFactory<CameraViewport>("Scene");
}

void CameraViewport::OnNodeSet(Node* node)
{
    if (node == nullptr)
        viewport_->SetCamera(nullptr);
    else
    {
        SubscribeToEvent(node, E_COMPONENTADDED, [this](StringHash, VariantMap& args) {
            using namespace ComponentAdded;
            if (Component* component = static_cast<Component*>(args[P_COMPONENT].GetPtr()))
            {
                if (Camera* camera = component->Cast<Camera>())
                {
                    viewport_->SetCamera(camera);
                    camera->SetViewMask(camera->GetViewMask() & ~(1U << 31U));   // Do not render last layer.
                }
            }
        });
        SubscribeToEvent(node, E_COMPONENTREMOVED, [this](StringHash, VariantMap& args) {
            using namespace ComponentRemoved;
            if (Component* component = static_cast<Component*>(args[P_COMPONENT].GetPtr()))
            {
                if (component->GetType() == Camera::GetTypeStatic())
                    viewport_->SetCamera(nullptr);
            }
        });

        if (Camera* camera = node->GetComponent<Camera>())
        {
            viewport_->SetCamera(camera);
            camera->SetViewMask(camera->GetViewMask() & ~(1U << 31U));   // Do not render last layer.
        }
        else
        {
            // If this node does not have a camera - get or create it on next frame. This is required because Camera may
            // be created later when deserializing node.
            SubscribeToEvent(E_BEGINFRAME, [this](StringHash, VariantMap& args) {
                if (Node* node = GetNode())
                {
                    if (Camera* camera = node->GetOrCreateComponent<Camera>())
                    {
                        viewport_->SetCamera(camera);
                        camera->SetViewMask(camera->GetViewMask() & ~(1U << 31U));   // Do not render last layer.
                    }
                }
                UnsubscribeFromEvent(E_BEGINFRAME);
            });
        }
    }
}

void CameraViewport::OnSceneSet(Scene* scene)
{
    viewport_->SetScene(scene);
}

IntRect CameraViewport::GetScreenRect() const
{
    return screenRect_;
}

const ea::vector<AttributeInfo>* CameraViewport::GetAttributes() const
{
    if (attributesDirty_)
        const_cast<CameraViewport*>(this)->RebuildAttributes();
    return &attributes_;
}

template<typename T>
AttributeInfo& CameraViewport::RegisterAttribute(const AttributeInfo& attr)
{
    attributes_.push_back(attr);
    return attributes_.back();
}

void CameraViewport::RebuildAttributes()
{
    auto* context = this;
    // Normal attributes.
    URHO3D_ACCESSOR_ATTRIBUTE("Viewport", GetNormalizedRect, SetNormalizedRect, Rect, fullScreenViewport, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("RenderPath", GetLastRenderPath, SetRenderPath, ResourceRef, defaultRenderPath, AttributeMode::Default);

    // PostProcess effects are special. One file may contain multiple effects that can be enabled or disabled.
    {
        effects_.clear();
        for (const auto& dir: context_->GetSubsystem<ResourceCache>()->GetResourceDirs())
        {
            ea::vector<ea::string> effects;
            ea::string resourcePath = "PostProcess/";
            ea::string scanDir = AddTrailingSlash(dir) + resourcePath;
            context_->GetSubsystem<FileSystem>()->ScanDir(effects, scanDir, "*.xml", SCAN_FILES, false);

            for (const auto& effectFileName: effects)
            {
                auto effectPath = resourcePath + effectFileName;
                auto* effect = context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effectPath);

                auto root = effect->GetRoot();
                ea::string tag;
                for (auto command = root.GetChild("command"); command.NotNull(); command = command.GetNext("command"))
                {
                    tag = command.GetAttribute("tag");

                    if (tag.empty())
                    {
                        URHO3D_LOGWARNING("Invalid PostProcess effect with empty tag");
                        continue;
                    }

                    if (effects_.find(tag) != effects_.end())
                        continue;

                    effects_[tag] = resourcePath + effectFileName;
                }
            }
        }

        StringVector tags = effects_.keys();
        ea::quick_sort(tags.begin(), tags.end());

        for (auto& effect : effects_)
        {
            auto getter = [this, &effect](const CameraViewport&, Variant& value) {
                if (RenderPath* renderPath = viewport_->GetRenderPath())
                    value = renderPath->IsEnabled(effect.first);
                else
                    value = false;
            };

            auto setter = [this, &effect](const CameraViewport&, const Variant& value) {
                RenderPath* path = viewport_->GetRenderPath();
                if (!path)
                    return;
                if (!path->IsAdded(effect.first))
                    path->Append(context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second));
                path->SetEnabled(effect.first, value.GetBool());
            };
            URHO3D_CUSTOM_ACCESSOR_ATTRIBUTE(effect.first.c_str(), getter, setter, bool, false, AttributeMode::Default);
        }
    }

    attributesDirty_ = false;
}

RenderPath* CameraViewport::RebuildRenderPath()
{
    if (!viewport_)
        return nullptr;

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    if (XMLFile* renderPathFile = context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(renderPath_.name_))
    {
        viewport_->SetRenderPath(renderPathFile);
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first))
            {
                if (!newRenderPath->IsAdded(effect.first))
                    newRenderPath->Append(context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second));
                newRenderPath->SetEnabled(effect.first, true);
            }
        }

        return newRenderPath;
    }

    return nullptr;
}

void CameraViewport::SetRenderPath(const ResourceRef& renderPathResource)
{
    if (!viewport_ || !context_->GetSubsystem<Graphics>())
        return;

    if (!renderPathResource.name_.empty() && renderPathResource.type_ != XMLFile::GetTypeStatic())
    {
        URHO3D_LOGWARNINGF("Incorrect RenderPath file '%s' type.", renderPathResource.name_.c_str());
        return;
    }

    SharedPtr<RenderPath> oldRenderPath(viewport_->GetRenderPath());

    const ea::string& renderPathFileName = renderPathResource.name_.empty() ? defaultRenderPath.name_ : renderPathResource.name_;
    if (XMLFile* renderPathFile = context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(renderPathFileName))
    {
        if (!viewport_->SetRenderPath(renderPathFile))
        {
            URHO3D_LOGERRORF("Loading renderpath from %s failed. File probably is not a renderpath.",
                renderPathFileName.c_str());
            return;
        }
        RenderPath* newRenderPath = viewport_->GetRenderPath();

        for (const auto& effect : effects_)
        {
            if (oldRenderPath->IsEnabled(effect.first))
            {
                if (!newRenderPath->IsAdded(effect.first))
                    newRenderPath->Append(context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(effect.second));
                newRenderPath->SetEnabled(effect.first, true);
            }
        }

        renderPath_.name_ = renderPathFileName;
    }
    else
    {
        URHO3D_LOGERRORF("Loading renderpath from %s failed. File is missing or you have no permissions to read it.",
                         renderPathFileName.c_str());
    }
}

void CameraViewport::UpdateViewport()
{
    SetNormalizedRect(GetNormalizedRect());
}

}
