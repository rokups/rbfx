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
#ifdef WIN32
#include <windows.h>
#endif

#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Engine/EngineEvents.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/WorkQueue.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Resource/JSONArchive.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>
#include <Urho3D/SystemUI/Console.h>
#include <Urho3D/SystemUI/DebugHud.h>
#include <Urho3D/LibraryInfo.h>
#include <Urho3D/Core/CommandLine.h>
#include <Urho3D/Audio/Sound.h>

#include <Toolbox/ToolboxAPI.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <nativefiledialog/nfd.h>

#include "Editor.h"
#include "EditorEvents.h"
#include "EditorIconCache.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/Scene/EditorSceneSettings.h"
#include "Tabs/InspectorTab.h"
#include "Tabs/HierarchyTab.h"
#include "Tabs/ConsoleTab.h"
#include "Tabs/ResourceTab.h"
#include "Tabs/PreviewTab.h"
#include "Pipeline/Asset.h"
#include "Pipeline/Commands/CookScene.h"
#include "Pipeline/Commands/BuildAssets.h"
#include "Pipeline/Importers/ModelImporter.h"
#include "Pipeline/Importers/SceneConverter.h"
#include "Pipeline/Importers/TextureImporter.h"
#if URHO3D_PLUGINS
#   include "Plugins/PluginManager.h"
#   include "Plugins/ModulePlugin.h"
#endif
#include "Plugins/ScriptBundlePlugin.h"
#include "Inspector/AssetInspector.h"
#include "Inspector/MaterialInspector.h"
#include "Inspector/ModelInspector.h"
#include "Inspector/NodeInspector.h"
#include "Inspector/ComponentInspector.h"
#include "Inspector/SerializableInspector.h"
#include "Inspector/SoundInspector.h"
#include "Tabs/ProfilerTab.h"
#include "EditorUndo.h"

