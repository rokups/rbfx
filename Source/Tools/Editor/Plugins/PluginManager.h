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

#pragma once


#if URHO3D_PLUGINS

#include <atomic>
#include <Urho3D/IO/Archive.h>
#include <Urho3D/IO/FileWatcher.h>
#include <Urho3D/IO/Log.h>

#include "Plugins/Plugin.h"

namespace Urho3D
{

class PluginManager : public Object
{
    URHO3D_OBJECT(PluginManager, Object);
public:
    /// Construct.
    explicit PluginManager(Context* context);
    /// Unload all plugins an destruct.
    ~PluginManager() override;
    /// Load a plugin and return true if succeeded.
    Plugin* Load(StringHash type, const ea::string& name);
    /// Returns a loaded plugin with specified name.
    Plugin* GetPlugin(const ea::string& name);
    /// Returns a vector containing all loaded plugins.
    const ea::vector<SharedPtr<Plugin>>& GetPlugins() const { return plugins_; }
    /// Tick native plugins.
    void OnEndFrame();
    /// Returns list of sorted plugin names that exist in editor directory.
    const StringVector& GetPluginNames();
    /// Converts relative or absolute plugin path to universal plugin name. Returns empty string on failure.
    static ea::string PathToName(const ea::string& path);
#if URHO3D_STATIC
    /// Registers static plugin.
    bool RegisterPlugin(PluginApplication* application);
#endif
    bool Serialize(Archive& archive) override;

protected:
    /// Entry about dynamic library on the disk. It may or may not be loaded.
    struct DynamicLibraryInfo
    {
        /// Last modification time.
        unsigned mtime_ = 0;
        /// Type of plugin.
        ModuleType pluginType_ = ModuleType::Invalid;
    };

    /// Loaded plugins.
    ea::vector<SharedPtr<Plugin>> plugins_;
    /// Plugin update check timer.
    Timer updateCheckTimer_;
    /// Cached plugin information.
    ea::unordered_map<ea::string, DynamicLibraryInfo> pluginInfoCache_;

    friend class Plugin;
};

/// Reigsters all plugin-related objects with the engine.
void RegisterPluginsLibrary(Context* context);

}
#endif
