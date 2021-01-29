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
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../UI/TextRenderer3D.h"
#include "../UI/Font.h"

namespace Urho3D
{

extern const char* SUBSYSTEM_CATEGORY;

static const ea::string defaultFontName = "Fonts/Anonymous Pro.ttf";

TextRenderer3D::TextRenderer3D(Context* context)
    : LogicComponent(context)
{
    auto cache = context_->GetSubsystem<ResourceCache>();
    defaultFont_ = cache->GetResource<Font>(defaultFontName);
}

void TextRenderer3D::RegisterObject(Context* context)
{
    context->RegisterFactory<TextRenderer3D>(SUBSYSTEM_CATEGORY);

    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Default Font", GetDefaultFontAttr, SetDefaultFontAttr, ResourceRef, ResourceRef(Font::GetTypeStatic(), defaultFontName), AttributeMode::Default);
    URHO3D_ATTRIBUTE("Default Font Size", float, defaultFontSize_, DEFAULT_FONT_SIZE, AttributeMode::Default);
}

void TextRenderer3D::AddText3D(const Vector3& position, const Quaternion& rotation,
    const Color& color, const TextParams3D& params)
{
    QueuedTextElement desc;
    desc.position_ = position;
    desc.rotation_ = rotation;
    desc.color_ = color;
    desc.params_ = params;
    if (!desc.params_.font_)
        desc.params_.font_ = defaultFont_;
    desc.params_.RecalculateHash();
    queuedTextNodes_.push_back(desc);
}

void TextRenderer3D::DelayedStart()
{
    static const ea::string containerNodeName = "__TextRenderer3D__";
    Scene* scene = node_->GetScene();
    containerNode_ = scene->GetChild(containerNodeName);
    if (!containerNode_)
        containerNode_ = scene->CreateChild(containerNodeName, LOCAL, 0, true);
}

void TextRenderer3D::PostUpdate(float timeStep)
{
    const auto isCacheElementAvailable = [](const TextCache::value_type& element)
    {
        return !element.second.used_ && element.second.text_;
    };

    // Re-initialize if expired
    Node* containerNode = containerNode_;
    if (!containerNode)
        DelayedStart();

    // Try to find and reuse cached nodes
    newTextNodes_.clear();
    for (QueuedTextElement& queuedText : queuedTextNodes_)
    {
        auto iterRange = cachedTextNodes_.equal_range(queuedText.params_);
        auto iter = ea::find_if(iterRange.first, iterRange.second, isCacheElementAvailable);
        if (iter != iterRange.second)
        {
            iter->second.used_ = true;
            Text3D* text = iter->second.text_;
            Node* node = text->GetNode();
            node->SetWorldPosition(queuedText.position_);
            node->SetWorldRotation(queuedText.rotation_);
            text->SetColor(queuedText.color_);
        }
        else
        {
            newTextNodes_.push_back(ea::move(queuedText));
        }
    }
    queuedTextNodes_.clear();

    // Remove and hide all cache misses
    for (auto iter = cachedTextNodes_.begin(); iter != cachedTextNodes_.end();)
    {
        if (!iter->second.used_)
        {
            if (WeakPtr<Text3D> text = iter->second.text_)
            {
                text->SetEnabled(false);
                unusedCachedTextNodes_.push_back(text);
            }
            iter = cachedTextNodes_.erase(iter);
        }
        else
        {
            iter->second.used_ = false;
            ++iter;
        }
    }

    // Create new nodes or reuse cached nodes
    for (QueuedTextElement& queuedText : newTextNodes_)
    {
        Text3D* text = nullptr;

        // Try to reuse unused cached node
        if (!unusedCachedTextNodes_.empty())
        {
            text = unusedCachedTextNodes_.back();
            unusedCachedTextNodes_.pop_back();
        }

        // Create new node if cannot
        if (!text)
        {
            Node* newNode = containerNode_->CreateChild();
            text = newNode->CreateComponent<Text3D>();
        }

        // Initialize text
        Node* node = text->GetNode();
        node->SetWorldPosition(queuedText.position_);
        node->SetWorldRotation(queuedText.rotation_);
        text->SetColor(queuedText.color_);
        text->SetEnabled(true);

        text->SetText(queuedText.params_.text_);
        text->SetFont(queuedText.params_.font_);
        text->SetFontSize(queuedText.params_.fontSize_);
        text->SetFaceCameraMode(queuedText.params_.faceCamera_);
        text->SetFixedScreenSize(queuedText.params_.fixedScreenSize_);
        text->SetSnapToPixels(queuedText.params_.snapToPixels_);
        text->SetHorizontalAlignment(queuedText.params_.horizontalAlignment_);
        text->SetVerticalAlignment(queuedText.params_.verticalAlignment_);
        text->SetTextAlignment(queuedText.params_.textAlignment_);

        CachedTextElement cachedText;
        cachedText.text_ = text;
        cachedTextNodes_.emplace(queuedText.params_, cachedText);
    }
}

void TextRenderer3D::SetDefaultFontAttr(const ResourceRef& value)
{
    auto cache = GetSubsystem<ResourceCache>();
    defaultFont_ = cache->GetResource<Font>(value.name_);
}

ResourceRef TextRenderer3D::GetDefaultFontAttr() const
{
    return GetResourceRef(defaultFont_, Font::GetTypeStatic());
}

}
