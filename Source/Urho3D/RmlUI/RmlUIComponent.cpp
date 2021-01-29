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
#include "../IO/Log.h"
#include "../Resource/BinaryFile.h"
#include "../Graphics/Material.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../RmlUI/RmlUI.h"
#include "../RmlUI/RmlUIComponent.h"
#include "../RmlUI/RmlCanvasComponent.h"
#include "../Scene/Node.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* RML_UI_CATEGORY;

RmlUIComponent::RmlUIComponent(Context* context)
    : LogicComponent(context)
{
    SetUpdateEventMask(USE_UPDATE);
}

RmlUIComponent::~RmlUIComponent()
{
    CloseInternal();
}

void RmlUIComponent::RegisterObject(Context* context)
{
    context->RegisterFactory<RmlUIComponent>(RML_UI_CATEGORY);
    URHO3D_COPY_BASE_ATTRIBUTES(BaseClassName);
    URHO3D_ACCESSOR_ATTRIBUTE("Resource", GetResource, SetResource, ResourceRef, ResourceRef{BinaryFile::GetTypeStatic()}, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Use Normalized Coordinates", bool, useNormalized_, false, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Position", GetPosition, SetPosition, Vector2, Vector2::ZERO, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Size", GetSize, SetSize, Vector2, Vector2::ZERO, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Auto Size", bool, autoSize_, true, AttributeMode::Default);
    URHO3D_ACCESSOR_ATTRIBUTE("Is Open", IsOpen, SetOpen, bool, false, AttributeMode::Default);
}

void RmlUIComponent::OnNodeSet(Node* node)
{
    if (node)
    {
        SubscribeToEvent(node->GetScene(), E_COMPONENTADDED, &RmlUIComponent::OnComponentAdded);
        SubscribeToEvent(node->GetScene(), E_COMPONENTREMOVED, &RmlUIComponent::OnComponentRemoved);
    }
    else
    {
        UnsubscribeFromEvent(E_COMPONENTADDED);
        UnsubscribeFromEvent(E_COMPONENTREMOVED);
    }

    canvasComponent_ = GetComponent<RmlCanvasComponent>();

    if (open_ && node != nullptr)
        OpenInternal();
    else
        CloseInternal();
}

void RmlUIComponent::SetResource(const ResourceRef& resourceRef)
{
    resource_ = resourceRef;
    if (resource_.type_ == StringHash::ZERO)
        resource_.type_ = BinaryFile::GetTypeStatic();
    SetOpen(open_);
}

void RmlUIComponent::SetOpen(bool open)
{
    if (open)
    {
        if (node_ != nullptr && !resource_.name_.empty())
        {
            OpenInternal();
            open_ = document_ != nullptr;
        }
    }
    else
    {
        CloseInternal();
        open_ = false;
    }
}

void RmlUIComponent::OpenInternal()
{
    if (document_ != nullptr)
        return; // Already open.

    if (resource_.name_.empty())
    {
        URHO3D_LOGERROR("UI document can be opened after setting resource path.");
        return;
    }

    RmlUI* ui = GetUI();
    ui->documentClosedEvent_.Subscribe(this, &RmlUIComponent::OnDocumentClosed);
    ui->canvasResizedEvent_.Subscribe(this, &RmlUIComponent::OnUICanvasResized);
    ui->documentReloaded_.Subscribe(this, &RmlUIComponent::OnDocumentReloaded);

    document_ = ui->LoadDocument(resource_.name_);
    SetPosition(position_);
    SetSize(size_);
    document_->Show();
}

void RmlUIComponent::CloseInternal()
{
    if (document_ == nullptr)
        return; // Already closed.

    RmlUI* ui = static_cast<Detail::RmlContext*>(document_->GetContext())->GetOwnerSubsystem();
    ui->documentClosedEvent_.Unsubscribe(this);
    ui->canvasResizedEvent_.Unsubscribe(this);
    ui->documentReloaded_.Unsubscribe(this);

    position_ = GetPosition();
    size_ = GetSize();
    document_->Close();
    document_ = nullptr;
}

void RmlUIComponent::OnDocumentClosed(Rml::ElementDocument* document)
{
    if (document_ == document)
    {
        document_ = nullptr;
        open_ = false;
    }
}

Vector2 RmlUIComponent::GetPosition() const
{
    if (document_ == nullptr)
        return position_;

    Vector2 pos = document_->GetAbsoluteOffset(Rml::Box::BORDER);
    if (useNormalized_)
    {
        IntVector2 canvasSize = document_->GetContext()->GetDimensions();
        pos.x_ = 1.0f / static_cast<float>(canvasSize.x_) * pos.x_;
        pos.y_ = 1.0f / static_cast<float>(canvasSize.y_) * pos.y_;
    }
    return pos;
}

