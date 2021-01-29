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

#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/SystemUI/SystemUI.h>
#include <Toolbox/SystemUI/Widgets.h>
#include "EditorIconCache.h"


namespace Urho3D
{

EditorIconCache::EditorIconCache(Context* context)
    : Object(context)
{
    ResourceCache* cache = ui::GetSystemUI()->GetSubsystem<ResourceCache>();
    auto* icons = cache->GetResource<XMLFile>("UI/EditorIcons.xml");
    if (icons == nullptr)
        return;

    XMLElement root = icons->GetRoot();
    for (XMLElement element = root.GetChild("element"); element.NotNull(); element = element.GetNext("element"))
    {
        ea::string type = element.GetAttribute("type");
        if (type.empty())
        {
            URHO3D_LOGERROR("EditorIconCache.xml contains icon entry without a \"type\" attribute.");
            continue;
        }

        XPathResultSet texture = element.SelectPrepared(XPathQuery("attribute[@name=\"Texture\"]"));
        if (texture.Empty())
        {
            URHO3D_LOGERROR("EditorIconCache.xml contains icon entry without a \"Texture\".");
            continue;
        }

        XPathResultSet rect = element.SelectPrepared(XPathQuery("attribute[@name=\"Image Rect\"]"));
        if (rect.Empty())
        {
            URHO3D_LOGERROR("EditorIconCache.xml contains icon entry without a \"Image Rect\".");
            continue;
        }

        IconData data;
        data.textureRef_ = texture.FirstResult().GetVariantValue(VariantType::ResourceRef).GetResourceRef();
        data.rect_ = rect.FirstResult().GetVariantValue(VariantType::IntRect).GetIntRect();
        iconCache_[type] = data;
    }
}

EditorIconCache::IconData* EditorIconCache::GetIconData(const ea::string& name)
{
    const auto iterIcon = iconCache_.find(name);
    if (iterIcon != iconCache_.end())
        return &iterIcon->second;

    const auto iterUnknown = iconCache_.find("Unknown");
    if (iterUnknown != iconCache_.end())
        return &iterUnknown->second;

    return nullptr;
}

}
