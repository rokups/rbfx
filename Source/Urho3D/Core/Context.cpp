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
#include "../Core/Profiler.h"
#include "../IO/Log.h"

#include "../Audio/Audio.h"
#include "../Engine/Engine.h"
#include "../Core/WorkQueue.h"
#include "../Core/Thread.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Renderer.h"
#include "../IO/FileSystem.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/Localization.h"
#if URHO3D_NETWORK
#include "../Network/Network.h"
#endif
#include "../Input/Input.h"
#include "../UI/UI.h"
#if URHO3D_SYSTEMUI
#include "../SystemUI/SystemUI.h"
#endif

#ifndef MINI_URHO
#include <SDL/SDL.h>
#ifdef URHO3D_IK
#include <ik/log.h>
#include <ik/memory.h>
#endif
#endif

#include "../DebugNew.h"

namespace Urho3D
{

#ifndef MINI_URHO
// Keeps track of how many times SDL was initialised so we know when to call SDL_Quit().
static int sdlInitCounter = 0;

// Keeps track of how many times IK was initialised
static int ikInitCounter = 0;

// Reroute all messages from the ik library to the Urho3D log
static void HandleIKLog(const char* msg)
{
    URHO3D_LOGINFOF("[IK] %s", msg);
}
#endif

void EventReceiverGroup::BeginSendEvent()
{
    ++inSend_;
}

void EventReceiverGroup::EndSendEvent()
{
    assert(inSend_ > 0);
    --inSend_;

    if (inSend_ == 0 && dirty_)
    {
        /// \todo Could be optimized by erase-swap, but this keeps the receiver order
        for (unsigned i = receivers_.size() - 1; i < receivers_.size(); --i)
        {
            if (!receivers_[i])
                receivers_.erase_at(i);
        }

        dirty_ = false;
    }
}

void EventReceiverGroup::Add(Object* object)
{
    if (object)
        receivers_.push_back(object);
}

void EventReceiverGroup::Remove(Object* object)
{
    if (inSend_ > 0)
    {
        auto i = receivers_.find(object);
        if (i != receivers_.end())
        {
            (*i) = nullptr;
            dirty_ = true;
        }
    }
    else
        receivers_.erase_first(object);
}

void RemoveNamedAttribute(ea::unordered_map<StringHash, ea::vector<AttributeInfo> >& attributes, StringHash objectType, const char* name)
{
    auto i = attributes.find(objectType);
    if (i == attributes.end())
        return;

    ea::vector<AttributeInfo>& infos = i->second;

    for (auto j = infos.begin(); j != infos.end(); ++j)
    {
        if (!j->name_.comparei(name))
        {
            infos.erase(j);
            break;
        }
    }

    // If the vector became empty, erase the object type from the map
    if (infos.empty())
        attributes.erase(i);
}

Context::Context() :
    eventHandler_(nullptr)
{
#ifdef __ANDROID__
    // Always reset the random seed on Android, as the Urho3D library might not be unloaded between runs
    SetRandomSeed(1);
#endif

    // Set the main thread ID (assuming the Context is created in it)
    Thread::SetMainThread();
}

Context::~Context()
{
#ifndef MINI_URHO
    // Destroying resource cache does clear it, however some resources depend on resource cache being available when
    // destructor executes.
    if (auto* cache = GetSubsystem<ResourceCache>())
        cache->Clear();
#endif
    // Remove subsystems that use SDL in reverse order of construction, so that Graphics can shut down SDL last
    /// \todo Context should not need to know about subsystems
    RemoveSubsystem("Audio");
    RemoveSubsystem("UI");
    RemoveSubsystem("SystemUI");
    RemoveSubsystem("ResourceCache");
    RemoveSubsystem("Input");
    RemoveSubsystem("Renderer");
    RemoveSubsystem("Graphics");

    subsystems_.clear();
    factories_.clear();

    // Delete allocated event data maps
    for (auto i = eventDataMaps_.begin(); i != eventDataMaps_.end(); ++i)
        delete *i;
    eventDataMaps_.clear();
}

SharedPtr<Object> Context::CreateObject(StringHash objectType)
{
    auto i = factories_.find(objectType);
    if (i != factories_.end())
        return i->second->CreateObject();
    else
        return SharedPtr<Object>();
}

void Context::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;

    auto it = factories_.find(factory->GetType());
    if (it != factories_.end())
    {
        URHO3D_LOGERRORF("Failed to register '%s' because type '%s' is already registered with same type hash.",
            factory->GetTypeName().c_str(), it->second->GetTypeName().c_str());
        assert(false);
        return;
    }
    factories_[factory->GetType()] = factory;
}

void Context::RegisterFactory(ObjectFactory* factory, const char* category)
{
    if (!factory)
        return;

    RegisterFactory(factory);
    if (CStringLength(category))
        objectCategories_[category].push_back(factory->GetType());
}

void Context::RemoveFactory(StringHash type)
{
    factories_.erase(type);
}