namespace Urho3D
{

namespace
{

const auto&& DEFAULT_TAB_TYPES = {
    InspectorTab::GetTypeStatic(),
    HierarchyTab::GetTypeStatic(),
    ResourceTab::GetTypeStatic(),
    ConsoleTab::GetTypeStatic(),
    PreviewTab::GetTypeStatic(),
    SceneTab::GetTypeStatic(),
    ProfilerTab::GetTypeStatic()
};

}

Editor::Editor(Context* context)
    : Application(context)
{
}

void Editor::Setup()
{
    context_->RegisterSubsystem(this, Editor::GetTypeStatic());

#ifdef _WIN32
    // Required until SDL supports hdpi on windows
    if (HMODULE hLibrary = LoadLibraryA("Shcore.dll"))
    {
        typedef HRESULT(WINAPI*SetProcessDpiAwarenessType)(size_t value);
        if (auto fn = GetProcAddress(hLibrary, "SetProcessDpiAwareness"))
            ((SetProcessDpiAwarenessType)fn)(2);    // PROCESS_PER_MONITOR_DPI_AWARE
        FreeLibrary(hLibrary);
    }
#endif

    // Discover resource prefix path by looking for CoreData and going up.
    for (coreResourcePrefixPath_ = context_->GetSubsystem<FileSystem>()->GetProgramDir();;
        coreResourcePrefixPath_ = GetParentPath(coreResourcePrefixPath_))
    {
        if (context_->GetSubsystem<FileSystem>()->DirExists(coreResourcePrefixPath_ + "CoreData"))
            break;
        else
        {
#if WIN32
            if (coreResourcePrefixPath_.length() <= 3)   // Root path of any drive
#else
            if (coreResourcePrefixPath_ == "/")          // Filesystem root
#endif
            {
                URHO3D_LOGERROR("Prefix path not found, unable to continue. Prefix path must contain all of your data "
                                "directories (including CoreData).");
                engine_->Exit();
            }
        }
    }

    engineParameters_[EP_WINDOW_TITLE] = GetTypeName();
    engineParameters_[EP_HEADLESS] = false;
    engineParameters_[EP_FULL_SCREEN] = false;
    engineParameters_[EP_LOG_LEVEL] = LOG_DEBUG;
    engineParameters_[EP_WINDOW_RESIZABLE] = true;
    engineParameters_[EP_AUTOLOAD_PATHS] = "";
    engineParameters_[EP_RESOURCE_PATHS] = "CoreData;EditorData";
    engineParameters_[EP_RESOURCE_PREFIX_PATHS] = coreResourcePrefixPath_;
    engineParameters_[EP_WINDOW_MAXIMIZE] = true;
    engineParameters_[EP_ENGINE_AUTO_LOAD_SCRIPTS] = false;
    engineParameters_[EP_SYSTEMUI_FLAGS] = ImGuiConfigFlags_DpiEnableScaleFonts;
#if URHO3D_SYSTEMUI_VIEWPORTS
    engineParameters_[EP_HIGH_DPI] = true;
    engineParameters_[EP_SYSTEMUI_FLAGS] = engineParameters_[EP_SYSTEMUI_FLAGS].GetUInt() | ImGuiConfigFlags_ViewportsEnable /*| ImGuiConfigFlags_DpiEnableScaleViewports*/;
#else
    engineParameters_[EP_HIGH_DPI] = false;
#endif
    // Load editor settings
    {
        auto* fs = context_->GetSubsystem<FileSystem>();
        ea::string editorSettingsDir = fs->GetAppPreferencesDir("rbfx", "Editor");
        if (!fs->DirExists(editorSettingsDir))
            fs->CreateDir(editorSettingsDir);

        ea::string editorSettingsFile = editorSettingsDir + "Editor.json";
        if (fs->FileExists(editorSettingsFile))
        {
            JSONFile file(context_);
            if (file.LoadFile(editorSettingsFile))
            {
                JSONInputArchive archive(&file);
                if (!Serialize(archive))
                    URHO3D_LOGERROR("Loading of editor settings failed.");

                engineParameters_[EP_WINDOW_WIDTH] = windowSize_.x_;
                engineParameters_[EP_WINDOW_HEIGHT] = windowSize_.y_;
                engineParameters_[EP_WINDOW_POSITION_X] = windowPos_.x_;
                engineParameters_[EP_WINDOW_POSITION_Y] = windowPos_.y_;
            }
        }
    }

    context_->GetSubsystem<Log>()->SetLogFormat("[%H:%M:%S] [%l] [%n] : %v");

    SetRandomSeed(Time::GetTimeSinceEpoch());

    // Register factories
    context_->RegisterFactory<EditorIconCache>();
    context_->RegisterFactory<SceneTab>();
    context_->RegisterFactory<ConsoleTab>();
    context_->RegisterFactory<HierarchyTab>();
    context_->RegisterFactory<InspectorTab>();
    context_->RegisterFactory<ResourceTab>();
    context_->RegisterFactory<PreviewTab>();
    context_->RegisterFactory<ProfilerTab>();

    // Inspectors.
    inspectors_.push_back(SharedPtr(new AssetInspector(context_)));
    inspectors_.push_back(SharedPtr(new ModelInspector(context_)));
    inspectors_.push_back(SharedPtr(new MaterialInspector(context_)));
    inspectors_.push_back(SharedPtr(new SoundInspector(context_)));
    inspectors_.push_back(SharedPtr(new NodeInspector(context_)));
    inspectors_.push_back(SharedPtr(new ComponentInspector(context_)));
    // FIXME: If user registers their own inspector later then SerializableInspector would no longer come in last.
    inspectors_.push_back(SharedPtr(new SerializableInspector(context_)));

#if URHO3D_PLUGINS
    RegisterPluginsLibrary(context_);
#endif
    RegisterToolboxTypes(context_);
    EditorSceneSettings::RegisterObject(context_);
    context_->RegisterFactory<SerializableInspector>();

    // Importers
    ModelImporter::RegisterObject(context_);
    SceneConverter::RegisterObject(context_);
    TextureImporter::RegisterObject(context_);
    Asset::RegisterObject(context_);

    // Define custom command line parameters here
    auto& cmd = GetCommandLineParser();
    cmd.add_option("project", defaultProjectPath_, "Project to open or create on startup.")->set_custom_option("dir");

    // Subcommands
    RegisterSubcommand<CookScene>();
    RegisterSubcommand<BuildAssets>();

    keyBindings_.Bind(ActionType::OpenProject, this, &Editor::OpenOrCreateProject);
    keyBindings_.Bind(ActionType::Exit, this, &Editor::OnExitHotkeyPressed);
}

void Editor::Start()
{
    // Execute specified subcommand and exit.
    for (SharedPtr<SubCommand>& cmd : subCommands_)
    {
        if (GetCommandLineParser().got_subcommand(cmd->GetTypeName().c_str()))
        {
            context_->GetSubsystem<Log>()->SetLogFormat("%v");
            ExecuteSubcommand(cmd);
            engine_->Exit();
            return;
        }
    }

    // Continue with normal editor initialization
    Input* input = context_->GetSubsystem<Input>();
    context_->RegisterSubsystem(new SceneManager(context_));
    context_->RegisterSubsystem(new EditorIconCache(context_));
    input->SetMouseMode(MM_ABSOLUTE);
    input->SetMouseVisible(true);
    input->SetEnabled(false);

    context_->GetSubsystem<ResourceCache>()->SetAutoReloadResources(true);
    engine_->SetAutoExit(false);

    SubscribeToEvent(E_UPDATE, [this](StringHash, VariantMap& args) { OnUpdate(args); });

    // Creates console but makes sure it's UI is not rendered. Console rendering is done manually in editor.
    auto* console = engine_->CreateConsole();
    console->SetAutoVisibleOnError(false);
    context_->GetSubsystem<FileSystem>()->SetExecuteConsoleCommands(false);
    SubscribeToEvent(E_CONSOLECOMMAND, [this](StringHash, VariantMap& args) { OnConsoleCommand(args); });
    console->RefreshInterpreters();

    SubscribeToEvent(E_ENDFRAME, [this](StringHash, VariantMap&) { OnEndFrame(); });
    SubscribeToEvent(E_EXITREQUESTED, [this](StringHash, VariantMap&) { OnExitRequested(); });
    SubscribeToEvent(E_EDITORPROJECTSERIALIZE, [this](StringHash, VariantMap&) { UpdateWindowTitle(); });
    SubscribeToEvent(E_CONSOLEURICLICK, [this](StringHash, VariantMap& args) { OnConsoleUriClick(args); });
    SubscribeToEvent(E_EDITORSELECTIONCHANGED, &Editor::OnSelectionChanged);
    SetupSystemUI();
    if (!defaultProjectPath_.empty())
    {
        ui::GetIO().IniFilename = nullptr;  // Avoid creating imgui.ini in some cases
        OpenProject(defaultProjectPath_);
    }

    // Hud will be rendered manually.
    context_->GetSubsystem<Engine>()->CreateDebugHud()->SetMode(DEBUGHUD_SHOW_NONE);
}

void Editor::ExecuteSubcommand(SubCommand* cmd)
{
    if (!defaultProjectPath_.empty())
    {
        project_ = new Project(context_);
        context_->RegisterSubsystem(project_);
        if (!project_->LoadProject(defaultProjectPath_))
        {
            URHO3D_LOGERRORF("Loading project '%s' failed.", pendingOpenProject_.c_str());
            exitCode_ = EXIT_FAILURE;
            engine_->Exit();
            return;
        }
    }

    cmd->Execute();
}

void Editor::Stop()
{
    // Save editor settings
    if (!engine_->IsHeadless())
    {
        // Save window geometry
        auto* graphics = GetSubsystem<Graphics>();
        windowPos_ = graphics->GetWindowPosition();
        windowSize_ = graphics->GetSize();

        auto* fs = context_->GetSubsystem<FileSystem>();
        ea::string editorSettingsDir = fs->GetAppPreferencesDir("rbfx", "Editor");
        if (!fs->DirExists(editorSettingsDir))
            fs->CreateDir(editorSettingsDir);

        JSONFile json(context_);
        JSONOutputArchive archive(&json);
        if (Serialize(archive))
        {
            if (!json.SaveFile(editorSettingsDir + "Editor.json"))
                URHO3D_LOGERROR("Saving of editor settings failed.");
        }
        else
            URHO3D_LOGERROR("Serializing of editor settings failed.");
    }

    context_->GetSubsystem<WorkQueue>()->Complete(0);
    if (auto* manager = GetSubsystem<SceneManager>())
        manager->UnloadAll();
    CloseProject();
    context_->RemoveSubsystem<WorkQueue>(); // Prevents deadlock when unloading plugin AppDomain in managed host.
    context_->RemoveSubsystem<Editor>();
}

void Editor::OnUpdate(VariantMap& args)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    flags |= ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar();

