//
// Copyright (c) 2017-2020 the rbfx project.
// Copyright (c) 2017 Eugene Kozlov
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

#include <Urho3D/Graphics/BillboardSet.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Toolbox/Scene/DebugCameraController.h>
#include "SceneTab.h"
#include "EditorEvents.h"
#include "Editor.h"
#include "EditorSceneSettings.h"


namespace Urho3D
{

static ResourceRef defaultRenderPath{XMLFile::GetTypeStatic(), "RenderPaths/Forward.xml"};

EditorSceneSettings::EditorSceneSettings(Context* context)
    : Component(context)
    , editorViewportRenderPath_(defaultRenderPath)
{
}

void EditorSceneSettings::RegisterObject(Context* context)
{
    context->RegisterFactory<EditorSceneSettings>();
    URHO3D_ACCESSOR_ATTRIBUTE("Viewport RenderPath", GetEditorViewportRenderPath, SetEditorViewportRenderPath, ResourceRef, defaultRenderPath, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Camera Position", GetCameraPosition, SetCameraPosition, Vector3, Vector3::ZERO, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Camera Orthographic Size", GetCameraOrthoSize, SetCameraOrthoSize, float, DEFAULT_ORTHOSIZE, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Camera Zoom", GetCameraZoom, SetCameraZoom, float, 1.f, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Camera Rotation", GetCameraRotation, SetCameraRotation, Quaternion, Quaternion::IDENTITY, AttributeMode::File | AttributeMode::NoEdit);
    URHO3D_ACCESSOR_ATTRIBUTE("Camera View 2D", GetCamera2D, SetCamera2D, bool, false, AttributeMode::File | AttributeMode::NoEdit);
}

Vector3 EditorSceneSettings::GetCameraPosition() const
{
    if (Node* node = GetCameraNode())
        return node->GetPosition();
    return Vector3::ZERO;
}

void EditorSceneSettings::SetCameraPosition(const Vector3& position)
{
    if (Node* node = GetCameraNode())
    {
        node->SetPosition(position);
    }
}

float EditorSceneSettings::GetCameraOrthoSize() const
{
    if (Node* node = GetCameraNode())
    {
        if (Camera* camera = node->GetComponent<Camera>())
            return camera->GetOrthoSize();
    }
    return 0;
}

void EditorSceneSettings::SetCameraOrthoSize(float size)
{
    if (Node* node = GetCameraNode())
    {
        if (Camera* camera = node->GetComponent<Camera>())
            camera->SetOrthoSize(size);
    }
}

float EditorSceneSettings::GetCameraZoom() const
{
    if (Node* node = GetCameraNode())
    {
        if (Camera* camera = node->GetComponent<Camera>())
            return camera->GetZoom();
    }
    return 0;
}

void EditorSceneSettings::SetCameraZoom(float zoom)
{
    if (Node* node = GetCameraNode())
    {
        if (Camera* camera = node->GetComponent<Camera>())
            camera->SetZoom(zoom);
    }
}

void EditorSceneSettings::OnSceneSet(Scene* scene)
{
    if (scene == nullptr)
        return;

    // When default viewport is used SetEditorViewportRenderPath() will not be called. This causes scene viewport to use
    // viewport renderpath of previous opened scene.
    SetEditorViewportRenderPath(editorViewportRenderPath_);

    Node* parent = scene->GetChild("__EditorObjects__");
    if (parent == nullptr)
    {
        parent = scene->CreateChild("__EditorObjects__", LOCAL);
        parent->AddTag("__EDITOR_OBJECT__");
        parent->SetTemporary(true);
    }

    Node* camera = parent->GetChild("__EditorCamera__");
    if (camera == nullptr)
    {
        camera = parent->CreateChild("__EditorCamera__", LOCAL);
        camera->AddTag("__EDITOR_OBJECT__");
    }

    auto* cameraComponent = camera->GetOrCreateComponent<Camera>();
    cameraComponent->SetFarClip(160000);

    auto* debug = scene->GetOrCreateComponent<DebugRenderer>(LOCAL);
    debug->SetView(cameraComponent);
    debug->SetTemporary(true);
    debug->SetLineAntiAlias(true);

    SetCamera2D(is2D_);
}

Node* EditorSceneSettings::GetCameraNode()
{
    return GetScene()->GetChild("__EditorCamera__", true);
}

Node* EditorSceneSettings::GetCameraNode() const
{
    return GetScene()->GetChild("__EditorCamera__", true);
}

bool EditorSceneSettings::GetCamera2D() const
{
    return is2D_;
}

void EditorSceneSettings::SetCamera2D(bool is2D)
{
    // Editor objects may not exist during deserialization. CreateEditorObjects() will call this
    // method again.
    if (Node* camera = GetCameraNode())
    {
        if (is2D)
        {
            camera->RemoveComponent<DebugCameraController3D>();
            auto* controller = camera->GetOrCreateComponent<DebugCameraController2D>();
            controller->SetUpdateEventMask(USE_NO_EVENT);
            Vector3 pos = camera->GetWorldPosition();
            camera->SetWorldPosition(pos);
            camera->LookAt(pos + Vector3::FORWARD);
            camera->GetComponent<Camera>()->SetOrthographic(true);
        }
        else
        {
            camera->RemoveComponent<DebugCameraController2D>();
            auto* controller = camera->GetOrCreateComponent<DebugCameraController3D>();
            controller->SetUpdateEventMask(USE_NO_EVENT);
            camera->GetComponent<Camera>()->SetOrthographic(false);
        }

        ea::vector<Node*> nodes;
        GetScene()->GetNodesWithTag(nodes, "DebugIcon");
        for (Node* node : nodes)
        {
            auto* billboard = node->GetComponent<BillboardSet>();
            if (!billboard)
                continue;

            if (is2D)
            {
                billboard->SetFaceCameraMode(FaceCameraMode::FC_NONE);
                node->LookAt(node->GetWorldPosition() + Vector3::FORWARD);
            }
            else
                billboard->SetFaceCameraMode(FaceCameraMode::FC_LOOKAT_XYZ);
            billboard->Commit();
        }
    }

    is2D_ = is2D;
}

Quaternion EditorSceneSettings::GetCameraRotation() const
{
    if (Node* camera = GetCameraNode())
        return camera->GetRotation();
    return Quaternion::IDENTITY;
}

void EditorSceneSettings::SetCameraRotation(const Quaternion& rotation)
{
    if (Node* camera = GetCameraNode())
        camera->SetRotation(rotation);
}

void EditorSceneSettings::SetEditorViewportRenderPath(const ResourceRef& renderPath)
{
    if (renderPath.type_ != XMLFile::GetTypeStatic())
    {
        URHO3D_LOGERROR("ResourceRef is not XMLFile.");
        return;
    }

    if (XMLFile* renderPathFile = context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(renderPath.name_))
    {
        if (auto* tab = GetSubsystem<Editor>()->GetTab<SceneTab>())
        {
            if (!tab->GetViewport()->SetRenderPath(renderPathFile))
                return;

            editorViewportRenderPath_ = renderPath;

            RenderPath* path = tab->GetViewport()->GetRenderPath();
            for (auto& command: path->commands_)
            {
                if (command.pixelShaderName_.starts_with("PBR"))
                {
                    XMLFile* gammaCorrection = context_->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(
                        "PostProcess/GammaCorrection.xml");
                    path->Append(gammaCorrection);
                    return;
                }
            }
        }
    }
}

}
