//
// Copyright (c) 2008-2020 the Urho3D project.
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
#include "../IO/Archive.h"
#include "../IO/ArchiveSerialization.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/JSONValue.h"
#include "../Resource/XMLElement.h"
#include "../Scene/Animatable.h"
#include "../Scene/ObjectAnimation.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/ValueAnimation.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* wrapModeNames[];

AttributeAnimationInfo::AttributeAnimationInfo(Animatable* animatable, const AttributeInfo& attributeInfo,
    ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed) :
    ValueAnimationInfo(animatable, attributeAnimation, wrapMode, speed),
    attributeInfo_(attributeInfo)
{
}

AttributeAnimationInfo::AttributeAnimationInfo(const AttributeAnimationInfo& other) = default;

AttributeAnimationInfo::~AttributeAnimationInfo() = default;

void AttributeAnimationInfo::ApplyValue(const Variant& newValue)
{
    auto* animatable = static_cast<Animatable*>(target_.Get());
    if (animatable)
    {
        animatable->OnSetAttribute(attributeInfo_, newValue);
        animatable->ApplyAttributes();
    }
}

Animatable::Animatable(Context* context) :
    Serializable(context),
    animationEnabled_(true)
{
}

Animatable::~Animatable() = default;

void Animatable::RegisterObject(Context* context)
{
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Object Animation", GetObjectAnimationAttr, SetObjectAnimationAttr, ResourceRef,
        ResourceRef(ObjectAnimation::GetTypeStatic()), AttributeMode::Default);
}

bool Animatable::Serialize(Archive& archive)
{
    if (ArchiveBlock block = archive.OpenUnorderedBlock("animatable"))
        return Serialize(archive, block);
    return false;
}

bool Animatable::Serialize(Archive& archive, ArchiveBlock& block)
{
    if (!Serializable::Serialize(archive, block))
        return false;

    if (archive.IsInput())
    {
        SetObjectAnimation(nullptr);
        attributeAnimationInfos_.clear();
    }

    // Object animation without name is serialized every time
    const bool uniqueObjectAnimation = objectAnimation_ && objectAnimation_->GetName().empty();
    SerializeOptional(archive, uniqueObjectAnimation,
        [&](bool loading)
    {
        if (ArchiveBlock block = archive.OpenUnorderedBlock("objectanimation"))
        {
            if (loading)
                objectAnimation_ = MakeShared<ObjectAnimation>(context_);
            return objectAnimation_->Serialize(archive, block);
        }
        return false;
    });

    // Count animations without owners
    unsigned numFreeAnimations = 0;
    for (const auto& elem : attributeAnimationInfos_)
    {
        if (!elem.second->GetAnimation()->GetOwner())
            ++numFreeAnimations;
    }

    SerializeCustomMap(archive, ArchiveBlockType::Map, "attributeanimation", numFreeAnimations, attributeAnimationInfos_,
        [&](unsigned /*index*/, const ea::string& name, const SharedPtr<AttributeAnimationInfo>& info, bool loading)
    {
        // Skip animations with owners
        SharedPtr<ValueAnimation> attributeAnimation{ info ? info->GetAnimation() : nullptr };
        if (attributeAnimation && attributeAnimation->GetOwner())
            return true;

        ea::string animationName = name;
        archive.SerializeKey(animationName);

        if (ArchiveBlock infoBlock = archive.OpenUnorderedBlock("attributeanimation"))
        {
            if (!attributeAnimation)
                attributeAnimation = MakeShared<ValueAnimation>(context_);

            attributeAnimation->Serialize(archive, infoBlock);

            WrapMode wrapMode = info ? info->GetWrapMode() : WM_LOOP;
            SerializeEnum(archive, "wrapmode", wrapModeNames, wrapMode);

            float speed = info ? info->GetSpeed() : 1.0f;
            SerializeValue(archive, "speed", speed);

            if (loading)
                SetAttributeAnimation(animationName, attributeAnimation, wrapMode, speed);

            return true;
        }
        return false;
    });

    return true;
}

