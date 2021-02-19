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

#include <EASTL/sort.h>

#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/ResourceEvents.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/IO/ArchiveSerialization.h>
#include <Urho3D/IO/Log.h>
#ifdef URHO3D_GLOW
#include <Urho3D/Glow/LightmapUVGenerator.h>
#endif
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/ModelView.h>

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <Toolbox/IO/ContentUtilities.h>
#include <SDL/SDL_clipboard.h>

#include "EditorEvents.h"
#include "Inspector/ModelPreview.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/InspectorTab.h"
#include "Pipeline/Pipeline.h"
#include "Editor.h"
#include "ResourceTab.h"

namespace Urho3D
{

static ea::unordered_map<ContentType, ea::string> contentToTabType{
    {CTYPE_SCENE, "SceneTab"},
    {CTYPE_UILAYOUT, "UITab"},
};

ResourceTab::ResourceTab(Context* context)
    : Tab(context)
{
    SetID("29d1a5dc-6b8d-4a27-bfb2-a84417f33ee2");
    SetTitle("Resources");
    isUtility_ = true;

    SubscribeToEvent(E_INSPECTORLOCATERESOURCE, &ResourceTab::OnLocateResource);
}

void ResourceTab::OnLocateResource(StringHash, VariantMap& args)
{
    auto resourceName = args[InspectorLocateResource::P_NAME].GetString();

    Project* project = GetSubsystem<Project>();
    FileSystem* fs = GetSubsystem<FileSystem>();

    currentDir_ = GetPath(resourceName);
    // if (fs->FileExists(project->GetCachePath() + resourceName))
    // {
    //     // File is in the cache. currentDir_ should point to a directory of source resource. For example:
    //     // We have Resources/Models/cube.fbx which is a source model.
    //     // It is converted to Cache/Models/cube.fbx/Model.mdl
    //     // ResourceBrowserWidget() expects:
    //     // * resourcePath = Models/ (path same as if cube.fbx was selected)
    //     // * resourceSelection = cube.fbx/Model.mdl (selection also includes a directory which resides in cache)
    //     while (!fs->DirExists(project->GetResourcePath() + currentDir_))
    //         currentDir_ = GetParentPath(currentDir_);
    //     selectedItem_ = resourceName.substr(currentDir_.length());
    // }
    // else
        selectedItem_ = GetFileNameAndExtension(resourceName);
    scrollToCurrent_ = true;
    if (ui::GetIO().KeyCtrl)
        SelectCurrentItemInspector();
}

bool ResourceTab::RenderWindowContent()
{
    const ImGuiStyle& style = ui::GetStyle();
    Project* project = GetSubsystem<Project>();
    Pipeline* pipeline = GetSubsystem<Pipeline>();
    ScanAssets();

    // Render folder tree on the left
    ui::Columns(2);
    if (ui::BeginChild("##DirectoryTree", ui::GetContentRegionAvail()))
    {
        if (ui::TreeNodeEx("Root", ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanFullWidth|(currentDir_.empty() ? ImGuiTreeNodeFlags_Selected : 0)))
        {
            if (ui::IsItemClicked(MOUSEB_LEFT))
            {
                currentDir_.clear();
                selectedItem_.clear();
                SelectCurrentItemInspector();
            }
            ui::TreePop();
        }
        RenderDirectoryTree();
    }
    ui::EndChild();
    ui::SameLine();
    ui::NextColumn();
    if (!ui::BeginChild("##DirectoryContent", ui::GetContentRegionAvail()))
    {
        ui::EndChild();
        return true;
    }

    ea::string destDirName = "";

    for (auto name : RemoveTrailingSlash(currentDir_).split("/"))
    {
        destDirName += name + "/";

        if (destDirName == currentDir_)
        {
            ui::TextUnformatted(name.c_str());
            continue;
        }

        if (ui::Selectable(name.c_str()))
            currentDir_ = destDirName;

        ui::SameLine();
        ui::TextUnformatted(" > ");
        ui::SameLine();
    }

    auto previewSize = ImVec2(200.0f, 200.0f);

    ui::BeginColumns("files", Max(1, Floor(ui::GetWindowWidth() / previewSize.x)),
                     ImGuiColumnsFlags_NoBorder | ImGuiColumnsFlags_NoResize);

    auto cache = context_->GetSubsystem<ResourceCache>();

    // Render folders
    for (const ea::string& name : currentDirs_)
    {
        if (name == "." || name == "..")
        {
            continue;
        }

        if (scrollToCurrent_ && selectedItem_ == name)
        {
            ui::SetScrollHereY();
            scrollToCurrent_ = false;
        }

        if (isRenamingFrame_ && selectedItem_ == name)
        {
            ui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});    // Make sure text starts rendering at the same position
            RenderRenameWidget();
            ui::PopStyleVar();
            continue;
        }
        
