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

/// \file

#pragma once

#include "../Container/FlagSet.h"
#include "../Container/Ptr.h"
#include "../Core/Variant.h"

namespace Urho3D
{

enum class AttributeMode
{
    /// Attribute shown only in the editor, but not serialized.
    Edit = 0x0,
    /// Attribute used for file serialization.
    File = 0x1,
    /// Attribute used for network replication.
    Net = 0x2,
    /// Attribute used for both file serialization and network replication (default).
    Default = 0x3,
    /// Attribute should use latest data grouping instead of delta update in network replication.
    LatestData = 0x4,
    /// Attribute should not be shown in the editor.
    NoEdit = 0x8,
    /// Attribute is a node ID and may need rewriting.
    NodeID = 0x10,
    /// Attribute is a component ID and may need rewriting.
    ComponentID = 0x20,
    /// Attribute is a node ID vector where first element is the amount of nodes.
    NodeIDVector = 0x40,
    /// Attribute is readonly. Can't be used with binary serialized objects.
    ReadOnly = 0x80,
};

#ifdef URHO3D_LEGACY_ENUMS
static const AttributeMode AM_EDIT = AttributeMode::Edit;
static const AttributeMode AM_FILE = AttributeMode::File;
static const AttributeMode AM_NET = AttributeMode::Net;
static const AttributeMode AM_DEFAULT = AttributeMode::Default;
static const AttributeMode AM_LATESTDATA = AttributeMode::LatestData;
static const AttributeMode AM_NOEDIT = AttributeMode::NoEdit;
static const AttributeMode AM_NODEID = AttributeMode::NodeID;
static const AttributeMode AM_COMPONENTID = AttributeMode::ComponentID;
static const AttributeMode AM_NODEIDVECTOR = AttributeMode::NodeIDVector;
static const AttributeMode AM_READONLY = AttributeMode::ReadOnly;
#endif

URHO3D_FLAGSET(AttributeMode, AttributeModeFlags);

class Serializable;

/// Abstract base class for invoking attribute accessors.
class URHO3D_API AttributeAccessor : public RefCounted
{
public:
    /// Construct.
    AttributeAccessor() = default;
    /// Get the attribute.
    virtual void Get(const Serializable* ptr, Variant& dest) const = 0;
    /// Set the attribute.
    virtual void Set(Serializable* ptr, const Variant& src) = 0;
};

/// Description of an automatically serializable variable.
struct AttributeInfo
{
    /// Construct empty.
    AttributeInfo() = default;
#ifndef SWIG
    /// Construct attribute.
    AttributeInfo(VariantType type, const char* name, const SharedPtr<AttributeAccessor>& accessor, const char** enumNames, const Variant& defaultValue, AttributeModeFlags mode) :
        type_(type),
        name_(name),
        nameHash_(name_),
        enumNames_(enumNames),
        accessor_(accessor),
        defaultValue_(defaultValue),
        mode_(mode)
    {
    }
#endif
    /// Construct attribute.
    AttributeInfo(VariantType type, const char* name, const SharedPtr<AttributeAccessor>& accessor, const ea::vector<ea::string>& enumNames, const Variant& defaultValue, AttributeModeFlags mode) :
        type_(type),
        name_(name),
        nameHash_(name_),
        enumNames_(nullptr),
        enumNamesStorage_(enumNames),
        accessor_(accessor),
        defaultValue_(defaultValue),
        mode_(mode)
    {
        InitializeEnumNamesFromStorage();
    }

    /// Copy attribute info.
    AttributeInfo(const AttributeInfo& other)
    {
        type_ = other.type_;
        name_ = other.name_;
        nameHash_ = other.nameHash_;
        enumNames_ = other.enumNames_;
        accessor_ = other.accessor_;
        defaultValue_ = other.defaultValue_;
        mode_ = other.mode_;
        metadata_ = other.metadata_;
        ptr_ = other.ptr_;
        enumNamesStorage_ = other.enumNamesStorage_;

        if (!enumNamesStorage_.empty())
            InitializeEnumNamesFromStorage();
    }

    /// Return attribute metadata.
    const Variant& GetMetadata(const StringHash& key) const
    {
        auto elem = metadata_.find(key);
        return elem != metadata_.end() ? elem->second : Variant::EMPTY;
    }

    /// Return attribute metadata of specified type.
    template <class T> T GetMetadata(const StringHash& key) const
    {
        return GetMetadata(key).Get<T>();
    }

    /// Return whether the attribute should be saved.
    bool ShouldSave() const { return (mode_ & AttributeMode::File) && !(mode_ & AttributeMode::ReadOnly); }

    /// Return whether the attribute should be loaded.
    bool ShouldLoad() const { return !!(mode_ & AttributeMode::File); }

    /// Instance equality operator.
    bool operator ==(const AttributeInfo& rhs) const
    {
        return this == &rhs;
    }

    /// Instance inequality operator.
    bool operator !=(const AttributeInfo& rhs) const
    {
        return this != &rhs;
    }

    /// Attribute type.
    VariantType type_ = VariantType::None;
    /// Name.
    ea::string name_;
    /// Name hash.
    StringHash nameHash_;
    /// Enum names.
    const char** enumNames_ = nullptr;
    /// Helper object for accessor mode.
    SharedPtr<AttributeAccessor> accessor_;
    /// Default value for network replication.
    Variant defaultValue_;
    /// Attribute mode: whether to use for serialization, network replication, or both.
    AttributeModeFlags mode_ = AttributeMode::Default;
    /// Attribute metadata.
    VariantMap metadata_;
    /// Attribute data pointer if elsewhere than in the Serializable.
    void* ptr_ = nullptr;
    /// List of enum names. Used when names can not be stored externally.
    ea::vector<ea::string> enumNamesStorage_;
    /// List of enum name pointers. Front of this vector will be assigned to enumNames_ when enumNamesStorage_ is in use.
    ea::vector<const char*> enumNamesPointers_;

private:
    void InitializeEnumNamesFromStorage()
    {
        if (enumNamesStorage_.empty())
            enumNames_ = nullptr;
        else
        {
            for (const auto& enumName : enumNamesStorage_)
                enumNamesPointers_.emplace_back(enumName.c_str());
            enumNamesPointers_.emplace_back(nullptr);
            enumNames_ = &enumNamesPointers_.front();
        }
    }
};

/// Attribute handle returned by Context::RegisterAttribute and used to chain attribute setup calls.
/// @nobind
struct AttributeHandle
{
    friend class Context;
public:
    /// Construct default.
    AttributeHandle() = default;
    /// Construct from another handle.
    AttributeHandle(const AttributeHandle& another) = default;
private:
    /// Attribute info.
    AttributeInfo* attributeInfo_ = nullptr;
    /// Network attribute info.
    AttributeInfo* networkAttributeInfo_ = nullptr;
public:
    /// Set metadata.
    AttributeHandle& SetMetadata(StringHash key, const Variant& value)
    {
        if (attributeInfo_)
            attributeInfo_->metadata_[key] = value;
        if (networkAttributeInfo_)
            networkAttributeInfo_->metadata_[key] = value;
        return *this;
    }
};

}