bool Animatable::LoadXML(const XMLElement& source)
{
    if (!Serializable::LoadXML(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.clear();

    XMLElement elem = source.GetChild("objectanimation");
    if (elem)
    {
        SharedPtr<ObjectAnimation> objectAnimation(context_->CreateObject<ObjectAnimation>());
        if (!objectAnimation->LoadXML(elem))
            return false;

        SetObjectAnimation(objectAnimation.Get());
    }

    elem = source.GetChild("attributeanimation");
    while (elem)
    {
        ea::string name = elem.GetAttribute("name");
        SharedPtr<ValueAnimation> attributeAnimation(context_->CreateObject<ValueAnimation>());
        if (!attributeAnimation->LoadXML(elem))
            return false;

        ea::string wrapModeString = source.GetAttribute("wrapmode");
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = elem.GetFloat("speed");
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);

        elem = elem.GetNext("attributeanimation");
    }

    return true;
}

bool Animatable::LoadJSON(const JSONValue& source)
{
    if (!Serializable::LoadJSON(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.clear();

    JSONValue value = source.Get("objectanimation");
    if (!value.IsNull())
    {
        SharedPtr<ObjectAnimation> objectAnimation(context_->CreateObject<ObjectAnimation>());
        if (!objectAnimation->LoadJSON(value))
            return false;

        SetObjectAnimation(objectAnimation.Get());
    }

    JSONValue attributeAnimationValue = source.Get("attributeanimation");

    if (attributeAnimationValue.IsNull())
        return true;

    if (!attributeAnimationValue.IsObject())
    {
        URHO3D_LOGWARNING("'attributeanimation' value is present in JSON data, but is not a JSON object; skipping it");
        return true;
    }

    const JSONObject& attributeAnimationObject = attributeAnimationValue.GetObject();
    for (auto it = attributeAnimationObject.begin(); it != attributeAnimationObject.end(); it++)
    {
        ea::string name = it->first;
        JSONValue value = it->second;
        SharedPtr<ValueAnimation> attributeAnimation(context_->CreateObject<ValueAnimation>());
        if (!attributeAnimation->LoadJSON(it->second))
            return false;

        ea::string wrapModeString = value.Get("wrapmode").GetString();
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = value.Get("speed").GetFloat();
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);
    }

    return true;
}

bool Animatable::SaveXML(XMLElement& dest) const
{
    if (!Serializable::SaveXML(dest))
        return false;

    // Object animation without name
    if (objectAnimation_ && objectAnimation_->GetName().empty())
    {
        XMLElement elem = dest.CreateChild("objectanimation");
        if (!objectAnimation_->SaveXML(elem))
            return false;
    }


    for (auto i = attributeAnimationInfos_.begin();
         i != attributeAnimationInfos_.end(); ++i)
    {
        ValueAnimation* attributeAnimation = i->second->GetAnimation();
        if (attributeAnimation->GetOwner())
            continue;

        const AttributeInfo& attr = i->second->GetAttributeInfo();
        XMLElement elem = dest.CreateChild("attributeanimation");
        elem.SetAttribute("name", attr.name_);
        if (!attributeAnimation->SaveXML(elem))
            return false;

        elem.SetAttribute("wrapmode", wrapModeNames[i->second->GetWrapMode()]);
        elem.SetFloat("speed", i->second->GetSpeed());
    }

    return true;
}

bool Animatable::SaveJSON(JSONValue& dest) const
{
    if (!Serializable::SaveJSON(dest))
        return false;

    // Object animation without name
    if (objectAnimation_ && objectAnimation_->GetName().empty())
    {
        JSONValue objectAnimationValue;
        if (!objectAnimation_->SaveJSON(objectAnimationValue))
            return false;
        dest.Set("objectanimation", objectAnimationValue);
    }

    JSONValue attributeAnimationValue;

    for (auto i = attributeAnimationInfos_.begin();
         i != attributeAnimationInfos_.end(); ++i)
    {
        ValueAnimation* attributeAnimation = i->second->GetAnimation();
        if (attributeAnimation->GetOwner())
            continue;

        const AttributeInfo& attr = i->second->GetAttributeInfo();
        JSONValue attributeValue;
        attributeValue.Set("name", attr.name_);
        if (!attributeAnimation->SaveJSON(attributeValue))
            return false;

        attributeValue.Set("wrapmode", wrapModeNames[i->second->GetWrapMode()]);
        attributeValue.Set("speed", (float) i->second->GetSpeed());

        attributeAnimationValue.Set(attr.name_, attributeValue);
    }

    if (!attributeAnimationValue.IsNull())
        dest.Set("attributeanimation", attributeAnimationValue);

    return true;
}

void Animatable::SetAnimationEnabled(bool enable)
{
    if (objectAnimation_)
    {
        // In object animation there may be targets in hierarchy. Set same enable/disable state in all
        ea::hash_set<Animatable*> targets;
        const ea::unordered_map<ea::string, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto i = infos.begin(); i !=
            infos.end(); ++i)
        {
            ea::string outName;
            Animatable* target = FindAttributeAnimationTarget(i->first, outName);
            if (target && target != this)
                targets.insert(target);
        }

        for (auto i = targets.begin(); i != targets.end(); ++i)
            (*i)->animationEnabled_ = enable;
    }

    animationEnabled_ = enable;
}

void Animatable::SetAnimationTime(float time)
{
    if (objectAnimation_)
    {
        // In object animation there may be targets in hierarchy. Set same time in all
        const ea::unordered_map<ea::string, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto i = infos.begin(); i !=
            infos.end(); ++i)
        {
            ea::string outName;
            Animatable* target = FindAttributeAnimationTarget(i->first, outName);
            if (target)
                target->SetAttributeAnimationTime(outName, time);
        }
    }
    else
    {
        for (auto i = attributeAnimationInfos_.begin();
            i != attributeAnimationInfos_.end(); ++i)
            i->second->SetTime(time);
    }
}

