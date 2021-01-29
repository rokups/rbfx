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

#include "../Core/Object.h"

namespace Urho3D
{

class PluginApplication;

/// Enumeration describing plugin file path status.
enum class ModuleType
{
    /// Not a valid plugin.
    Invalid,
    /// A native plugin.
    Native,
    /// A managed plugin.
    Managed,
};

#ifdef URHO3D_LEGACY_ENUMS
static const ModuleType MODULE_INVALID = ModuleType::Invalid;
static const ModuleType MODULE_NATIVE = ModuleType::Native;
static const ModuleType MODULE_MANAGED = ModuleType::Managed;
#endif

/// A class managing lifetime of dynamic library module.
class URHO3D_API PluginModule : public Object
{
    URHO3D_OBJECT(PluginModule, Object);
public:
    /// Construct.
    explicit PluginModule(Context* context) : Object(context) { }
    /// Destruct.
    ~PluginModule() override;
    /// Load a specified dynamic library and return true on success.
    bool Load(const ea::string& path);
    /// Unload currently loaded dynamic library. Returns true only if library was previously loaded and unloading succeeded.
    bool Unload();
    /// Looks up exported symbol in current loaded dynamic library and returns it. Works only for native modules.
    void* GetSymbol(const ea::string& symbol);
    /// Return a type of current loaded module.
    ModuleType GetModuleType() const { return moduleType_; }
    ///
    const ea::string& GetPath() const { return path_; }
    /// Inspects a specified file and detects it's type.
    static ModuleType ReadModuleInformation(Context* context, const ea::string& path, unsigned* pdbPathOffset=nullptr,
        unsigned* pdbPathLength=nullptr);
    ///
    PluginApplication* InstantiatePlugin();

protected:
    /// A path of current loaded module.
    ea::string path_;
    /// A platform-specific handle to current loaded module.
    uintptr_t handle_ = 0;
    /// A type of current loaded module.
    ModuleType moduleType_ = ModuleType::Invalid;
};

}