    RenderMenuBar();
    RenderSettingsWindow();

    bool hasModified = false;
    if (project_.NotNull())
    {
        dockspaceId_ = ui::GetID("Root");
        ui::DockSpace(dockspaceId_);

        auto tabsCopy = tabs_;
        for (auto& tab : tabsCopy)
        {
            if (tab->RenderWindow())
            {
                // Only active window may override another active window
                if (activeTab_ != tab && tab->IsActive())
                {
                    activeTab_ = tab;
                    tab->OnFocused();
                }
                hasModified |= tab->IsModified();
            }
            else if (!tab->IsUtility())
                // Content tabs get closed permanently
                tabs_.erase(tabs_.find(tab));
        }

        if (loadDefaultLayout_ && project_)
        {
            loadDefaultLayout_ = false;
            LoadDefaultLayout();
        }
    }
    else
    {
        // Render start page
        auto& style = ui::GetStyle();
        auto* lists = ui::GetWindowDrawList();
        ImRect rect{ui::GetWindowContentRegionMin(), ui::GetWindowContentRegionMax()};

        ImVec2 tileSize{200, 200};
        ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10, 10});

        ui::SetCursorPos(rect.GetCenter() - ImVec2{tileSize.x * 1.5f + 10, tileSize.y * 1.5f + 10});

        ui::BeginGroup();

        struct State
        {
            explicit State(Editor* editor)
            {
                FileSystem *fs = editor->GetContext()->GetSubsystem<FileSystem>();
                StringVector& recents = editor->recentProjects_;
                snapshots_.resize(recents.size());
                for (int i = 0; i < recents.size();)
                {
                    const ea::string& projectPath = recents[i];
                    ea::string snapshotFile = AddTrailingSlash(projectPath) + ".snapshot.png";
                    if (fs->FileExists(snapshotFile))
                    {
                        Image img(editor->context_);
                        if (img.LoadFile(snapshotFile))
                        {
                            SharedPtr<Texture2D> texture(editor->context_->CreateObject<Texture2D>());
                            texture->SetData(&img);
                            snapshots_[i] = texture;
                        }
                    }
                    ++i;
                }
            }

            ea::vector<SharedPtr<Texture2D>> snapshots_;
        };

        auto* state = ui::GetUIState<State>(this);
        const StringVector& recents = recentProjects_;

        int index = 0;
        for (int row = 0; row < 3; row++)
        {
            for (int col = 0; col < 3; col++, index++)
            {
                SharedPtr<Texture2D> snapshot;
                if (state->snapshots_.size() > index)
                    snapshot = state->snapshots_[index];

                if (recents.size() <= index || (row == 2 && col == 2))  // Last tile never shows a project.
                {
                    if (ui::Button("Open/Create Project", tileSize))
                        OpenOrCreateProject();
                }
                else
                {
                    const ea::string& projectPath = recents[index];
                    if (snapshot.NotNull())
                    {
                        if (ui::ImageButton(snapshot.Get(), tileSize - style.ItemInnerSpacing * 2))
                            OpenProject(projectPath);
                    }
                    else
                    {
                        if (ui::Button(recents[index].c_str(), tileSize))
                            OpenProject(projectPath);
                    }
                    if (ui::IsItemHovered())
                        ui::SetTooltip("%s", projectPath.c_str());
                }
                ui::SameLine();
            }
            ui::NewLine();
        }

        ui::EndGroup();
        ui::PopStyleVar();
    }

    ui::End();
    ImGui::PopStyleVar();

    // Dialog for a warning when application is being closed with unsaved resources.
    if (exiting_)
    {
        if (!context_->GetSubsystem<WorkQueue>()->IsCompleted(0))
        {
            ui::OpenPopup("Completing Tasks");

            if (ui::BeginPopupModal("Completing Tasks", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                                                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_Popup))
            {
                ui::TextUnformatted("Some tasks are in progress and are being completed. Please wait.");
                static float totalIncomplete = context_->GetSubsystem<WorkQueue>()->GetNumIncomplete(0);
                ui::ProgressBar(100.f / totalIncomplete * Min(totalIncomplete - (float)context_->GetSubsystem<WorkQueue>()->GetNumIncomplete(0), totalIncomplete));
                ui::EndPopup();
            }
        }
        else if (hasModified)
        {
            ui::OpenPopup("Save All?");

            if (ui::BeginPopupModal("Save All?", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                                                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_Popup))
            {
                ui::TextUnformatted("You have unsaved resources. Save them before exiting?");

                if (ui::Button(ICON_FA_SAVE " Save & Close"))
                {
                    for (auto& tab : tabs_)
                    {
                        if (tab->IsModified())
                            tab->SaveResource();
                    }
                    ui::CloseCurrentPopup();
                }

                ui::SameLine();

                if (ui::Button(ICON_FA_EXCLAMATION_TRIANGLE " Close without saving"))
                {
                    engine_->Exit();
                }
                ui::SetHelpTooltip(ICON_FA_EXCLAMATION_TRIANGLE " All unsaved changes will be lost!", KEY_UNKNOWN);

                ui::SameLine();

                if (ui::Button(ICON_FA_TIMES " Cancel"))
                {
                    exiting_ = false;
                    ui::CloseCurrentPopup();
                }

                ui::EndPopup();
            }
        }
        else
        {
            context_->GetSubsystem<WorkQueue>()->Complete(0);
            if (project_.NotNull())
            {
                project_->SaveProject();
                CloseProject();
            }
            engine_->Exit();
        }
    }
}

