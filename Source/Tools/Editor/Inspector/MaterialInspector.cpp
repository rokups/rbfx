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
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/SystemUI/SystemUI.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/FileSystem.h>

#include <EASTL/sort.h>
#include <EASTL/functional.h>

#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <ImGui/imgui_stdlib.h>
#include <Toolbox/Common/UndoStack.h>
#include <Toolbox/SystemUI/Widgets.h>
#include <Toolbox/SystemUI/AttributeInspector.h>

#include "ModelPreview.h"
#include "Editor.h"
#include "Tabs/Scene/SceneTab.h"
#include "Tabs/InspectorTab.h"
#include "MaterialInspector.h"
#include "MaterialInspectorUndo.h"

namespace Urho3D
{

MaterialInspector::MaterialInspector(Context* context)
    : Object(context)
{
    auto* editor = GetSubsystem<Editor>();
    editor->onInspect_.Subscribe(this, &MaterialInspector::RenderInspector);
}

void MaterialInspector::RenderInspector(InspectArgs& args)
{
    auto* material = args.object_.Expired() ? nullptr : args.object_->Cast<Material>();
    if (material == nullptr)
        return;

    args.handledTimes_++;
    ui::IdScope idScope(material);
    ImGuiStyle& style = ui::GetStyle();

    if (!ui::CollapsingHeader(material->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    auto undo = GetSubsystem<UndoStack>();
    auto&& save = [name=material->GetName()](Context* context)
    {
        auto* cache = context->GetSubsystem<ResourceCache>();
        if (auto material = cache->GetResource<Material>(name))
        {
            cache->IgnoreResourceReload(material);
            material->SaveFile(cache->GetResourceFileName(material->GetName()));
        }
    };


    // Render a material preview
    auto* preview = ui::GetUIState<ModelPreview>(context_);
    if (preview->GetMaterial(0) == nullptr)
        preview->SetMaterial(material, 0);
    preview->RenderPreview();
    ui::SetHelpTooltip("Click to switch object.");
    if (ui::IsItemClicked(MOUSEB_LEFT))
        preview->ToggleModel();

    // Cull
    {
        int value = material->GetCullMode(), valuePrev = value;
        ui::ItemLabel("Cull");
        if (ui::Combo("###Cull", &value, cullModeNames, MAX_CULLMODES) && value != valuePrev)
        {
            material->SetCullMode(static_cast<CullMode>(value));
            undo->Add<UndoResourceSetter<Material, CullMode>>(material->GetName(), static_cast<CullMode>(valuePrev),
                                                              static_cast<CullMode>(value), &Material::SetCullMode)->Redo(context_);
        }
    }
    // Shadow Cull
    {
        int value = material->GetShadowCullMode(), valuePrev = value;
        ui::ItemLabel("Shadow Cull");
        if (ui::Combo("###Shadow Cull", &value, cullModeNames, MAX_CULLMODES) && value != valuePrev)
        {
            material->SetShadowCullMode(static_cast<CullMode>(value));
            undo->Add<UndoResourceSetter<Material, CullMode>>(material->GetName(), static_cast<CullMode>(valuePrev),
                                                              static_cast<CullMode>(value), &Material::SetShadowCullMode)->Redo(context_);
        }
    }
    // Fill Mode
    {
        int value = material->GetFillMode(), valuePrev = value;
        ui::ItemLabel("Fill Mode");
        if (ui::Combo("###Fill Mode", &value, fillModeNames, MAX_FILLMODES) && value != valuePrev)
        {
            material->SetFillMode(static_cast<FillMode>(value));
            undo->Add<UndoResourceSetter<Material, FillMode>>(material->GetName(), static_cast<FillMode>(valuePrev),
                                                              static_cast<FillMode>(value), &Material::SetFillMode)->Redo(context_);
        }
    }
    // Alpha To Coverage
    {
        bool value = material->GetAlphaToCoverage();
        ui::ItemLabel("Alpha To Coverage");
        if (ui::Checkbox("###Alpha To Coverage", &value))
        {
            material->SetAlphaToCoverage(value);
            undo->Add<UndoResourceSetter<Material, bool>>(material->GetName(), !value, value, &Material::SetAlphaToCoverage);
        }
    }
    // Line Anti Alias
    {
        bool value = material->GetLineAntiAlias();
        ui::ItemLabel("Line Anti Alias");
        // if (flags & ui::ItemLabelFlag::Right)
        //     ui::ItemAlign(ui::GetFrameHeight());
        if (ui::Checkbox("###Line Anti Alias", &value))
        {
            material->SetLineAntiAlias(value);
            undo->Add<UndoResourceSetter<Material, bool>>(material->GetName(), !value, value, &Material::SetLineAntiAlias);
        }
    }
    // Render Order
    {
        ui::PushID("Render Order");
        auto&& setRenderOrder = [name=material->GetName()](Context* context, unsigned char value)
        {
            auto* cache = context->GetSubsystem<ResourceCache>();
            if (auto* material = cache->GetResource<Material>(name))
            {
                material->SetRenderOrder(value);
                return true;
            }
            return false;
        };
        ui::ItemLabel("Render Order");
        if (auto mod = undo->Track<UndoCustomAction<unsigned char>>(material->GetRenderOrder(), setRenderOrder, save))
            mod.SetModified(ui::DragScalar("###Render Order", ImGuiDataType_U8, &mod.value_, 0.1f));
        ui::PopID();
    }
    // Occlusion
    {
        bool value = material->GetOcclusion();
        ui::ItemLabel("Occlusion");
        // if (flags & ui::ItemLabelFlag::Right)
        //     ui::ItemAlign(ui::GetFrameHeight());
        if (ui::Checkbox("###Occlusion", &value))
        {
            material->SetOcclusion(value);
            undo->Add<UndoResourceSetter<Material, bool>>(material->GetName(), !value, value, &Material::SetOcclusion);
        }
    }
    // Constant Bias
    {
        ui::PushID("Constant Bias");
        auto&& setDepthBias = [name=material->GetName()](Context* context, float value)
        {
            auto* cache = context->GetSubsystem<ResourceCache>();
            if (auto* material = cache->GetResource<Material>(name))
            {
                auto bias = material->GetDepthBias();
                bias.constantBias_ = value;
                material->SetDepthBias(bias);
                return true;
            }
            return false;
        };
        ui::ItemLabel("Constant Bias");
        if (auto mod = undo->Track<UndoCustomAction<float>>(material->GetDepthBias().constantBias_, setDepthBias, save))
            mod.SetModified(ui::DragScalar("###Constant Bias", ImGuiDataType_Float, &mod.value_, 0.01f));
        ui::PopID();
    }
    // Slope Scaled Bias
    {
        ui::PushID("Slope Scaled Bias");
        auto&& setSlopeScaledBias = [name=material->GetName()](Context* context, float value)
        {
            auto* cache = context->GetSubsystem<ResourceCache>();
            if (auto* material = cache->GetResource<Material>(name))
            {
                auto bias = material->GetDepthBias();
                bias.slopeScaledBias_ = value;
                material->SetDepthBias(bias);
                return true;
            }
            return false;
        };
        ui::ItemLabel("Slope Scaled Bias");
        if (auto mod = undo->Track<UndoCustomAction<float>>(material->GetDepthBias().slopeScaledBias_, setSlopeScaledBias, save))
            mod.SetModified(ui::DragScalar("###Slope Scaled Bias", ImGuiDataType_Float, &mod.value_, 0.01f));
        ui::PopID();
    }
    // Normal Offset
    {
        ui::PushID("Normal Offset");
        auto&& setNormalOffset = [name=material->GetName()](Context* context, float value)
        {
            auto* cache = context->GetSubsystem<ResourceCache>();
            if (auto* material = cache->GetResource<Material>(name))
            {
                auto bias = material->GetDepthBias();
                bias.normalOffset_ = value;
                material->SetDepthBias(bias);
                return true;
            }
            return false;
        };
        ui::ItemLabel("Normal Offset");
        if (auto mod = undo->Track<UndoCustomAction<float>>(material->GetDepthBias().normalOffset_, setNormalOffset, save))
            mod.SetModified(ui::DragScalar("###Normal Offset", ImGuiDataType_Float, &mod.value_, 0.01f));
        ui::PopID();
    }
    // Techniques
    {
        ui::TextCentered("Techniques");
        ui::Separator();

        struct TechniquesCache
        {
            Context* context_;
            StringVector techniques_;
            unsigned lastScan_ = 0;

            explicit TechniquesCache(Context* context) : context_(context) { }

            void Update()
            {
                unsigned now = Time::GetSystemTime();
                if (now - lastScan_ < 1000)
                    return;
                lastScan_ = now;
                auto* cache = context_->GetSubsystem<ResourceCache>();
                techniques_.clear();
                cache->Scan(techniques_, "Techniques", "*.xml", SCAN_FILES, true);
                for (ea::string& name : techniques_)
                    name.insert(0, "Techniques/");
                ea::quick_sort(techniques_.begin(), techniques_.end());
            }
        };

        auto* techniquesCache = ui::GetUIState<TechniquesCache>(context_);
        techniquesCache->Update();

        for (unsigned i = 0; i < material->GetNumTechniques(); i++)
        {
            if (i > 0)
                ui::Separator();

            ui::IdScope idScope(i);

            auto& tech = material->GetTechniqueEntry(i);
            auto* techName = ui::GetUIState<ea::string>(tech.technique_->GetName());
            bool modifiedInput = *techName != tech.technique_->GetName();

            // Input
            if (modifiedInput)
                ui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ui::ItemLabel("Technique");
            ui::SetNextItemWidth(ui::CalcItemWidth() - (ui::IconButtonSize() + style.ItemSpacing.x) * 2);
            bool modified = ui::InputText("##techniqueName", techName, ImGuiInputTextFlags_EnterReturnsTrue);
            if (modifiedInput)
                ui::PopStyleColor();
            ui::SetHelpTooltip("Drag resource here.");
            // Autocomplete
            modified |= ui::Autocomplete(ui::GetID("##techniqueName"), techName, &techniquesCache->techniques_);
            // Drop target
            if (ui::BeginDragDropTarget())
            {
                const Variant& payload = ui::AcceptDragDropVariant(Technique::GetTypeStatic().ToString());
                if (!payload.IsEmpty())
                {
                    *techName = payload.GetString();
                    modified = true;
                }
                ui::EndDragDropTarget();
            }
            // Undo
            if (modified)
            {
                auto* cache = GetSubsystem<ResourceCache>();
                if (auto* technique = cache->GetResource<Technique>(*techName))
                {
                    // Track
                    undo->Add<UndoCustomAction<const ea::string>>(tech.technique_->GetName(), *techName,
                        [name=material->GetName(), index=i](Context* context, const ea::string& value)
                        {
                            auto* cache = context->GetSubsystem<ResourceCache>();
                            if (auto* material = cache->GetResource<Material>(name))
                            {
                                if (auto* technique = cache->GetResource<Technique>(value))
                                {
                                    const TechniqueEntry& entry = material->GetTechniqueEntry(index);
                                    material->SetTechnique(index, technique, entry.qualityLevel_, entry.lodDistance_);
                                    return true;
                                }
                            }
                            return false;
                        }, save);
                    // Update
                    const TechniqueEntry& entry = material->GetTechniqueEntry(i);
                    material->SetTechnique(i, technique, entry.qualityLevel_, entry.lodDistance_);
                }
            }
            else if (*techName != tech.technique_->GetName() && !ui::IsItemActive() && !ui::IsItemFocused())
                *techName = tech.technique_->GetName();     // Apply change from undo to current buffer

            // Locate
            ui::SameLine();
            if (ui::IconButton(ICON_FA_CROSSHAIRS))
                SendEvent(E_INSPECTORLOCATERESOURCE, InspectorLocateResource::P_NAME, material->GetTechnique(i)->GetName());
            ui::SetHelpTooltip("Locate resource");
            // Delete
            ui::SameLine();
            if (ui::IconButton(ICON_FA_TRASH))
            {
                struct TechniqueInfo
                {
                    /// Technique resource name.
                    ea::string name_;
                    /// Quality level.
                    MaterialQuality qualityLevel_;
                    /// LOD distance.
                    float lodDistance_;
                    /// Index in technique stack of material.
                    int index_;

                    TechniqueInfo(Context* context, const TechniqueEntry& entry, int index)
                        : name_(entry.original_->GetName()), qualityLevel_(entry.qualityLevel_)
                        , lodDistance_(entry.lodDistance_), index_(index)
                    {
                    }
                };

                undo->Add<UndoCustomAction<const TechniqueInfo>>(TechniqueInfo{context_, tech, (int)i}, TechniqueInfo{context_, tech, (int)i},
                    [name=material->GetName()](Context* context, const TechniqueInfo& info)
                    {
                        // Re-add
                        auto* cache = context->GetSubsystem<ResourceCache>();
                        auto* material = cache->GetResource<Material>(name);
                        auto* technique = cache->GetResource<Technique>(info.name_);
                        if (material == nullptr || technique == nullptr)
                            return false;

                        material->SetNumTechniques(material->GetNumTechniques() + 1);
                        // Shift techniques towars the end by 1 to make space for insertion.
                        for (int i = static_cast<int>(material->GetNumTechniques()) - 2; i >= info.index_; i--)
                        {
                            const TechniqueEntry& entry = material->GetTechniqueEntry(i);
                            material->SetTechnique(i + 1, entry.original_.Get(), entry.qualityLevel_, entry.lodDistance_);
                        }
                        // Insert back
                        material->SetTechnique(info.index_, technique, info.qualityLevel_, info.lodDistance_);
                        return true;
                    }, [name=material->GetName()](Context* context, const TechniqueInfo& info)
                    {
                        // Remove again
                        auto* cache = context->GetSubsystem<ResourceCache>();
                        auto* material = cache->GetResource<Material>(name);
                        if (material == nullptr)
                            return false;
                        for (int j = info.index_ + 1; j < material->GetNumTechniques(); j++)
                        {
                            const TechniqueEntry& entry = material->GetTechniqueEntry(j);
                            material->SetTechnique(j - 1, entry.original_.Get(), entry.qualityLevel_, entry.lodDistance_);
                        }
                        material->SetNumTechniques(material->GetNumTechniques() - 1);
                        return true;
                    }, save);

                // Technique removed possibly from the middle. Shift existing techniques back to the front and remove last one.
                for (auto j = i + 1; j < material->GetNumTechniques(); j++)
                {
                    const TechniqueEntry& entry = material->GetTechniqueEntry(j);
                    material->SetTechnique(j - 1, entry.original_.Get(), entry.qualityLevel_, entry.lodDistance_);
                }
                material->SetNumTechniques(material->GetNumTechniques() - 1);
            }
            // LOD Distance
            auto&& setLodDistance = [name=material->GetName(), index=i](Context* context, float value)
            {
                auto* cache = context->GetSubsystem<ResourceCache>();
                if (auto* material = cache->GetResource<Material>(name))
                {
                    const TechniqueEntry& entry = material->GetTechniqueEntry(index);
                    material->SetTechnique(index, entry.original_, entry.qualityLevel_, value);
                    return true;
                }
                return false;
            };
            ui::ItemLabel("LOD Distance");
            if (auto mod = undo->Track<UndoCustomAction<float>>(tech.lodDistance_, setLodDistance, save))
                mod.SetModified(ui::DragFloat("###LOD Distance", &mod.value_));
            // Quality
            static const char* qualityNames[] = {"low", "medium", "high", "max"};
            int quality = tech.qualityLevel_;
            ui::ItemLabel("Quality");
            if (ui::Combo("###Quality", &quality, qualityNames, URHO3D_ARRAYSIZE(qualityNames)) && quality != tech.qualityLevel_)
            {
                undo->Add<UndoCustomAction<const int>>(static_cast<int>(tech.qualityLevel_), quality,
                    [name=material->GetName(), index=i](Context* context, int value)
                    {
                        auto* cache = context->GetSubsystem<ResourceCache>();
                        if (auto* material = cache->GetResource<Material>(name))
                        {
                            const TechniqueEntry& entry = material->GetTechniqueEntry(index);
                            material->SetTechnique(index, entry.original_, static_cast<MaterialQuality>(value), entry.lodDistance_);
                            return true;
                        }
                        return false;
                    }, save)->Redo(context_);
            }
        }
        ui::Separator();
        ui::PushID("Add Technique");
        auto* newTechniqueName = ui::GetUIState<ea::string>();
        // New technique name
        ui::ItemLabel("Add Technique");
        bool modified = ui::InputTextWithHint("###Add Technique", "Enter technique path and press [Enter]", newTechniqueName, ImGuiInputTextFlags_EnterReturnsTrue);
        ui::SetHelpTooltip("Drag and drop technique resource here.");
        // Autocomplete
        modified |= ui::Autocomplete(ui::GetID("##techniqueName"), newTechniqueName, &techniquesCache->techniques_);
        // Drag and drop
        if (ui::BeginDragDropTarget())
        {
            const Variant& payload = ui::AcceptDragDropVariant(Technique::GetTypeStatic().ToString());
            if (!payload.IsEmpty())
            {
                *newTechniqueName = payload.GetString();
                modified = true;
            }
            ui::EndDragDropTarget();
        }
        // Save
        if (modified)
        {
            auto* cache = GetSubsystem<ResourceCache>();
            if (auto* technique = cache->GetResource<Technique>(*newTechniqueName))
            {
                auto index = material->GetNumTechniques();
                material->SetNumTechniques(index + 1);
                material->SetTechnique(index, technique);
                undo->Add<UndoCustomAction<const ea::string>>("", *newTechniqueName,
                    [name=material->GetName()](Context* context, const ea::string& techniqueName)
                    {
                        auto* cache = context->GetSubsystem<ResourceCache>();
                        if (auto* material = cache->GetResource<Material>(name))
                        {
                            if (techniqueName.empty())  // delete last
                                material->SetNumTechniques(material->GetNumTechniques() - 1);
                            else if (auto* technique = cache->GetResource<Technique>(techniqueName))
                            {
                                auto index = material->GetNumTechniques();
                                material->SetNumTechniques(index + 1);
                                material->SetTechnique(index, technique);
                            }
                            return true;
                        }
                        return false;
                    }, save);
                newTechniqueName->clear();
            }
        }
        ui::PopID();
        ui::Separator();
        ui::Separator();
    }
    // Shader Parameters
    {
        struct ShaderParameterState
        {
            ea::string fieldName_;
            int variantTypeIndex_ = 0;
        };

        ui::TextCentered("Shader parameters");
        ui::Separator();

        auto* paramState = ui::GetUIState<ShaderParameterState>();
        const auto& parameters = material->GetShaderParameters();
        for (const auto& pair : parameters)
        {
            const ea::string& parameterName = pair.second.name_;
            ui::IdScope pushId(parameterName.c_str());
            auto& value = ValueHistory<Variant>::Get(pair.second.value_);
            float width = ui::CalcItemWidth() - (ui::IconButtonSize() + style.ItemSpacing.x) * 1;

            // Shaders do not support Color type, but we would like to editr colors as shader parameters.
            Variant colorVariant;
            if (parameterName.ends_with("Color"))
            {
                if (pair.second.value_.GetType() == VariantType::Vector3)
                    colorVariant = Color(value.current_.GetVector3());
                else if (pair.second.value_.GetType() == VariantType::Vector4)
                    colorVariant = Color(value.current_.GetVector4());
            }

            if (RenderAttribute(parameterName, colorVariant.IsEmpty() ? value.current_ : colorVariant, Color::WHITE, "", nullptr, args.eventSender_, width))
            {
                if (!colorVariant.IsEmpty())
                {
                    if (pair.second.value_.GetType() == VariantType::Vector3)
                        value.current_ = colorVariant.GetColor().ToVector3();
                    else if (pair.second.value_.GetType() == VariantType::Vector4)
                        value.current_ = colorVariant.GetColor().ToVector4();
                }
                material->SetShaderParameter(parameterName, value.current_);
                value.SetModified(true);
            }
            if (value.IsModified())
                undo->Add<UndoShaderParameterChanged>(material, parameterName, value.initial_, value.current_);

            ui::SameLine();
            if (ui::Button(ICON_FA_TRASH))
            {
                undo->Add<UndoShaderParameterChanged>(material, parameterName, pair.second.value_, Variant{})
                    ->Redo(context_);
                break;
            }
        }
        // Add a new parameter
        {
            static const VariantType shaderParameterVariantTypes[] = {
                VariantType::Float,
                VariantType::Vector2,
                VariantType::Vector3,
                VariantType::Vector4,
            };

            static const char* shaderParameterVariantNames[] = {
                "Float",
                "Vector2",
                "Vector3",
                "Vector4",
            };

            ui::ItemLabel("Add Parameter");
            float width = ui::CalcItemWidth();
            ui::SetNextItemWidth(width * 0.2 - style.ItemSpacing.x);
            ui::Combo("###Type", &paramState->variantTypeIndex_, shaderParameterVariantNames, SDL_arraysize(shaderParameterVariantTypes));
            ui::SameLine();
            ui::SetHelpTooltip("Shader parameter type.");

            ui::SetNextItemWidth(width * 0.8 - style.ItemSpacing.x - ui::IconButtonSize());
            bool addNew = ui::InputTextWithHint("###Name", "Enter parameter name and press [Enter]", &paramState->fieldName_, ImGuiInputTextFlags_EnterReturnsTrue);
            ui::SameLine();
            ui::SetHelpTooltip("Shader parameter name.");

            addNew |= ui::Button(ICON_FA_CHECK);
            if (addNew)
            {
                if (!paramState->fieldName_.empty() && material->GetShaderParameter(paramState->fieldName_.c_str()).GetType() == VariantType::None)   // TODO: Show warning about duplicate name
                {
                    Variant value{shaderParameterVariantTypes[paramState->variantTypeIndex_]};
                    undo->Add<UndoShaderParameterChanged>(material, paramState->fieldName_.c_str(), Variant{}, value);
                    material->SetShaderParameter(paramState->fieldName_.c_str(), value);
                    paramState->fieldName_.clear();
                    paramState->variantTypeIndex_ = 0;
                }
            }
        }
        ui::Separator();
        ui::Separator();
    }
    ui::TextCentered("Textures");
    ui::Separator();
    for (auto i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; i++)
    {
        ui::IdScope pushTextureUnitId(i);

        ea::string finalName = Format("{} Texture", textureUnitNames[i]);
        ReplaceUTF8(finalName, 0, ToUpper(AtUTF8(finalName, 0)));
        auto textureUnit = static_cast<TextureUnit>(i);

        ResourceRef resourceRef;
        if (auto* texture = material->GetTexture(textureUnit))
            resourceRef = ResourceRef(texture->GetType(), texture->GetName());
        else
            resourceRef = ResourceRef(Texture2D::GetTypeStatic());  // FIXME: cubemap support.

        Variant resource(resourceRef);
        if (RenderAttribute(finalName, resource, Color::WHITE, "", nullptr, args.eventSender_))
        {
            const auto& ref = resource.GetResourceRef();
            auto cache = GetSubsystem<ResourceCache>();
            if (auto texture = dynamic_cast<Texture*>(cache->GetResource(ref.type_, ref.name_)))
                material->SetTexture(textureUnit, texture);

            undo->Add<UndoCustomAction<const ResourceRef>>(resourceRef, ref,
                [name=material->GetName(), textureUnit=textureUnit](Context* context, const ResourceRef& ref)
                {
                    auto cache = context->GetSubsystem<ResourceCache>();
                    if (auto material = cache->GetResource<Material>(name))
                    {
                        if (ref.name_.empty())
                            material->SetTexture(textureUnit, nullptr);
                        else if (auto texture = dynamic_cast<Texture*>(cache->GetResource(ref.type_, ref.name_)))
                        {
                            material->SetTexture(textureUnit, texture);
                            return true;
                        }
                    }
                    return false;
                }, save);
        }
    }
    ui::Separator();
    ui::Separator();
}

}
