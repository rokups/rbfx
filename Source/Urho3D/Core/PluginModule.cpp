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

#include "../Core/PluginModule.h"
#include "../IO/Log.h"
#include "../IO/File.h"

#if URHO3D_CSHARP
#   include "../Script/Script.h"
#endif
#if _WIN32
#   include <windows.h>
#else
#   include "PE.h"
#   include <dlfcn.h>
#endif
#if __linux__
#   include <elf.h>
#   if URHO3D_64BIT
using Elf_Ehdr = Elf64_Ehdr;
using Elf_Phdr = Elf64_Phdr;
using Elf_Shdr = Elf64_Shdr;
using Elf_Sym = Elf64_Sym;
#   else
using Elf_Ehdr = Elf32_Ehdr;
using Elf_Phdr = Elf32_Phdr;
using Elf_Shdr = Elf32_Shdr;
using Elf_Sym = Elf32_Sym;
#   endif
#endif
#if __APPLE__
struct mach_header
{
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
#if URHO3D_64BIT
    uint32_t reserved;
#endif
};


#if URHO3D_64BIT
const unsigned MACHO_MAGIC = 0xFEEDFACF;
#else
const unsigned MACHO_MAGIC = 0xFEEDFACE;
#endif

struct nlist
{
    int32_t n_strx;
    uint8_t n_type;
    uint8_t n_sect;
    int16_t n_desc;
#if URHO3D_64BIT
    uint64_t n_value;
#else
    uint32_t n_value;
#endif
};

struct load_command
{
    uint32_t cmd;
    uint32_t cmdsize;
};
struct symtab_command : load_command
{
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
};

struct dysymtab_command : load_command
{
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
};
#endif