void Context::RemoveFactory(StringHash type, const char* category)
{
    RemoveFactory(type);
    if (CStringLength(category))
        objectCategories_[category].erase_first_unsorted(type);
}

void Context::RegisterSubsystem(Object* object, StringHash type)
{
    if (!object)
        return;

    bool isTypeValid = false;
    for (const TypeInfo* typeInfo = object->GetTypeInfo(); typeInfo != nullptr && !isTypeValid; typeInfo = typeInfo->GetBaseTypeInfo())
        isTypeValid = typeInfo->GetType() == type;

    if (isTypeValid)
        subsystems_[type] = object;
    else
        URHO3D_LOGERROR("Type supplied to RegisterSubsystem() does not belong to object inheritance hierarchy.");
}

void Context::RegisterSubsystem(Object* object)
{
    if (!object)
        return;

    subsystems_[object->GetType()] = object;
}

void Context::RemoveSubsystem(StringHash objectType)
{
    auto i = subsystems_.find(objectType);
    if (i != subsystems_.end())
        subsystems_.erase(i);
}

AttributeHandle Context::RegisterAttribute(StringHash objectType, const AttributeInfo& attr)
{
    // None or pointer types can not be supported
    if (attr.type_ == VariantType::None || attr.type_ == VariantType::VoidPtr || attr.type_ == VariantType::Ptr)
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute type {} to class {}", Variant::GetTypeName(attr.type_),
            GetTypeName(objectType));
        return AttributeHandle();
    }

    // Only SharedPtr<> of Serializable or it's subclasses are supported in attributes
    if (attr.type_ == VariantType::Custom && !attr.defaultValue_.IsCustomType<SharedPtr<Serializable>>())
    {
        URHO3D_LOGWARNING("Attempt to register unsupported attribute of custom type to class {}", GetTypeName(objectType));
        return AttributeHandle();
    }

    AttributeHandle handle;

    ea::vector<AttributeInfo>& objectAttributes = attributes_[objectType];
    objectAttributes.push_back(attr);
    handle.attributeInfo_ = &objectAttributes.back();

    if (attr.mode_ & AttributeMode::Net)
    {
        ea::vector<AttributeInfo>& objectNetworkAttributes = networkAttributes_[objectType];
        objectNetworkAttributes.push_back(attr);
        handle.networkAttributeInfo_ = &objectNetworkAttributes.back();
    }
    return handle;
}

void Context::RemoveAttribute(StringHash objectType, const char* name)
{
    RemoveNamedAttribute(attributes_, objectType, name);
    RemoveNamedAttribute(networkAttributes_, objectType, name);
}

void Context::RemoveAllAttributes(StringHash objectType)
{
    attributes_.erase(objectType);
    networkAttributes_.erase(objectType);
}

void Context::UpdateAttributeDefaultValue(StringHash objectType, const char* name, const Variant& defaultValue)
{
    AttributeInfo* info = GetAttribute(objectType, name);
    if (info)
        info->defaultValue_ = defaultValue;
}

VariantMap& Context::GetEventDataMap()
{
    unsigned nestingLevel = eventSenders_.size();
    while (eventDataMaps_.size() < nestingLevel + 1)
        eventDataMaps_.push_back(new VariantMap());

    VariantMap& ret = *eventDataMaps_[nestingLevel];
    ret.clear();
    return ret;
}