Tab* Editor::CreateTab(StringHash type)
{
    SharedPtr<Tab> tab(DynamicCast<Tab>(context_->CreateObject(type)));
    tabs_.push_back(tab);
    return tab.Get();
}

StringVector Editor::GetObjectsByCategory(const ea::string& category)
{
    StringVector result;
    const auto& factories = context_->GetObjectFactories();
    auto it = context_->GetObjectCategories().find(category);
    if (it != context_->GetObjectCategories().end())
    {
        for (const StringHash& type : it->second)
        {
            auto jt = factories.find(type);
            if (jt != factories.end())
                result.push_back(jt->second->GetTypeName());
        }
    }
    return result;
}

void Editor::OnConsoleCommand(VariantMap& args)
{
    using namespace ConsoleCommand;
    if (args[P_COMMAND].GetString() == "revision")
        URHO3D_LOGINFOF("Engine revision: %s", GetRevision());
}

void Editor::OnEndFrame()
{
    // Opening a new project must be done at the point when SystemUI is not in use. End of the frame is a good
    // candidate. This subsystem will be recreated.
    if (!pendingOpenProject_.empty())
    {
        CloseProject();
        // Reset SystemUI so that imgui loads it's config proper.
        context_->RemoveSubsystem<SystemUI>();
        unsigned flags = engineParameters_[EP_SYSTEMUI_FLAGS].GetUInt();
        context_->RegisterSubsystem(new SystemUI(context_, flags));
        SetupSystemUI();

        project_ = new Project(context_);
        context_->RegisterSubsystem(project_);
        bool loaded = project_->LoadProject(pendingOpenProject_);
        // SystemUI has to be started after loading project, because project sets custom settings file path. Starting
        // subsystem reads this file and loads settings.
        if (loaded)
        {
            auto* fs = context_->GetSubsystem<FileSystem>();
            loadDefaultLayout_ = project_->NeeDefaultUIPlacement();
            StringVector& recents = recentProjects_;
            // Remove latest project if it was already opened or any projects that no longer exists.
            for (auto it = recents.begin(); it != recents.end();)
            {
                if (*it == pendingOpenProject_ || !fs->DirExists(*it))
                    it = recents.erase(it);
                else
                    ++it;
            }
            // Latest project goes to front
            recents.insert(recents.begin(), pendingOpenProject_);
            // Limit recents list size
            if (recents.size() > 10)
                recents.resize(10);
        }
        else
        {
            CloseProject();
            URHO3D_LOGERROR("Loading project failed.");
        }
        pendingOpenProject_.clear();
    }
}

