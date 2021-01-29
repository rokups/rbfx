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

#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Script/Script.h>

#include "Plugins/ModulePlugin.h"


namespace Urho3D
{

bool ModulePlugin::Load()
{
    ea::string path = NameToPath(name_);
    ea::string pluginPath = VersionModule(path);

    if (pluginPath.empty())
        return false;

    lastModuleType_ = ModuleType::Invalid;
    if (module_.Load(pluginPath))
    {
        application_ = module_.InstantiatePlugin();
        if (application_.NotNull())
        {
            application_->InitializeReloadablePlugin();
            path_ = path;
            mtime_ = context_->GetSubsystem<FileSystem>()->GetLastModifiedTime(pluginPath);
            version_++;
            unloading_ = false;
            lastModuleType_ = module_.GetModuleType();
            return true;
        }
    }
    return false;
}

bool ModulePlugin::PerformUnload()
{
    if (application_.Null())
        return false;

#if URHO3D_CSHARP
    ModuleType moduleType = module_.GetModuleType();
#endif
    // Disposing object requires managed reference to be the last one alive.
    WeakPtr<PluginApplication> application(application_);
    application_->UninitializeReloadablePlugin();
#if URHO3D_CSHARP
    if (moduleType == ModuleType::Managed)
         Script::GetRuntimeApi()->Dispose(application_.Detach());
#endif
    application_ = nullptr;
    if (!module_.Unload())
        return false;
    return true;
}

ea::string ModulePlugin::NameToPath(const ea::string& name) const
{
    FileSystem* fs = context_->GetSubsystem<FileSystem>();
    ea::string result;

#if __linux__ || __APPLE__
    result = Format("{}lib{}{}", fs->GetProgramDir(), name, DYN_LIB_SUFFIX);
    if (fs->FileExists(result))
        return result;
#endif

#if !_WIN32
    result = Format("{}{}{}", fs->GetProgramDir(), name, ".dll");
    if (fs->FileExists(result))
        return result;
#endif

    result = Format("{}{}{}", fs->GetProgramDir(), name, DYN_LIB_SUFFIX);
    if (fs->FileExists(result))
        return result;

    return EMPTY_STRING;
}

ea::string ModulePlugin::VersionModule(const ea::string& path)
{
    auto* fs = context_->GetSubsystem<FileSystem>();
    ea::string dir, name, ext;
    SplitPath(path, dir, name, ext);

    ea::string versionString, shortenedName;

    // Headless utilities do not reuqire reloading. They will load module directly.
    if (context_->GetSubsystem<Engine>()->IsHeadless())
    {
        versionString = "";
        shortenedName = name;
    }
    else
    {
        versionString = ea::to_string(version_ + 1);
        shortenedName = name.substr(0, name.length() - versionString.length());
    }

    if (shortenedName.length() < 3)
    {
        URHO3D_LOGERROR("Plugin file name '{}' is too short.", name);
        return EMPTY_STRING;
    }

    ea::string versionedPath = dir + shortenedName + versionString + ext;

    // Non-versioned modules do not need pdb/version patching.
    if (context_->GetSubsystem<Engine>()->IsHeadless())
        return versionedPath;

    if (!fs->Copy(path, versionedPath))
    {
        URHO3D_LOGERROR("Copying '{}' to '{}' failed.", path, versionedPath);
        return EMPTY_STRING;
    }

#if _MSC_VER || URHO3D_CSHARP
    unsigned pdbOffset = 0, pdbSize = 0;
    ModuleType type = PluginModule::ReadModuleInformation(context_, path, &pdbOffset, &pdbSize);
#if _MSC_VER
    bool hashPdb = true;
#else
    bool hashPdb = type == ModuleType::Managed;
#endif
    if (hashPdb)
    {
        if (pdbOffset != 0)
        {
            File dll(context_);
            if (dll.Open(versionedPath, FILE_READWRITE))
            {
                VectorBuffer fileData(dll, dll.GetSize());
                char* pdbPointer = (char*)fileData.GetModifiableData() + pdbOffset;

                ea::string pdbPath;
                pdbPath.resize(pdbSize);
                strncpy(&pdbPath.front(), pdbPointer, pdbSize);
                SplitPath(pdbPath, dir, name, ext);

                ea::string versionedPdbPath = dir + shortenedName + versionString + ".pdb";
                assert(versionedPdbPath.length() == pdbPath.length());

                // We have to copy pdbs for native and managed dlls
                if (!fs->Copy(pdbPath, versionedPdbPath))
                {
                    URHO3D_LOGERROR("Copying '{}' to '{}' failed.", pdbPath, versionedPdbPath);
                    return EMPTY_STRING;
                }

                strcpy((char*)fileData.GetModifiableData() + pdbOffset, versionedPdbPath.c_str());
                dll.Seek(0);
                dll.Write(fileData.GetData(), fileData.GetSize());
            }
            else
                return EMPTY_STRING;
        }
    }
#if URHO3D_CSHARP
    if (type == ModuleType::Managed)
    {
        // Managed runtime will modify file version in specified file.
        Script::GetRuntimeApi()->SetAssemblyVersion(versionedPath, version_ + 1);
    }
#endif
#endif
    return versionedPath;
}

bool ModulePlugin::IsOutOfDate() const
{
    return mtime_ < context_->GetSubsystem<FileSystem>()->GetLastModifiedTime(path_);
}

bool ModulePlugin::WaitForCompleteFile(unsigned timeoutMs) const
{
    Timer wait;
    // Plugin change is detected the moment compiler starts linking file. We should wait until linker is done.
    while (PluginModule::ReadModuleInformation(context_, path_) != lastModuleType_)
    {
        Time::Sleep(0);
        if (wait.GetMSec(false) >= timeoutMs)
        {
            URHO3D_LOGERROR("Plugin module '{}' linking timeout. Plugin will be unloaded.", name_);
            return false;
        }
    }
    return true;
}

}
