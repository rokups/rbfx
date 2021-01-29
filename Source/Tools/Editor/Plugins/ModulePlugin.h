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


#include "Plugins/Plugin.h"

namespace Urho3D
{

/// Plugin that is loaded from a native or managed dynamic library.
class ModulePlugin : public Plugin
{
    URHO3D_OBJECT(ModulePlugin, Plugin);
public:
    /// Construct.
    explicit ModulePlugin(Context* context) : Plugin(context) { }
    /// Loads plugin into application memory space and initializes it.
    bool Load() override;
    /// Returns true if plugin is loaded and functional.
    bool IsLoaded() const override { return module_.GetModuleType() != ModuleType::Invalid && !unloading_ && application_.NotNull(); }
    /// Returns true if plugin was modified on the disk and should be reloaded.
    bool IsOutOfDate() const override;
    /// This function will block until plugin file is complete and ready to be loaded. Returns false if timeout exceeded, but file is still incomplete.
    bool WaitForCompleteFile(unsigned timeoutMs) const override;

protected:
    ///
    const ea::string& GetPath() const { return path_; }
    /// Converts name to a full plugin file path. Returns empty string on error.
    ea::string NameToPath(const ea::string& name) const;
    ///
    ea::string VersionModule(const ea::string& path);
    ///
    bool PerformUnload() override;

    /// Unversioned plugin module path.
    ea::string path_;
    /// Native module of this plugin.
    PluginModule module_{context_};
    /// Last modification time.
    unsigned mtime_ = 0;
    /// Last loaded module type.
    ModuleType lastModuleType_ = ModuleType::Invalid;
};


}