void Editor::OnExitRequested()
{
    if (auto* preview = GetTab<PreviewTab>())
    {
        if (preview->GetSceneSimulationStatus() != SCENE_SIMULATION_STOPPED)
            preview->Stop();
    }
    exiting_ = true;
}

void Editor::OnExitHotkeyPressed()
{
    if (!exiting_)
        OnExitRequested();
}

void Editor::CreateDefaultTabs()
{
    for (StringHash type : DEFAULT_TAB_TYPES)
        context_->RemoveSubsystem(type);
    tabs_.clear();

    for (StringHash type : DEFAULT_TAB_TYPES)
    {
        SharedPtr<Tab> tab;
        tab.StaticCast(context_->CreateObject(type));
        tabs_.push_back(tab);
    }
}

void Editor::LoadDefaultLayout()
{
    CreateDefaultTabs();

    auto* inspector = GetTab<InspectorTab>();
    auto* hierarchy = GetTab<HierarchyTab>();
    auto* resources = GetTab<ResourceTab>();
    auto* console = GetTab<ConsoleTab>();
    auto* preview = GetTab<PreviewTab>();
    auto* scene = GetTab<SceneTab>();
    auto* profiler = GetTab<ProfilerTab>();
    profiler->SetOpen(false);

    ImGui::DockBuilderRemoveNode(dockspaceId_);
    ImGui::DockBuilderAddNode(dockspaceId_, 0);
    ImGui::DockBuilderSetNodeSize(dockspaceId_, ui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspaceId_;
    ImGuiID dockHierarchy = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
    ImGuiID dockResources = ImGui::DockBuilderSplitNode(dockHierarchy, ImGuiDir_Down, 0.40f, nullptr, &dockHierarchy);
    ImGuiID dockInspector = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
    ImGuiID dockLog = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

    ImGui::DockBuilderDockWindow(hierarchy->GetUniqueTitle().c_str(), dockHierarchy);
    ImGui::DockBuilderDockWindow(resources->GetUniqueTitle().c_str(), dockLog);
    ImGui::DockBuilderDockWindow(console->GetUniqueTitle().c_str(), dockLog);
    ImGui::DockBuilderDockWindow(profiler->GetUniqueTitle().c_str(), dockLog);
    ImGui::DockBuilderDockWindow(scene->GetUniqueTitle().c_str(), dock_main_id);
    ImGui::DockBuilderDockWindow(preview->GetUniqueTitle().c_str(), dock_main_id);
    ImGui::DockBuilderDockWindow(inspector->GetUniqueTitle().c_str(), dockInspector);
    ImGui::DockBuilderFinish(dockspaceId_);

    scene->Activate();
}

void Editor::OpenProject(const ea::string& projectPath)
{
    pendingOpenProject_ = AddTrailingSlash(projectPath);
}

void Editor::CloseProject()
{
    SendEvent(E_EDITORPROJECTCLOSING);
    context_->RemoveSubsystem<Project>();
    for (StringHash type : DEFAULT_TAB_TYPES)
        context_->RemoveSubsystem(type);
    tabs_.clear();
    project_.Reset();
}

Tab* Editor::GetTabByName(const ea::string& uniqueName)
{
    for (auto& tab : tabs_)
    {
        if (tab->GetUniqueName() == uniqueName)
            return tab.Get();
    }
    return nullptr;
}

Tab* Editor::GetTabByResource(const ea::string& resourceName)
{
    for (auto& tab : tabs_)
    {
        auto resource = DynamicCast<BaseResourceTab>(tab);
        if (resource && resource->GetResourceName() == resourceName)
            return resource.Get();
    }
    return nullptr;
}

Tab* Editor::GetTab(StringHash type)
{
    for (auto& tab : tabs_)
    {
        if (tab->GetType() == type)
            return tab.Get();
    }
    return nullptr;
}

void Editor::SetupSystemUI()
{
    auto& io = ui::GetIO();
    auto& style = ImGui::GetStyleTemplate();
    static ImWchar fontAwesomeIconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    static ImWchar notoSansRanges[] = {0x20, 0x52f, 0x1ab0, 0x2189, 0x2c60, 0x2e44, 0xa640, 0xab65, 0};
    static ImWchar notoMonoRanges[] = {0x20, 0x513, 0x1e00, 0x1f4d, 0};
    SystemUI* systemUI = GetSubsystem<SystemUI>();

    systemUI->ApplyStyleDefault(true, 1.0f);
    systemUI->AddFont("Fonts/NotoSans-Regular.ttf", notoSansRanges, 16.f);
    systemUI->AddFont("Fonts/" FONT_ICON_FILE_NAME_FAS, fontAwesomeIconRanges, 14.f, true);
    monoFont_ = systemUI->AddFont("Fonts/NotoMono-Regular.ttf", notoMonoRanges, 14.f);
    systemUI->AddFont("Fonts/" FONT_ICON_FILE_NAME_FAS, fontAwesomeIconRanges, 12.f, true);
    style.WindowRounding = 3;
    // Disable imgui saving ui settings on it's own. These should be serialized to project file.
#if URHO3D_SYSTEMUI_VIEWPORTS
    io.ConfigViewportsNoAutoMerge = true;
#endif
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.ConfigWindowsResizeFromEdges = true;

    // TODO: Make configurable.
    style.FrameBorderSize = 0;
    style.WindowBorderSize = 1;
    style.ItemSpacing = {4, 4};
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border]                 = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.26f, 0.26f, 0.26f, 0.40f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.78f, 0.88f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.44f, 0.44f, 0.44f, 0.35f);

    ImGuiSettingsHandler handler;
    handler.TypeName = "Project";
    handler.TypeHash = ImHashStr(handler.TypeName, 0, 0);
    handler.ReadOpenFn = [](ImGuiContext* context, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return (void*) name;
    };
    handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
    {
        auto* systemUI = ui::GetSystemUI();
        auto* editor = systemUI->GetSubsystem<Editor>();
        const char* name = static_cast<const char*>(entry);
        if (strcmp(name, "Window") == 0)
            editor->CreateDefaultTabs();
        else
        {
            Tab* tab = editor->GetTabByName(name);
            if (tab == nullptr)
            {
                StringVector parts = ea::string(name).split('#');
                tab = editor->CreateTab(parts.front());
            }
            tab->OnLoadUISettings(name, line);
        }
    };
    handler.WriteAllFn = [](ImGuiContext* imgui_ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
    {
        auto* systemUI = ui::GetSystemUI();
        auto* editor = systemUI->GetSubsystem<Editor>();
        buf->appendf("[Project][Window]\n");
        // Save tabs
        for (auto& tab : editor->GetContentTabs())
            tab->OnSaveUISettings(buf);
    };
    ui::GetCurrentContext()->SettingsHandlers.push_back(handler);
}