void RmlUIComponent::SetPosition(Vector2 pos)
{
    if (document_ == nullptr)
    {
        position_ = pos;
        return;
    }

    if (pos == Vector2::ZERO)
        return;

    if (useNormalized_)
    {
        IntVector2 canvasSize = document_->GetContext()->GetDimensions();
        pos.x_ = Round(static_cast<float>(canvasSize.x_) * pos.x_);
        pos.y_ = Round(static_cast<float>(canvasSize.y_) * pos.y_);
    }
    document_->SetProperty(Rml::PropertyId::Left, Rml::Property(pos.x_, Rml::Property::PX));
    document_->SetProperty(Rml::PropertyId::Top, Rml::Property(pos.y_, Rml::Property::PX));
    document_->UpdateDocument();
}

Vector2 RmlUIComponent::GetSize() const
{
    if (document_ == nullptr)
        return size_;

    if (autoSize_)
        return Vector2::ZERO;

    Vector2 size = document_->GetBox().GetSize(Rml::Box::CONTENT);
    if (useNormalized_)
    {
        IntVector2 canvasSize = document_->GetContext()->GetDimensions();
        size.x_ = 1.0f / static_cast<float>(canvasSize.x_) * size.x_;
        size.y_ = 1.0f / static_cast<float>(canvasSize.y_) * size.y_;
    }
    return size;
}

void RmlUIComponent::SetSize(Vector2 size)
{
    if (document_ == nullptr)
    {
        size_ = size;
        return;
    }

    if (size == Vector2::ZERO || autoSize_)
        return;

    if (useNormalized_)
    {
        IntVector2 canvasSize = document_->GetContext()->GetDimensions();
        size.x_ = Round(static_cast<float>(canvasSize.x_) * size.x_);
        size.y_ = Round(static_cast<float>(canvasSize.y_) * size.y_);
    }
    document_->SetProperty(Rml::PropertyId::Width, Rml::Property(size.x_, Rml::Property::PX));
    document_->SetProperty(Rml::PropertyId::Height, Rml::Property(size.y_, Rml::Property::PX));
    document_->UpdateDocument();
}

void RmlUIComponent::OnUICanvasResized(const RmlCanvasResizedArgs& args)
{
    if (!useNormalized_)
        // Element is positioned using absolute pixel values. Nothing to adjust.
        return;

    // When using normalized coordinates, element is positioned relative to canvas size. When canvas is resized old positions are no longer
    // valid. Convert pixel size and position back to normalized coordiantes using old size and reapply them, which will use new canvas size
    // for calculating new pixel position and size.

    Vector2 pos = document_->GetAbsoluteOffset(Rml::Box::BORDER);
    Vector2 size = document_->GetBox().GetSize(Rml::Box::CONTENT);

    pos.x_ = 1.0f / static_cast<float>(args.oldSize_.x_) * pos.x_;
    pos.y_ = 1.0f / static_cast<float>(args.oldSize_.y_) * pos.y_;
    size.x_ = 1.0f / static_cast<float>(args.oldSize_.x_) * size.x_;
    size.y_ = 1.0f / static_cast<float>(args.oldSize_.y_) * size.y_;

    SetPosition(pos);
    SetSize(size);
}

void RmlUIComponent::OnDocumentReloaded(const RmlDocumentReloadedArgs& args)
{
    if (document_ == args.unloadedDocument_)
        document_ = args.loadedDocument_;
}

void RmlUIComponent::SetResource(const eastl::string& resourceName)
{
    ResourceRef resourceRef(BinaryFile::GetTypeStatic(), resourceName);
    SetResource(resourceRef);
}

void RmlUIComponent::OnComponentAdded(StringHash, VariantMap& args)
{
    using namespace ComponentAdded;

    Node* node = static_cast<Node*>(args[P_NODE].GetPtr());
    if (node != node_)
        return;

    if (canvasComponent_.NotNull())
        // Window is rendered into some component already.
        return;

    Component* addedComponent = static_cast<Component*>(args[P_COMPONENT].GetPtr());
    if (addedComponent->IsInstanceOf<RmlCanvasComponent>())
    {
        CloseInternal();
        canvasComponent_ = addedComponent->Cast<RmlCanvasComponent>();
        OpenInternal();
    }
}

void RmlUIComponent::OnComponentRemoved(StringHash, VariantMap& args)
{
    using namespace ComponentRemoved;

    Node* node = static_cast<Node*>(args[P_NODE].GetPtr());
    if (node != node_)
        return;

    Component* removedComponent = static_cast<Component*>(args[P_COMPONENT].GetPtr());

    // Reopen this window in a new best fitting RmlUI instance.
    if (removedComponent == canvasComponent_)
    {
        CloseInternal();
        canvasComponent_ = GetNode()->GetComponent<RmlCanvasComponent>();
        if (canvasComponent_ == removedComponent)
            canvasComponent_ = nullptr;
        OpenInternal();
    }
}

RmlUI* RmlUIComponent::GetUI() const
{
    if (canvasComponent_.NotNull())
        return canvasComponent_->GetUI();
    return GetSubsystem<RmlUI>();
}

}