        ui::PushID(name.c_str());

        auto texture = cache->GetResource<Texture2D>("Textures/Editor/FileIcons/folder.png");

        if (texture)
        {
            ui::ImageItem(texture, previewSize);
        }
        
        int clicks = ui::DoubleClickSelectable(RemoveTrailingSlash(name).c_str(), name == selectedItem_);

        if (ui::IsItemHovered() && ui::IsMouseClicked(MOUSEB_RIGHT))
        {
            selectedItem_ = name;
            ui::OpenPopup("Resource Item Context Menu");
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (clicks == 1)
        {
            selectedItem_ = name;
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (clicks == 2)
        {
            currentDir_ += AddTrailingSlash(name);
            selectedItem_.clear();
            rescan_ = true;
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (ui::IsItemActive())
        {
            if (ui::BeginDragDropSource())
            {
                ea::string resourceName = currentDir_ + name;
                ui::SetDragDropVariant("path", resourceName);
                ui::TextUnformatted(resourceName.c_str());
                ui::EndDragDropSource();
            }
        }
        else if (ui::BeginDragDropTarget())
        {
            const Variant& dropped = ui::AcceptDragDropVariant("path");
            if (dropped.GetType() == VAR_STRING)
            {
                ResourceCache* cache = GetSubsystem<ResourceCache>();
                FileSystem* fs = GetSubsystem<FileSystem>();
                ea::string source = dropped.GetString();
                bool isDir = source.ends_with("/");
                ea::string destination;
                if (name == "..")
                    destination = GetParentPath(currentDir_);
                else
                    destination = currentDir_ + name;
                destination = AddTrailingSlash(destination);
                destination += GetFileNameAndExtension(RemoveTrailingSlash(source));
                destination = project->GetResourcePath() + destination;

                if (isDir)
                    source = project->GetResourcePath() + source;
                else
                    source = cache->GetResourceFileName(source);

                if (!fs->Exists(source))
                    URHO3D_LOGWARNING("File operations can be performed only on files in the main resource path.");
                else if (source != destination)
                {
                    cache->RenameResource(source, destination);
                    rescan_ = true;
                }
            }
            ui::EndDragDropTarget();
        }

        ui::PopID();

        ui::NextColumn();
    }
    
    // Render files
    for (Asset* asset : assets_)
    {
        // Asset may get deleted.
        if (asset == nullptr)
            continue;

        ea::string name = GetFileNameAndExtension(asset->GetName());
        ea::string icon = GetFileIcon(asset->GetName());

        if (scrollToCurrent_ && selectedItem_ == name)
        {
            ui::SetScrollHereY();
            scrollToCurrent_ = false;
        }

        ui::PushID(asset->GetName().c_str());
        ui::BeginChild("preview", previewSize);

        if (asset->GetContentType() == CTYPE_MODEL)
        {
            auto model = cache->GetResource<Model>(asset->GetName());

            if (model)
            {
                auto preview = ui::GetUIState<ModelPreview>(context_);
                preview->SetModel(model);
                preview->RenderPreview();
            }
        }
        else if (asset->GetContentType() == CTYPE_MATERIAL)
        {
            auto material = cache->GetResource<Material>(asset->GetName());

            if (material)
            {
                auto preview = ui::GetUIState<ModelPreview>(context_);
                if (preview->GetMaterial(0) == nullptr)
                    preview->SetMaterial(material, 0);
                preview->RenderPreview();
                ui::SetHelpTooltip("Click to switch object.");
                if (ui::IsItemClicked(MOUSEB_LEFT))
                    preview->ToggleModel();
            }
        }
        else if (asset->GetContentType() == CTYPE_TEXTURE)
        {
            auto texture = cache->GetResource<Texture2D>(asset->GetName());

            if (texture)
            {
                ui::ImageItem(texture, previewSize);
            }
        }
        else
        {
            auto texture = cache->GetResource<Texture2D>("Textures/Editor/FileIcons/generic.png");

            if (texture)
            {
                ui::ImageItem(texture, previewSize);
            }
        }

        ui::EndChild();

        int clicks = 0;

        if (isRenamingFrame_ && selectedItem_ == name)
        {
            ui::PushStyleVar(ImGuiStyleVar_FramePadding,
                             {0, 0}); // Make sure text starts rendering at the same position
            RenderRenameWidget();
            ui::PopStyleVar();
        }
        else
        {
            clicks = ui::DoubleClickSelectable(name.c_str(), name == selectedItem_);
        }

        ui::PopID();

        if (ui::IsItemHovered() && ui::IsMouseClicked(MOUSEB_RIGHT))
        {
            selectedItem_ = name;
            ui::OpenPopup("Resource Item Context Menu");
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (clicks == 1)
        {
            selectedItem_ = name;
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (clicks == 2)
        {
            OpenResource(currentDir_ + selectedItem_);
            isRenamingFrame_ = 0;
            SelectCurrentItemInspector();
        }
        else if (ui::IsItemActive())
        {
            if (ui::BeginDragDropSource())
            {
                ea::string resourceName = currentDir_ + name;
                ResourceContentTypes contentTypes;
                GetContentResourceType(context_, resourceName, contentTypes);
                ea::string dragType = "path";
                for (StringHash type : contentTypes)
                {
                    dragType += ",";
                    dragType += type.ToString();
                }
                ui::SetDragDropVariant(dragType, resourceName);
                ui::TextUnformatted(resourceName.c_str());
                ui::EndDragDropSource();
            }
        }

        // Byproducts are now viewed by opening the folder with the same name.

        ui::NextColumn();
    }

    // Context menu when clicking empty area
    if (!ui::IsAnyItemHovered() && ui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ui::IsMouseClicked(MOUSEB_RIGHT))
    {
        ui::OpenPopup("Resource Item Context Menu");
        selectedItem_.clear();
    }

    RenderContextMenu();
    ui::EndChild();
    ui::Columns(1);

    if (ui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ui::IsKeyPressed(KEY_F2))
        StartRename();

    return true;
}

ea::string ResourceTab::GetNewResourcePath(const ea::string& name)
{
    auto project = GetSubsystem<Project>();
    auto fs = GetSubsystem<FileSystem>();
    ea::string resourcePath = project->GetResourcePath() + name;
    if (!fs->FileExists(resourcePath))
        return resourcePath;

    ea::string basePath = GetPath(name);
    ea::string baseName = GetFileName(name);
    ea::string ext = GetExtension(name, false);

    for (auto i = 1; i < M_MAX_INT; i++)
    {
        resourcePath = Format("{}{} {}{}", project->GetResourcePath(), basePath, baseName, i, ext);
        if (!context_->GetSubsystem<FileSystem>()->FileExists(resourcePath))
            return resourcePath;
    }

    std::abort();
}

void ResourceTab::SelectCurrentItemInspector()
{
    if (selectedItem_ == "." || selectedItem_ == "..")
        return;

    ea::string selected = currentDir_ + selectedItem_;

    ResourceContentTypes contentTypes;
    auto* inspector = GetSubsystem<InspectorTab>();
    auto* pipeline = GetSubsystem<Pipeline>();
    auto* cache = GetSubsystem<ResourceCache>();
    auto* undo = GetSubsystem<UndoStack>();
    inspector->Clear();

    if (Asset* asset = pipeline->GetAsset(selected))
        asset->Inspect();
    else if (GetContentResourceType(context_, selected, contentTypes))
    {
        // Inspect byproduct directly.
        if (Resource* resource = cache->GetResource(contentTypes.front(), selected))
        {
            inspector->Inspect(resource);
            undo->Connect(resource);    // ??
        }
    }

    using namespace EditorResourceSelected;
    SendEvent(E_EDITORRESOURCESELECTED, P_CTYPE, GetContentType(context_, selected), P_RESOURCENAME, selected);

    using namespace EditorSelectionChanged;
    SendEvent(E_EDITORSELECTIONCHANGED, P_TAB, this);
}

void ResourceTab::ClearSelection()
{
    // Do not clear resource path to keep current folder user is navigated into.
    selectedItem_.clear();
    using namespace EditorSelectionChanged;
    SendEvent(E_EDITORSELECTIONCHANGED, P_TAB, this);
}

bool ResourceTab::SerializeSelection(Archive& archive)
{
    if (auto block = archive.OpenSequentialBlock("paths"))
    {
        if (!SerializeValue(archive, "path", currentDir_))
            return false;
        if (!SerializeValue(archive, "item", selectedItem_))
            return false;
        return true;
    }
    return false;
}

void ResourceTab::OpenResource(const ea::string& resourceName)
{
    FileSystem* fs = GetSubsystem<FileSystem>();
    Project* project = GetSubsystem<Project>();
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Editor* editor = GetSubsystem<Editor>();

    ea::string resourcePath = cache->GetResourceFileName(resourceName);
    auto it = contentToTabType.find(GetContentType(context_, resourceName));
    if (it != contentToTabType.end())
    {
        if (auto* tab = editor->GetTab(it->second))
        {
            if (tab->IsUtility())
            {
                // Tabs that can be opened once.
                tab->LoadResource(resourceName);
                tab->Activate();
            }
            else
            {
                // Tabs that can be opened multiple times.
                if ((tab = editor->GetTabByResource(resourceName)))
                    tab->Activate();
                else if ((tab = editor->CreateTab(it->second)))
                {
                    tab->LoadResource(resourceName);
                    tab->AutoPlace();
                    tab->Activate();
                }
            }
        }
        else if ((tab = editor->CreateTab(it->second)))
        {
            // Tabs that can be opened multiple times.
            tab->LoadResource(resourceName);
            tab->AutoPlace();
            tab->Activate();
        }
    }
    else
    {
        // Unknown resources are opened with associated application.
        if (fs->Exists(resourcePath))
            fs->SystemOpen(resourcePath);
    }
}

void ResourceTab::ScanAssets()
{
    rescan_ |= rescanTimer_.GetMSec(false) >= 1000;
    if (!rescan_)
        return;
    rescan_ = false;
    rescanTimer_.Reset();

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Project* project = GetSubsystem<Project>();
    Pipeline* pipeline = GetSubsystem<Pipeline>();

    // Gather directories from project resource paths and CoreData.
    currentDirs_.clear();
    cache->Scan(currentDirs_, currentDir_, "", SCAN_DIRS, false);
    ea::sort(currentDirs_.begin(), currentDirs_.end());
    currentDirs_.erase(ea::unique(currentDirs_.begin(), currentDirs_.end()), currentDirs_.end());
    if (currentDir_.empty())
        currentDirs_.erase_first("..");
    else
    {
        currentDirs_.erase_first(".");
        if (currentDirs_.front() != "..")
            currentDirs_.insert_at(0, "..");
    }

    // Gather files from project resource paths and CoreData.
    currentFiles_.clear();
    cache->Scan(currentFiles_, currentDir_, "", SCAN_FILES, false);
    ea::sort(currentFiles_.begin(), currentFiles_.end());
    currentFiles_.erase(ea::unique(currentFiles_.begin(), currentFiles_.end()), currentFiles_.end());

    // Gather assets
    assets_.clear();
    for (const ea::string& fileName : currentFiles_)
    {
        ea::string resourceName = currentDir_ + fileName;
        // We may hit file names in resource cache, they would yield no asset.
        if (Asset* asset = pipeline->GetAsset(resourceName))
            assets_.emplace_back(asset);
    }
}

void ResourceTab::RenderContextMenu()
{
    bool requiresItemSelected = false;
    bool deletionPending = false;
    if (ui::BeginPopup("Resource Item Context Menu"))
        requiresItemSelected = true;
    else if (ui::BeginPopup("Resource Dir Context Menu"))
        requiresItemSelected = false;
    else
    {
        RenderDeletionDialog();
        return;
    }

    const ImGuiStyle& style = ui::GetStyle();
    bool hasSelection = !selectedItem_.empty() && selectedItem_ != "." && selectedItem_ != "..";
    if (ui::BeginMenu("Create"))
    {
        if (ui::MenuItem(ICON_FA_FOLDER " Folder"))
        {
            selectedItem_ = "New Folder";
            ea::string path = GetNewResourcePath(currentDir_ + selectedItem_);
            if (context_->GetSubsystem<FileSystem>()->CreateDir(path))
            {
                scrollToCurrent_ = true;
                StartRename();
            }
            else
                URHO3D_LOGERRORF("Failed creating folder '%s'.", path.c_str());
        }

        if (ui::MenuItem("Scene"))
        {
            auto path = GetNewResourcePath(currentDir_ + "New Scene.xml");
            context_->GetSubsystem<FileSystem>()->CreateDirsRecursive(GetPath(path));

            SharedPtr<Scene> scene(new Scene(context_));
            scene->CreateComponent<Octree>();
            File file(context_, path, FILE_WRITE);
            if (file.IsOpen())
            {
                scene->SaveXML(file);
                scrollToCurrent_ = true;
                selectedItem_ = GetFileNameAndExtension(path);
                StartRename();
            }
            else
                URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
        }

        if (ui::MenuItem("Material"))
        {
            ea::string path = GetNewResourcePath(currentDir_ + "New Material.xml");
            context_->GetSubsystem<FileSystem>()->CreateDirsRecursive(GetPath(path));

            Material material(context_);
            File file(context_, path, FILE_WRITE);
            if (file.IsOpen() && material.Save(file))
            {
                scrollToCurrent_ = true;
                selectedItem_ = GetFileNameAndExtension(path);
                StartRename();
            }
            else
                URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
        }

        if (ui::MenuItem("Texture (PNG)"))
        {
            ea::string path = GetNewResourcePath(currentDir_ + "New Texture.png");
            Image image(context_);
            image.SetSize(1, 1, 4);
            File file(context_, path, FILE_WRITE);
            if (file.IsOpen() && image.Save(file))
            {
                scrollToCurrent_ = true;
                selectedItem_ = GetFileNameAndExtension(path);
                StartRename();
            }
            else
                URHO3D_LOGERRORF("Failed opening file '%s'.", path.c_str());
        }

        ui::EndMenu();
    }

    ea::string resourceName = currentDir_ + (hasSelection ? selectedItem_ : "");
    if (ui::MenuItem("Copy Path"))
        SDL_SetClipboardText(resourceName.c_str());

    bool disableCurrentItemOps = requiresItemSelected && selectedItem_.empty();
    if (disableCurrentItemOps)
    {
        ui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    }

    if (ui::MenuItem("Rename", "F2"))
        StartRename();

    deletionPending = ui::MenuItem("Delete", "Del");

    if (disableCurrentItemOps)
    {
        ui::PopStyleColor();
        ui::PopItemFlag();
    }

    // TODO(glow): Move it into separate addon
#ifdef URHO3D_GLOW
    const bool modelSelected = selectedItem_.ends_with(".mdl");
    if (modelSelected)
    {
        ui::Separator();

        if (ui::MenuItem("Generate Lightmap UV", nullptr, nullptr))
        {
            auto model = context_->GetSubsystem<ResourceCache>()->GetResource<Model>(currentDir_ + selectedItem_);
            if (model && !model->GetNativeFileName().empty())
            {
                ModelView modelView(context_);
                if (modelView.ImportModel(model))
                {
                    if (GenerateLightmapUV(modelView, {}))
                    {
                        model->SendEvent(E_RELOADSTARTED);
                        modelView.ExportModel(model);
                        model->SendEvent(E_RELOADFINISHED);

                        model->SaveFile(model->GetNativeFileName());
                    }
                }
            }
        }
    }
#endif

    ResourceContextMenuArgs args;
    args.resourceName_ = resourceName;
    resourceContextMenu_(this, args);

    ui::EndPopup();

    if (deletionPending)
        ui::OpenPopup("Delete?");
    RenderDeletionDialog();
}

void ResourceTab::RenderDirectoryTree(const eastl::string& path)
{
    struct CachedDirs
    {
        /// Cached directories.
        StringVector dirs_;
        /// Timer since this cache was last expired.
        Timer expires_;
    };

    auto getCachedDirs = [&](unsigned id, const eastl::string& scanPath)
    {
        CachedDirs* value = cache_.Peek<CachedDirs>(id);
        bool isNewlyCreated = value == nullptr;
        if (isNewlyCreated)
            value = cache_.Get<CachedDirs>(id);

        // Refresh caches every now and then.
        if (isNewlyCreated || value->expires_.GetMSec(false) > 1000)
        {
            ScanDirTree(value->dirs_, scanPath);
            value->expires_.Reset();
        }
        return value;
    };
    ImGuiContext& g = *GImGui;
    ui::PushID(path.c_str());
    CachedDirs* value = getCachedDirs(ui::GetCurrentWindow()->IDStack.back(), path);
    bool openContextMenu = false;
    bool expireCache = false;
    for (const ea::string& dir : value->dirs_)
    {
        ea::string childDir = path + AddTrailingSlash(dir);
        bool isSelected = currentDir_ == childDir;
        bool isRenamingCurrent = isSelected && isRenamingFrame_ && selectedItem_.empty();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!isRenamingCurrent)
            flags |= ImGuiTreeNodeFlags_SpanFullWidth;
        if (currentDir_ == childDir)
            flags |= ImGuiTreeNodeFlags_Selected;

        // Also unconditionally scan children so we can determine whether directory is a leaf node or not.
        CachedDirs* childValue = getCachedDirs(ui::GetID(childDir.c_str()), childDir);
        if (childValue->dirs_.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool isOpen = ui::TreeNodeEx(isRenamingCurrent ? "" : dir.c_str(), flags);
        if (isRenamingCurrent)
        {
            const ImGuiStyle& style = ui::GetStyle();
            ui::SameLine();
            ui::PushStyleVar(ImGuiStyleVar_FramePadding, {g.StyleTemplate.FramePadding.x, 0});    // Make sure text starts rendering at the same position
            expireCache |= !RenderRenameWidget();
            ui::PopStyleVar();
        }
        else if (ui::IsItemClicked(MOUSEB_LEFT) || ui::IsItemClicked(MOUSEB_RIGHT))
        {
            currentDir_ = childDir;
            selectedItem_.clear();
            isRenamingFrame_ = 0;
            openContextMenu |= ui::IsItemClicked(MOUSEB_RIGHT);
            // Not selecting this item in inspector on purpose, so users can navigate directory tree with a selected scene entity.
        }

        if (!isOpen && currentDir_.starts_with(childDir) && currentDir_.length() > childDir.length())
        {
            // Open tree nodes when navigating file system in files panel.
            isOpen = true;
            ui::OpenTreeNode(ui::GetID(dir.c_str()));
        }

        if (isOpen)
        {
            RenderDirectoryTree(childDir);
            ui::TreePop();
        }
    }

    if (expireCache)
        cache_.Remove<CachedDirs>(ui::GetCurrentWindow()->IDStack.back());

    if (openContextMenu)
        ui::OpenPopup("Resource Dir Context Menu");

    RenderContextMenu();
    ui::PopID();
}

void ResourceTab::ScanDirTree(StringVector& result, const eastl::string& path)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    result.clear();
    cache->Scan(result, path, "", SCAN_DIRS, false);
    ea::sort(result.begin(), result.end());
    result.erase(ea::unique(result.begin(), result.end()), result.end());
    result.erase_first(".");
    result.erase_first("..");
}

void ResourceTab::RenderDeletionDialog()
{
    if (ui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ui::Text("Would you like to delete '%s%s'?", currentDir_.c_str(), selectedItem_ == ".." ? "" : selectedItem_.c_str());
        ui::TextUnformatted(ICON_FA_EXCLAMATION_TRIANGLE " This action can not be undone!");
        ui::NewLine();

        if (ui::Button("Delete Permanently"))
        {
            Project* project = GetSubsystem<Project>();
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            FileSystem* fs = GetSubsystem<FileSystem>();
            const ea::string& resourceName = currentDir_ + (selectedItem_ == ".." ? "" : selectedItem_);
            const ea::string& fileName = cache->GetResourceFileName(resourceName);
            if (fs->FileExists(fileName))
            {
                cache->IgnoreResourceReload(resourceName);
                cache->ReleaseResource(resourceName, true);
                fs->Delete(fileName);

                using namespace ResourceBrowserDelete;
                SendEvent(E_RESOURCEBROWSERDELETE, P_NAME, resourceName);
            }
            else
            {
                for (const ea::string& resourceDir : project->GetResourcePaths())
                {
                    ea::string resourcePath = project->GetProjectPath() + resourceDir + resourceName;
                    if (fs->DirExists(resourcePath))
                    {
                        StringVector resources;
                        cache->Scan(resources, resourceName, "", SCAN_FILES, true);
                        for (const ea::string& name : resources)
                        {
                            ea::string fullResourceName = AddTrailingSlash(fullResourceName) + name;
                            cache->IgnoreResourceReload(fullResourceName);
                            cache->ReleaseResource(fullResourceName, true);

                            using namespace ResourceBrowserDelete;
                            SendEvent(E_RESOURCEBROWSERDELETE, P_NAME, fullResourceName);
                        }
                        fs->RemoveDir(resourcePath, true);
                    }
                }
            }
            ui::CloseCurrentPopup();
        }

        if (ui::IsKeyPressed(KEY_ESCAPE))
            ui::CloseCurrentPopup();

        ui::EndPopup();
    }
}

void ResourceTab::StartRename()
{
    if (!selectedItem_.empty())
    {
        // Renaming in file/folder panel (right).
        if (selectedItem_ != "." && selectedItem_ != "..")
            renameBuffer_ = selectedItem_;
        else
            return;
    }
    else if (!currentDir_.empty())
    {
        // Renaming in dir tree panel (left).
        assert(currentDir_.ends_with("/"));
        renameBuffer_ = GetFileNameAndExtension(currentDir_.substr(0, currentDir_.length() - 1));
    }
    else
        return;

    isRenamingFrame_ = ui::GetFrameCount() + 1;
}

bool ResourceTab::RenderRenameWidget(const ea::string& icon)
{
    const ImGuiStyle& style = ui::GetStyle();

    if (!icon.empty())
    {
        ui::TextUnformatted(icon.c_str());
        ui::SameLine();
    }

    ui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    if (isRenamingFrame_ == ui::GetFrameCount())
        ui::SetKeyboardFocusHere();
    ui::PushStyleColor(ImGuiCol_FrameBg, 0);
    if (ui::InputText("##rename", &renameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        ea::string currentPath, newPath, resourceDir;

        currentPath = currentDir_ + selectedItem_;
        if (!selectedItem_.empty())
            newPath = currentDir_ + renameBuffer_;
        else
            newPath = GetParentPath(currentDir_) + AddTrailingSlash(renameBuffer_);

        if (currentPath != newPath)
        {
            FileSystem* fs = GetSubsystem<FileSystem>();
            Project* project = GetSubsystem<Project>();
            ResourceCache* cache = GetSubsystem<ResourceCache>();

            bool renamedAny = false;
            for (const ea::string& dir : project->GetResourcePaths())
            {
                ea::string absoluteCurrentPath = project->GetProjectPath() + dir + currentPath;
                ea::string absoluteNewPath = project->GetProjectPath() + dir + newPath;
                if (fs->Exists(absoluteNewPath))
                    URHO3D_LOGERROR("Failed renaming '{}' to '{}' because destination exists.", absoluteCurrentPath, absoluteNewPath);
                else if (fs->Exists(absoluteCurrentPath))
                {
                    if (!cache->RenameResource(absoluteCurrentPath, absoluteNewPath))
                        URHO3D_LOGERROR("Failed renaming '{}' to '{}'.", absoluteCurrentPath, absoluteNewPath);
                    else
                        renamedAny = true;
                }
            }

            if (renamedAny)
            {
                if (!selectedItem_.empty())
                    selectedItem_ = renameBuffer_;
                else
                    currentDir_ = newPath;
            }
        }
        renameBuffer_.clear();
        isRenamingFrame_ = 0;
    }
    ui::PopStyleColor();
    ui::PopStyleVar();
    if (ui::IsKeyPressed(KEY_ESCAPE) || !ui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        isRenamingFrame_ = 0;

    if (isRenamingFrame_ == 0)
        rescan_ = true;

    return isRenamingFrame_ > 0;
}

}