void Editor::UpdateWindowTitle(const ea::string& resourcePath)
{
    if (context_->GetSubsystem<Engine>()->IsHeadless())
        return;

    auto* project = GetSubsystem<Project>();
    ea::string title;
    if (project == nullptr)
        title = "Editor";
    else
    {
        ea::string projectName = GetFileName(RemoveTrailingSlash(project->GetProjectPath()));
        title = ToString("Editor | %s", projectName.c_str());
        if (!resourcePath.empty())
            title += ToString(" | %s", GetFileName(resourcePath).c_str());
    }

    context_->GetSubsystem<Graphics>()->SetWindowTitle(title);
}

template<typename T>
void Editor::RegisterSubcommand()
{
    T::RegisterObject(context_);
    SharedPtr<T> cmd(context_->CreateObject<T>());
    subCommands_.push_back(DynamicCast<SubCommand>(cmd));
    if (CLI::App* subCommand = GetCommandLineParser().add_subcommand(T::GetTypeNameStatic().c_str()))
        cmd->RegisterCommandLine(*subCommand);
    else
        URHO3D_LOGERROR("Sub-command '{}' was not registered due to user error.", T::GetTypeNameStatic());
}

void Editor::OpenOrCreateProject()
{
    nfdchar_t* projectDir = nullptr;
    if (NFD_PickFolder("", &projectDir) == NFD_OKAY)
    {
        OpenProject(projectDir);
        NFD_FreePath(projectDir);
    }
}