#ifndef MINI_URHO
bool Context::RequireSDL(unsigned int sdlFlags)
{
    // Always increment, the caller must match with ReleaseSDL(), regardless of
    // what happens.
    ++sdlInitCounter;

    // Need to call SDL_Init() at least once before SDL_InitSubsystem()
    if (sdlInitCounter == 1)
    {
        URHO3D_LOGDEBUG("Initialising SDL");
        if (SDL_Init(0) != 0)
        {
            URHO3D_LOGERRORF("Failed to initialise SDL: %s", SDL_GetError());
            return false;
        }
    }

    Uint32 remainingFlags = sdlFlags & ~SDL_WasInit(0);
    if (remainingFlags != 0)
    {
        if (SDL_InitSubSystem(remainingFlags) != 0)
        {
            URHO3D_LOGERRORF("Failed to initialise SDL subsystem: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

void Context::ReleaseSDL()
{
    --sdlInitCounter;

    if (sdlInitCounter == 0)
    {
        URHO3D_LOGDEBUG("Quitting SDL");
        SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
        SDL_Quit();
    }

    if (sdlInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseSDL()!");
}

#ifdef URHO3D_IK
void Context::RequireIK()
{
    // Always increment, the caller must match with ReleaseIK(), regardless of
    // what happens.
    ++ikInitCounter;

    if (ikInitCounter == 1)
    {
        URHO3D_LOGDEBUG("Initialising Inverse Kinematics library");
        ik_memory_init();
        ik_log_init(IK_LOG_NONE);
        ik_log_register_listener(HandleIKLog);
    }
}

void Context::ReleaseIK()
{
    --ikInitCounter;

    if (ikInitCounter == 0)
    {
        URHO3D_LOGDEBUG("De-initialising Inverse Kinematics library");
        ik_log_unregister_listener(HandleIKLog);
        ik_log_deinit();
        ik_memory_deinit();
    }

    if (ikInitCounter < 0)
        URHO3D_LOGERROR("Too many calls to Context::ReleaseIK()");
}
#endif // ifdef URHO3D_IK
#endif // ifndef MINI_URHO

void Context::CopyBaseAttributes(StringHash baseType, StringHash derivedType)
{
    // Prevent endless loop if mistakenly copying attributes from same class as derived
    if (baseType == derivedType)
    {
        URHO3D_LOGWARNING("Attempt to copy base attributes to itself for class " + GetTypeName(baseType));
        return;
    }

    const ea::vector<AttributeInfo>* baseAttributes = GetAttributes(baseType);
    if (baseAttributes)
    {
        for (unsigned i = 0; i < baseAttributes->size(); ++i)
        {
            const AttributeInfo& attr = baseAttributes->at(i);
            attributes_[derivedType].push_back(attr);
            if (attr.mode_ & AttributeMode::Net)
                networkAttributes_[derivedType].push_back(attr);
        }
    }
}

Object* Context::GetSubsystem(StringHash type) const
{
    auto i = subsystems_.find(type);
    if (i != subsystems_.end())
        return i->second;
    else
        return nullptr;
}

const Variant& Context::GetGlobalVar(StringHash key) const
{
    auto i = globalVars_.find(key);
    return i != globalVars_.end() ? i->second : Variant::EMPTY;
}

void Context::SetGlobalVar(StringHash key, const Variant& value)
{
    globalVars_[key] = value;
}

Object* Context::GetEventSender() const
{
    if (!eventSenders_.empty())
        return eventSenders_.back();
    else
        return nullptr;
}

const ea::string& Context::GetTypeName(StringHash objectType) const
{
    // Search factories to find the hash-to-name mapping
    auto i = factories_.find(objectType);
    return i != factories_.end() ? i->second->GetTypeName() : EMPTY_STRING;
}

AttributeInfo* Context::GetAttribute(StringHash objectType, const char* name)
{
    auto i = attributes_.find(objectType);
    if (i == attributes_.end())
        return nullptr;

    ea::vector<AttributeInfo>& infos = i->second;

    for (auto j = infos.begin(); j != infos.end(); ++j)
    {
        if (!j->name_.comparei(name))
            return &(*j);
    }

    return nullptr;
}

void Context::AddEventReceiver(Object* receiver, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = eventReceivers_[eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::AddEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    SharedPtr<EventReceiverGroup>& group = specificEventReceivers_[sender][eventType];
    if (!group)
        group = new EventReceiverGroup();
    group->Add(receiver);
}

void Context::RemoveEventSender(Object* sender)
{
    auto i = specificEventReceivers_.find(
        sender);
    if (i != specificEventReceivers_.end())
    {
        for (auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            for (auto k = j->second->receivers_.begin(); k !=
                j->second->receivers_.end(); ++k)
            {
                Object* receiver = *k;
                if (receiver)
                    receiver->RemoveEventSender(sender);
            }
        }
        specificEventReceivers_.erase(i);
    }
}

void Context::RemoveEventReceiver(Object* receiver, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(eventType);
    if (group)
        group->Remove(receiver);
}

void Context::RemoveEventReceiver(Object* receiver, Object* sender, StringHash eventType)
{
    EventReceiverGroup* group = GetEventReceivers(sender, eventType);
    if (group)
        group->Remove(receiver);
}

void Context::BeginSendEvent(Object* sender, StringHash eventType)
{
    eventSenders_.push_back(sender);
}

void Context::EndSendEvent()
{
    eventSenders_.pop_back();
}

void Context::RegisterSubsystem(Engine* subsystem)
{
    engine_ = subsystem;
    RegisterSubsystem((Object*)subsystem);
}

void Context::RegisterSubsystem(Time* subsystem)
{
    time_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(WorkQueue* subsystem)
{
    workQueue_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
void Context::RegisterSubsystem(FileSystem* subsystem)
{
    fileSystem_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_LOGGING
void Context::RegisterSubsystem(Log* subsystem)
{
    log_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(ResourceCache* subsystem)
{
    cache_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(Localization* subsystem)
{
    l18n_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_NETWORK
void Context::RegisterSubsystem(Network* subsystem)
{
    network_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(Input* subsystem)
{
    input_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(Audio* subsystem)
{
    audio_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(UI* subsystem)
{
    ui_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#if URHO3D_SYSTEMUI
void Context::RegisterSubsystem(SystemUI* subsystem)
{
    systemUi_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}
#endif
void Context::RegisterSubsystem(Graphics* subsystem)
{
    graphics_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

void Context::RegisterSubsystem(Renderer* subsystem)
{
    renderer_ = subsystem;
    RegisterSubsystem((Object*) subsystem);
}

}
