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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Engine/EngineEvents.h>
#include <Urho3D/Engine/PluginApplication.h>
#include <Urho3D/IO/ArchiveSerialization.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Script/Script.h>
#include <Toolbox/SystemUI/Widgets.h>
#include "EditorEvents.h"
#include "Editor.h"
#include "Plugins/PluginManager.h"
#include "Plugins/ModulePlugin.h"
#include "Plugins/ScriptBundlePlugin.h"

#if URHO3D_PLUGINS

namespace Urho3D
{

URHO3D_EVENT(E_ENDFRAMEPRIVATE, EndFramePrivate)
{
}

PluginManager::PluginManager(Context* context)
    : Object(context)
{
    SubscribeToEvent(E_ENDFRAMEPRIVATE, [this](StringHash, VariantMap&) { OnEndFrame(); });
    SubscribeToEvent(E_SIMULATIONSTART, [this](StringHash, VariantMap&) {
        for (Plugin* plugin : plugins_)
        {
            plugin->application_->SendEvent(E_PLUGINSTART);
            plugin->application_->Start();
        }
    });
    SubscribeToEvent(E_SIMULATIONSTOP, [this](StringHash, VariantMap&) {
        for (Plugin* plugin : plugins_)
        {
            plugin->application_->SendEvent(E_PLUGINSTOP);
            plugin->application_->Stop();
        }
    });
}

PluginManager::~PluginManager()
{
    for (Plugin* plugin : plugins_)
        plugin->Unload();
    // Could have called PerformUnload() above, but this way we get a consistent log message informing that module was unloaded.
    OnEndFrame();
}

Plugin* PluginManager::Load(StringHash type, const ea::string& name)
{
    Plugin* plugin = GetPlugin(name);

    if (plugin == nullptr)
    {
        plugin = static_cast<Plugin*>(context_->CreateObject(type).Detach());
        assert(plugin != nullptr && plugin->GetTypeInfo()->GetBaseTypeInfo()->GetType() == Plugin::GetTypeStatic());
        plugin->SetName(name);
    }
    else if (plugin->IsLoaded())
        return plugin;

    if (plugin->Load())
    {
        plugin->application_->SendEvent(E_PLUGINLOAD);
        plugin->application_->Load();
        URHO3D_LOGINFO("Loaded plugin '{}' version {}.", name, plugin->version_);
        plugins_.push_back(SharedPtr(plugin));
        return plugin;
    }
    else
        URHO3D_LOGERROR("Failed loading plugin '{}'.", name);
    return nullptr;
}

void PluginManager::OnEndFrame()
{
    // TODO: Timeout probably should be configured, larger projects will have a pretty long linking time.
    const unsigned pluginLinkingTimeout = 10000;
    bool eventSent = false;

    bool checkOutOfDatePlugins = updateCheckTimer_.GetMSec(false) >= 1000;
    if (checkOutOfDatePlugins)
        updateCheckTimer_.Reset();

    for (Plugin* plugin : plugins_)
    {
        if (plugin->application_.Null())
            continue;

        bool pluginOutOfDate = false;

        // Plugin reloading is not used in headless executions.
        if (!context_->GetSubsystem<Engine>()->IsHeadless())
        {
            // Check for modified plugins once in a while, do not hammer syscalls on every frame.
            if (checkOutOfDatePlugins)
                pluginOutOfDate = checkOutOfDatePlugins && plugin->IsOutOfDate();
        }

        if (plugin->unloading_ || pluginOutOfDate)
        {
            if (!eventSent)
            {
                SendEvent(E_EDITORUSERCODERELOADSTART);
                eventSent = true;
            }

            plugin->application_->SendEvent(E_PLUGINUNLOAD);
            plugin->application_->Unload();

            int allowedPluginRefs = 1;
            if (plugin->application_->Refs() != 1)
            {
                URHO3D_LOGERROR("Plugin application '{}' has more than one reference remaining. "
                                 "This will lead to memory leaks or crashes.",
                                 plugin->application_->GetTypeName());
            }
            plugin->PerformUnload();
        }

        if (pluginOutOfDate)
        {
            if (!plugin->WaitForCompleteFile(pluginLinkingTimeout))
                plugin->Unload();       // unloading_ = true
        }

        if (!plugin->unloading_ && pluginOutOfDate)
        {
            if (plugin->Load())
                URHO3D_LOGINFO("Reloaded plugin '{}' version {}.", GetFileNameAndExtension(plugin->name_), plugin->version_);
            else
                URHO3D_LOGERROR("Reloading plugin '{}' failed and it was unloaded.", GetFileNameAndExtension(plugin->name_));
        }
        else if (plugin->unloading_)
            URHO3D_LOGINFO("Unloaded plugin '{}'.", GetFileNameAndExtension(plugin->name_));

        if (!plugin->unloading_)
        {
            if (plugin->application_.NotNull())
            {
                if (pluginOutOfDate)
                {
                    plugin->application_->SendEvent(E_PLUGINLOAD);
                    plugin->application_->Load();
                }
            }
            else
                plugin->unloading_ = true;
        }
    }

    if (eventSent)
        SendEvent(E_EDITORUSERCODERELOADEND);
}

Plugin* PluginManager::GetPlugin(const ea::string& name)
{
    for (Plugin* plugin : plugins_)
    {
        if (plugin->name_ == name)
            return plugin;
    }
    return nullptr;
}

ea::string PluginManager::PathToName(const ea::string& path)
{
#if !_WIN32
    if (path.ends_with(DYN_LIB_SUFFIX))
    {
        ea::string name = GetFileName(path);
#if __linux__ || __APPLE__
        if (name.starts_with("lib"))
            name = name.substr(3);
#endif
        return name;
    }
#endif
    if (path.ends_with(".dll"))
        return GetFileName(path);

    return EMPTY_STRING;
}

const StringVector& PluginManager::GetPluginNames()
{
    auto* pluginNames = ui::GetUIState<StringVector>();

    if (pluginNames->empty())
    {
        FileSystem* fs = context_->GetSubsystem<FileSystem>();

        StringVector files;
        ea::unordered_map<ea::string, ea::string> nameToPath;
        fs->ScanDir(files, fs->GetProgramDir(), "*.*", SCAN_FILES, false);

        // Remove deleted plugin files.
        for (const ea::string& key : pluginInfoCache_.keys())
        {
            if (!files.contains(key))
                pluginInfoCache_.erase(key);
        }

        // Remove definitely not plugins.
        for (const ea::string& file : files)
        {
            ea::string baseName = PluginManager::PathToName(file);
            // Native plugins will rename main file and append version after base name.
            if (baseName.empty() || IsDigit(static_cast<unsigned int>(baseName.back())))
                continue;

            ea::string fullPath = fs->GetProgramDir() + file;
            DynamicLibraryInfo& info = pluginInfoCache_[file];
            unsigned currentModificationTime = fs->GetLastModifiedTime(fullPath);
            if (info.mtime_ != currentModificationTime)
            {
                // Parse file only if it is outdated or was not parsed already.
                info.mtime_ = currentModificationTime;
                info.pluginType_ = PluginModule::ReadModuleInformation(context_, fullPath);
            }

            if (info.pluginType_ == ModuleType::Invalid)
                continue;

            pluginNames->push_back(baseName);
        }
    }
    return *pluginNames;
}

bool PluginManager::Serialize(Archive& archive)
{
#if URHO3D_STATIC
    // In static builds plugins are registered manually by the user.
    SendEvent(E_REGISTERSTATICPLUGINS);
#else
    if (auto block = archive.OpenSequentialBlock("plugins"))
    {
        ea::string name;
        bool isPrivate;
        for (unsigned i = 0, num = archive.IsInput() ? block.GetSizeHint() : plugins_.size(); i < num; i++)
        {
            if (!archive.IsInput())
            {
                // ScriptBundlePlugin is special. Only one instance of this plugin exists and it is loaded manually. No point in serializing it.
                Plugin* plugin = plugins_[i];

                if (!plugin->IsManagedManually())
                    continue;

                if (!plugin->IsLoaded())
                    continue;

                name = plugin->GetName();
                isPrivate = plugin->IsPrivate();
            }

            if (auto block = archive.OpenUnorderedBlock("plugin"))
            {
                if (!SerializeValue(archive, "name", name))
                    return false;
                if (!SerializeValue(archive, "private", isPrivate))
                    return false;
                if (archive.IsInput())
                {
                    if (Plugin* plugin = Load(ModulePlugin::GetTypeStatic(), name))
                        plugin->SetPrivate(isPrivate);
                }
            }
        }
    }
#endif

    return true;
}

#if URHO3D_STATIC
bool PluginManager::RegisterPlugin(PluginApplication* application)
{
    auto* plugin = new Plugin(context_);
    plugin->SetName(application->GetTypeName());
    plugin->application_ = application;
    plugin->application_->SendEvent(E_PLUGINLOAD);
    plugin->application_->Load();
    plugins_.push_back(SharedPtr(plugin));
    return true;
}
#endif

void RegisterPluginsLibrary(Context* context)
{
    context->RegisterFactory<PluginManager>();
    context->RegisterFactory<ModulePlugin>();
#if URHO3D_CSHARP
    context->RegisterFactory<ScriptBundlePlugin>();
#endif
}

}

#endif // URHO3D_PLUGINS