#if URHO3D_STATIC && URHO3D_PLUGINS
bool Editor::RegisterPlugin(PluginApplication* plugin)
{
    return project_->GetPlugins()->RegisterPlugin(plugin);
}
#endif

void Editor::OnConsoleUriClick(VariantMap& args)
{
    using namespace ConsoleUriClick;
    if (ui::IsMouseClicked(MOUSEB_LEFT))
    {
        const ea::string& protocol = args[P_PROTOCOL].GetString();
        const ea::string& address = args[P_ADDRESS].GetString();
        if (protocol == "res")
            context_->GetSubsystem<FileSystem>()->SystemOpen(context_->GetSubsystem<ResourceCache>()->GetResourceFileName(address));
    }
}

void Editor::OnSelectionChanged(StringHash, VariantMap& args)
{
    using namespace EditorSelectionChanged;
    auto tab = static_cast<Tab*>(args[P_TAB].GetPtr());
    auto undo = GetSubsystem<UndoStack>();
    ByteVector newSelection = tab->SerializeSelection();
    if (tab == selectionTab_)
    {
        if (newSelection == selectionBuffer_)
            return;
    }
    else
    {
        if (!selectionTab_.Expired())
            selectionTab_->ClearSelection();
    }
    undo->Add<UndoSetSelection>(selectionTab_, selectionBuffer_, tab, newSelection);
    selectionTab_ = tab;
    selectionBuffer_ = newSelection;
}

}