void Animatable::SetObjectAnimation(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == objectAnimation_.Get())
        return;

    if (objectAnimation_)
    {
        OnObjectAnimationRemoved(objectAnimation_.Get());
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED);
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED);
    }

    objectAnimation_ = objectAnimation;

    if (objectAnimation_)
    {
        OnObjectAnimationAdded(objectAnimation_.Get());
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED, URHO3D_HANDLER(Animatable, HandleAttributeAnimationAdded));
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED, URHO3D_HANDLER(Animatable, HandleAttributeAnimationRemoved));
    }
}

void Animatable::SetAttributeAnimation(const ea::string& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);

    if (attributeAnimation)
    {
        if (info && attributeAnimation == info->GetAnimation())
        {
            info->SetWrapMode(wrapMode);
            info->SetSpeed(speed);
            return;
        }

        // Get attribute info
        const AttributeInfo* attributeInfo = nullptr;
        if (info)
            attributeInfo = &info->GetAttributeInfo();
        else
        {
            const ea::vector<AttributeInfo>* attributes = GetAttributes();
            if (!attributes)
            {
                URHO3D_LOGERROR(GetTypeName() + " has no attributes");
                return;
            }

            for (auto i = attributes->begin(); i != attributes->end(); ++i)
            {
                if (name == (*i).name_)
                {
                    attributeInfo = &(*i);
                    break;
                }
            }
        }

        if (!attributeInfo)
        {
            URHO3D_LOGERROR("Invalid name: " + name);
            return;
        }

        // Check value type is same with attribute type
        if (attributeAnimation->GetValueType() != attributeInfo->type_)
        {
            URHO3D_LOGERROR("Invalid value type");
            return;
        }

        // Add network attribute to set
        if (attributeInfo->mode_ & AttributeMode::Net)
            animatedNetworkAttributes_.insert(attributeInfo);

        attributeAnimationInfos_[name] = new AttributeAnimationInfo(this, *attributeInfo, attributeAnimation, wrapMode, speed);

        if (!info)
            OnAttributeAnimationAdded();
    }
    else
    {
        if (!info)
            return;

        // Remove network attribute from set
        if (info->GetAttributeInfo().mode_ & AttributeMode::Net)
            animatedNetworkAttributes_.erase(&info->GetAttributeInfo());

        attributeAnimationInfos_.erase(name);
        OnAttributeAnimationRemoved();
    }
}

void Animatable::SetAttributeAnimationWrapMode(const ea::string& name, WrapMode wrapMode)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetWrapMode(wrapMode);
}

void Animatable::SetAttributeAnimationSpeed(const ea::string& name, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetSpeed(speed);
}

void Animatable::SetAttributeAnimationTime(const ea::string& name, float time)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetTime(time);
}