namespace Urho3D
{

template<typename T>
static bool IsValidPtr(const ea::vector<unsigned char>& data, const T* p)
{
    return reinterpret_cast<std::uintptr_t>(p) >= reinterpret_cast<std::uintptr_t>(data.data()) &&
           reinterpret_cast<std::uintptr_t>(p) + sizeof(T) <= reinterpret_cast<std::uintptr_t>(data.data() + data.size());
}

template<typename T>
static bool IsValidPtr(const ea::vector<unsigned char>& data, const T* p, unsigned len)
{
    return reinterpret_cast<std::uintptr_t>(p) >= reinterpret_cast<std::uintptr_t>(data.data()) &&
           reinterpret_cast<std::uintptr_t>(p) + len <= reinterpret_cast<std::uintptr_t>(data.data() + data.size());
}

PluginModule::~PluginModule()
{
    if (handle_ != 0)
        Unload();
}

bool PluginModule::Load(const ea::string& path)
{
#if URHO3D_PLUGINS
    if (handle_ != 0)
    {
        URHO3D_LOGERROR("Plugin {} is already loaded.", path_);
        return false;
    }

    moduleType_ = ReadModuleInformation(context_, path);
    if (moduleType_ == ModuleType::Invalid)
        return false;

#if URHO3D_CSHARP
    if (moduleType_ == ModuleType::Managed)
        handle_ = Script::GetRuntimeApi()->LoadAssembly(path.data());
    else
#endif  // URHO3D_CSHARP
    {
#if _WIN32
        handle_ = (uintptr_t)LoadLibrary(path.data());
#elif __linux__ || __APPLE__
        handle_ = (uintptr_t)dlopen(path.c_str(), RTLD_NOW);
#else
        URHO3D_LOGERROR("Plugin loading is not supported on this platform.");
#endif
    }
    if (handle_ != 0)
    {
        path_ = path;
        return true;
    }
    else
    {
        path_.clear();
        moduleType_ = ModuleType::Invalid;
#if __linux__ || __APPLE__
        URHO3D_LOGERROR("dlerror: {}", dlerror());
#endif
    }
#endif  // URHO3D_PLUGINS
    return false;
}

bool PluginModule::Unload()
{
#if URHO3D_PLUGINS
    if (handle_ == 0)
        return false;

#if URHO3D_CSHARP
    if (moduleType_ == ModuleType::Managed)
    {
        Script::GetRuntimeApi()->FreeGCHandle((void*)handle_);
    }
    else
#endif  // URHO3D_CSHARP
    {
#if _WIN32
        FreeLibrary((HMODULE)handle_);
#elif __linux__ || __APPLE__
        dlclose((void*)handle_);
#else
        URHO3D_LOGERROR("Plugin loading is not supported on this platform.");
        return false;
#endif  // __linux__ || __APPLE__
    }

    handle_ = 0;
    path_.clear();
    moduleType_ = ModuleType::Invalid;
    return true;
#else
    return false;
#endif  // URHO3D_PLUGINS
}

void* PluginModule::GetSymbol(const ea::string& symbol)
{
#if URHO3D_PLUGINS
#if URHO3D_CSHARP
    if (moduleType_ != ModuleType::Native)
    {
        URHO3D_LOGERROR("Symbol retrieval is not supported for this type of module.");
        return nullptr;
    }
#endif  // URHO3D_CSHARP

#if _WIN32
    return (void*)GetProcAddress((HMODULE)handle_, symbol.c_str());
#elif __linux__ || __APPLE__
    return dlsym((void*)handle_, symbol.c_str());
#else
    URHO3D_LOGERROR("Plugin symbol retrieval is not supported on this platform.");
    return nullptr;
#endif  // __linux__ || __APPLE__
#else
    return nullptr;
#endif  // URHO3D_PLUGINS
}

ModuleType PluginModule::ReadModuleInformation(Context* context, const ea::string& path, unsigned* pdbPathOffset, unsigned* pdbPathLength)
{
#if URHO3D_PLUGINS
    // This function implements a naive check for plugin validity. Proper check would parse executable headers and look
    // for relevant exported function names.
    ea::vector<unsigned char> data;
    const char pluginEntryPoint[] = "PluginApplicationMain";

#if __linux__
    // ELF magic
    if (path.ends_with(".so"))
    {
        File file(context);
        if (!file.Open(path, FILE_READ))
            return ModuleType::Invalid;

        data.resize(file.GetSize());
        if (file.Read(data.data(), data.size()) != data.size())
            return ModuleType::Invalid;

        // Elf header parsing code based on elfdump by Owen Klan.
        auto* hdr = reinterpret_cast<Elf_Ehdr*>(data.data());
        if (!IsValidPtr(data, hdr) || strncmp(reinterpret_cast<const char*>(hdr->e_ident), ELFMAG, SELFMAG) != 0)
            // Not elf.
            return ModuleType::Invalid;

        if (hdr->e_type != ET_DYN)
            return ModuleType::Invalid;

        // Find symbol name table
        unsigned symNameTableOffset = 0;
        {
            auto* shdr = reinterpret_cast<Elf_Shdr*>(data.data() + hdr->e_shoff + sizeof(Elf_Shdr) * hdr->e_shstrndx);
            if (!IsValidPtr(data, shdr))
                return ModuleType::Invalid;

            auto nameTableOffset = shdr->sh_offset;
            shdr = reinterpret_cast<Elf_Shdr*>(data.data() + hdr->e_shoff);

            for (auto i = 0; i < hdr->e_shnum; i++)
            {
                if (!IsValidPtr(data, shdr))
                    return ModuleType::Invalid;

                const char* tabNamePtr = reinterpret_cast<const char*>(data.data() + nameTableOffset + shdr->sh_name);
                const char strTabName[] = ".strtab";

                if (!IsValidPtr(data, tabNamePtr, sizeof(strTabName)))
                    return ModuleType::Invalid;

                if (strncmp(tabNamePtr, strTabName, sizeof(strTabName)) == 0)
                {
                    symNameTableOffset = shdr->sh_offset;
                    break;
                }
                else
                    ++shdr;
            }
        }

        if (symNameTableOffset == 0)
            return ModuleType::Invalid;

        // Find PluginApplicationMain symbol
        {
            auto shoff = hdr->e_shoff;
            Elf_Shdr* sectab = nullptr;
            do
            {
                sectab = reinterpret_cast<Elf_Shdr*>(data.data() + shoff);
                if (!IsValidPtr(data, sectab))
                    return ModuleType::Invalid;
                shoff += sizeof(Elf_Shdr);
            } while (sectab->sh_type != SHT_SYMTAB);

            shoff = sectab->sh_offset;
            auto num = sectab->sh_size / sectab->sh_entsize;
            ea::string funcName;
            for (auto i = 0; i < num; i++)
            {
                auto* symbol = reinterpret_cast<Elf_Sym*>(data.data() + shoff);
                if (!IsValidPtr(data, symbol))
                    return ModuleType::Invalid;
                shoff += sizeof(Elf_Sym);

                const char* funcNamePtr = reinterpret_cast<const char*>(data.data() + symNameTableOffset + symbol->st_name);
                if (!IsValidPtr(data, funcNamePtr, sizeof(pluginEntryPoint)))
                    return ModuleType::Invalid;

                if (strncmp(funcNamePtr, pluginEntryPoint, sizeof(pluginEntryPoint)) == 0)
                    return ModuleType::Native;
            }
        }
    }
#endif  // __linux__

#if _WIN32 || URHO3D_CSHARP
    if (path.ends_with(".dll"))
    {
        File file(context);
        if (!file.Open(path.data(), FILE_READ))
            return ModuleType::Invalid;

        data.resize(file.GetSize());
        if (file.Read(data.data(), data.size()) != data.size())
            return ModuleType::Invalid;

        auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(data.data());

        if (!IsValidPtr(data, dos) || dos->e_magic != IMAGE_DOS_SIGNATURE)
            return ModuleType::Invalid;

        auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(data.data() + dos->e_lfanew);
        if (!IsValidPtr(data, dos) || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
            return ModuleType::Invalid;

        const auto& eatDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        const auto& netDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR];
        const auto& dbgDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

        // Find pdb
        if (pdbPathOffset != nullptr)
        {
            *pdbPathOffset = 0;
            if (dbgDir.VirtualAddress && dbgDir.Size)
            {
                uint32_t offset = 0;
                // Convert dbgDir.VirtualAddress virtual address to offset in physical file.
                for (auto *sect = IMAGE_FIRST_SECTION(nt), *end = sect + nt->FileHeader.NumberOfSections; sect < end && offset == 0; sect++)
                {
                    if (dbgDir.VirtualAddress >= sect->VirtualAddress && dbgDir.VirtualAddress < sect->VirtualAddress + sect->Misc.VirtualSize)
                        offset = dbgDir.VirtualAddress - (sect->VirtualAddress - sect->PointerToRawData);
                }

                if (offset != 0)
                {
                    auto* debugEntries = reinterpret_cast<PIMAGE_DEBUG_DIRECTORY>(data.data() + offset);
                    if (IsValidPtr(data, debugEntries, dbgDir.Size))
                    {
                        for (unsigned i = 0, numEntries = dbgDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY); i < numEntries; i++)
                        {
                            if (debugEntries[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW)
                            {
                                struct CodeViewData
                                {
                                    /* 000 */ uint32_t magic;
                                    /* 004 */ uint8_t guid[16];
                                    /* 014 */ uint32_t age;
                                    /* 018 */ char pdbPath[];
                                };

                                auto* codeViewData = reinterpret_cast<CodeViewData*>(data.data() + debugEntries[i].PointerToRawData);
                                if (IsValidPtr(data, codeViewData, debugEntries[i].SizeOfData))
                                {
                                    if (codeViewData->magic == 0x53445352)
                                    {
                                        *pdbPathOffset = (uintptr_t)&codeViewData->pdbPath - (uintptr_t)data.data();
                                        if (pdbPathLength != nullptr)
                                            *pdbPathLength = strnlen(codeViewData->pdbPath, debugEntries[i].PointerToRawData - offsetof(CodeViewData, pdbPath));
                                    }
                                    else
                                    {
                                        URHO3D_LOGERROR("Unsupported CodeView version.");
                                        return ModuleType::Invalid;
                                    }
                                }
                                else
                                    return ModuleType::Invalid;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Detect if dll is native or managed
        if (netDir.VirtualAddress != 0)
        {
#if URHO3D_CSHARP
            // Verify that plugin has a class that inherits from PluginApplication.
            if (Script::GetRuntimeApi()->VerifyAssembly(path.data()))
                return ModuleType::Managed;
#endif
        }
#if _WIN32
        else if (eatDir.VirtualAddress > 0)
        {
            // Verify that plugin has exported function named PluginApplicationMain.
            // Find section that contains EAT.

            unsigned firstSectionOffset = dos->e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + nt->FileHeader.SizeOfOptionalHeader;
            uint32_t eatModifier = 0;
            for (auto i = 0; i < nt->FileHeader.NumberOfSections; i++)
            {
                auto* section = reinterpret_cast<PIMAGE_SECTION_HEADER>(data.data() + firstSectionOffset + i * sizeof(IMAGE_SECTION_HEADER));
                if (!IsValidPtr(data, section))
                    return ModuleType::Invalid;

                if (eatDir.VirtualAddress >= section->VirtualAddress && eatDir.VirtualAddress < (section->VirtualAddress + section->SizeOfRawData))
                {
                    eatModifier = section->VirtualAddress - section->PointerToRawData;
                    break;
                }
            }

            auto* eat = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(data.data() + eatDir.VirtualAddress - eatModifier);
            if (!IsValidPtr(data, eat))
                return ModuleType::Invalid;

            unsigned namesOffset = eat->AddressOfNames - eatModifier;
            for (auto i = 0; i < eat->NumberOfFunctions; i++)
            {
                uint32_t* nameOffset = reinterpret_cast<unsigned*>(data.data() + namesOffset + i * sizeof(uint32_t));
                if (!IsValidPtr(data, nameOffset))
                    return ModuleType::Invalid;

                const char* funcNamePtr = reinterpret_cast<const char*>(data.data() + *nameOffset - eatModifier);
                if (!IsValidPtr(data, funcNamePtr, sizeof(pluginEntryPoint)))
                    return ModuleType::Invalid;

                if (strncmp(funcNamePtr, pluginEntryPoint, sizeof(pluginEntryPoint)) == 0)
                    return ModuleType::Native;
            }
        }
#endif  // _WIN32
    }
#endif  // _WIN32 || URHO3D_CSHARP
#if __APPLE__
    if (path.ends_with(".dylib"))
    {
        File file(context);
        if (!file.Open(path.data(), FILE_READ))
            return ModuleType::Invalid;

        data.resize(file.GetSize());
        if (file.Read(data.data(), data.size()) != data.size())
            return ModuleType::Invalid;

        auto* hdr = reinterpret_cast<mach_header*>(data.data());

        if (!IsValidPtr(data, hdr) || hdr->magic != MACHO_MAGIC || hdr->filetype != 6 /*dylib*/)
            return ModuleType::Invalid;

        symtab_command* symtab = nullptr;
        dysymtab_command* dysymtab = nullptr;
        unsigned offset = sizeof(mach_header);
        for (unsigned i = 0; i < hdr->ncmds; i++)
        {
            auto* cmd = reinterpret_cast<load_command*>(data.data() + offset);
            if (!IsValidPtr(data, cmd))
                return ModuleType::Invalid;

            if (cmd->cmd == 0x2)            // SYMTAB
            {
                symtab = reinterpret_cast<symtab_command*>(cmd);
                if (!IsValidPtr(data, symtab))
                    return ModuleType::Invalid;
            }
            else if (cmd->cmd == 0xB)   // DYSYMTAB
            {
                dysymtab = reinterpret_cast<dysymtab_command*>(cmd);
                if (!IsValidPtr(data, dysymtab))
                    return ModuleType::Invalid;
            }

            if (symtab && dysymtab)
                break;

            offset += cmd->cmdsize;
        }

        if (!symtab || !dysymtab)
            return ModuleType::Invalid;

        const char* strtab = reinterpret_cast<const char*>(data.data() + symtab->stroff);
        const nlist* lists = reinterpret_cast<nlist*>(data.data() + symtab->symoff);

        if (!IsValidPtr(data, lists, sizeof(nlist) * symtab->nsyms))
            return ModuleType::Invalid;

        if (!IsValidPtr(data, strtab, symtab->strsize))
            return ModuleType::Invalid;

        for (unsigned i = dysymtab->ilocalsym; i < dysymtab->ilocalsym + dysymtab->nlocalsym; i++)
        {
            const char* name = strtab + lists[i].n_strx;
            if (strncmp(name, pluginEntryPoint, sizeof(pluginEntryPoint)) == 0)
                return ModuleType::Native;
        }
    }
#endif  // __APPLE__
#endif  // URHO3D_PLUGINS
    return ModuleType::Invalid;
}

PluginApplication* PluginModule::InstantiatePlugin()
{
    if (moduleType_ == ModuleType::Native)
    {
        if (void* main = GetSymbol("PluginApplicationMain"))
            return reinterpret_cast<PluginApplication*(*)(Context*)>(main)(context_);
    }
#if URHO3D_CSHARP
    else if (moduleType_ == ModuleType::Managed)
        return Script::GetRuntimeApi()->CreatePluginApplication(handle_);
#endif
    return nullptr;
}

}