void Animatable::RemoveObjectAnimation()
{
    SetObjectAnimation(nullptr);
}

void Animatable::RemoveAttributeAnimation(const ea::string& name)
{
    SetAttributeAnimation(name, nullptr);
}

ObjectAnimation* Animatable::GetObjectAnimation() const
{
    return objectAnimation_;
}

ValueAnimation* Animatable::GetAttributeAnimation(const ea::string& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetAnimation() : nullptr;
}

WrapMode Animatable::GetAttributeAnimationWrapMode(const ea::string& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetWrapMode() : WM_LOOP;
}

float Animatable::GetAttributeAnimationSpeed(const ea::string& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetSpeed() : 1.0f;
}

float Animatable::GetAttributeAnimationTime(const ea::string& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetTime() : 0.0f;
}

void Animatable::SetObjectAnimationAttr(const ResourceRef& value)
{
    if (!value.name_.empty())
    {
        auto* cache = GetSubsystem<ResourceCache>();
        SetObjectAnimation(cache->GetResource<ObjectAnimation>(value.name_));
    }
}

ResourceRef Animatable::GetObjectAnimationAttr() const
{
    return GetResourceRef(objectAnimation_, ObjectAnimation::GetTypeStatic());
}

Animatable* Animatable::FindAttributeAnimationTarget(const ea::string& name, ea::string& outName)
{
    // Base implementation only handles self
    outName = name;
    return this;
}

void Animatable::SetObjectAttributeAnimation(const ea::string& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    ea::string outName;
    Animatable* target = FindAttributeAnimationTarget(name, outName);
    if (target)
        target->SetAttributeAnimation(outName, attributeAnimation, wrapMode, speed);
}

void Animatable::OnObjectAnimationAdded(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Set all attribute animations from the object animation
    const ea::unordered_map<ea::string, SharedPtr<ValueAnimationInfo> >& attributeAnimationInfos = objectAnimation->GetAttributeAnimationInfos();
    for (auto i = attributeAnimationInfos.begin();
         i != attributeAnimationInfos.end(); ++i)
    {
        const ea::string& name = i->first;
        ValueAnimationInfo* info = i->second;
        SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
    }
}

void Animatable::OnObjectAnimationRemoved(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Just remove all attribute animations listed by the object animation
    const ea::unordered_map<ea::string, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation->GetAttributeAnimationInfos();
    for (auto i = infos.begin(); i !=
        infos.end(); ++i)
        SetObjectAttributeAnimation(i->first, nullptr, WM_LOOP, 1.0f);
}

void Animatable::UpdateAttributeAnimations(float timeStep)
{
    if (!animationEnabled_)
        return;

    // Keep weak pointer to self to check for destruction caused by event handling
    WeakPtr<Animatable> self(this);

    ea::vector<ea::string> finishedNames;
    for (auto i = attributeAnimationInfos_.begin();
         i != attributeAnimationInfos_.end(); ++i)
    {
        bool finished = i->second->Update(timeStep);
        // If self deleted as a result of an event sent during animation playback, nothing more to do
        if (self.Expired())
            return;

        if (finished)
            finishedNames.push_back(i->second->GetAttributeInfo().name_);
    }

    for (unsigned i = 0; i < finishedNames.size(); ++i)
        SetAttributeAnimation(finishedNames[i], nullptr);
}

bool Animatable::IsAnimatedNetworkAttribute(const AttributeInfo& attrInfo) const
{
    return animatedNetworkAttributes_.contains(&attrInfo);
}

AttributeAnimationInfo* Animatable::GetAttributeAnimationInfo(const ea::string& name) const
{
    auto i = attributeAnimationInfos_.find(
        name);
    if (i != attributeAnimationInfos_.end())
        return i->second;

    return nullptr;
}

void Animatable::HandleAttributeAnimationAdded(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationAdded;
    const ea::string& name = eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    ValueAnimationInfo* info = objectAnimation_->GetAttributeAnimationInfo(name);
    if (!info)
        return;

    SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
}

void Animatable::HandleAttributeAnimationRemoved(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationRemoved;
    const ea::string& name = eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    SetObjectAttributeAnimation(name, nullptr, WM_LOOP, 1.0f);
}

}
