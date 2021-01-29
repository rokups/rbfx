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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../IO/Archive.h"
#include "../IO/ArchiveSerialization.h"
#include "../IO/Deserializer.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/Serializer.h"
#include "../Resource/XMLElement.h"
#include "../Resource/XMLFile.h"
#include "../Resource/JSONFile.h"
#include "../Resource/JSONValue.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/ReplicationState.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/Serializable.h"

#include <EASTL/fixed_vector.h>

#include "../DebugNew.h"

namespace Urho3D
{

static const unsigned MAX_STACK_ATTRIBUTE_COUNT = 128;

static unsigned RemapAttributeIndex(const ea::vector<AttributeInfo>* attributes, const AttributeInfo& netAttr, unsigned netAttrIndex)
{
    if (!attributes)
        return netAttrIndex; // Could not remap

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        // Compare accessor to avoid name string compare
        if (attr.accessor_ && attr.accessor_ == netAttr.accessor_.Get())
            return i;
    }

    return netAttrIndex; // Could not remap
}

static bool SaveAttributeWithName(Archive& archive, const AttributeInfo& attr, const Variant& value)
{
    assert(!archive.IsInput());

    // Save attribute name
    if (!SerializeStringHashKey(archive, const_cast<StringHash&>(attr.nameHash_), attr.name_))
        return false;

    // Save attribute
    if (attr.enumNames_)
    {
        int enumValue = value.GetInt();
        return SerializeEnum<int, int>(archive, "attribute", attr.enumNames_, enumValue);
    }
    else
    {
        return SerializeVariantValue(archive, attr.type_, "attribute", const_cast<Variant&>(value));
    }
}

static bool LoadAttribute(Archive& archive, const AttributeInfo& attr, Variant& value)
{
    assert(archive.IsInput());

    // Read attribute value
    if (attr.enumNames_)
    {
        int enumValue{};
        if (!SerializeEnum<int, int>(archive, "attribute", attr.enumNames_, enumValue))
            return false;

        value = enumValue;
        return true;
    }
    else
    {
        // Reset value to default so it could use custom serialization
        if (attr.type_ == VariantType::Custom)
            value = attr.defaultValue_;

        return SerializeVariantValue(archive, attr.type_, "attribute", value);
    }
}

Serializable::Serializable(Context* context) :
    Object(context),
    setInstanceDefault_(false),
    temporary_(false)
{
}

Serializable::~Serializable() = default;

void Serializable::OnSetAttribute(const AttributeInfo& attr, const Variant& src)
{
    // TODO: may be could use an observer pattern here
    if (setInstanceDefault_)
        SetInstanceDefault(attr.name_, src);

    // Check for accessor function mode
    if (attr.accessor_)
    {
        attr.accessor_->Set(this, src);
        return;
    }

    // Get the destination address
    assert(attr.ptr_);
    void* dest = attr.ptr_;

    switch (attr.type_)
    {
    case VariantType::Int:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_)
            *(reinterpret_cast<unsigned char*>(dest)) = src.GetInt();
        else
            *(reinterpret_cast<int*>(dest)) = src.GetInt();
        break;

    case VariantType::Int64:
        *(reinterpret_cast<long long*>(dest)) = src.GetInt64();
        break;

    case VariantType::Bool:
        *(reinterpret_cast<bool*>(dest)) = src.GetBool();
        break;

    case VariantType::Float:
        *(reinterpret_cast<float*>(dest)) = src.GetFloat();
        break;

    case VariantType::Vector2:
        *(reinterpret_cast<Vector2*>(dest)) = src.GetVector2();
        break;

    case VariantType::Vector3:
        *(reinterpret_cast<Vector3*>(dest)) = src.GetVector3();
        break;

    case VariantType::Vector4:
        *(reinterpret_cast<Vector4*>(dest)) = src.GetVector4();
        break;

    case VariantType::Quaternion:
        *(reinterpret_cast<Quaternion*>(dest)) = src.GetQuaternion();
        break;

    case VariantType::Color:
        *(reinterpret_cast<Color*>(dest)) = src.GetColor();
        break;

    case VariantType::String:
        *(reinterpret_cast<ea::string*>(dest)) = src.GetString();
        break;

    case VariantType::Buffer:
        *(reinterpret_cast<ea::vector<unsigned char>*>(dest)) = src.GetBuffer();
        break;

    case VariantType::ResourceRef:
        *(reinterpret_cast<ResourceRef*>(dest)) = src.GetResourceRef();
        break;

    case VariantType::ResourceRefList:
        *(reinterpret_cast<ResourceRefList*>(dest)) = src.GetResourceRefList();
        break;

    case VariantType::VariantVector:
        *(reinterpret_cast<VariantVector*>(dest)) = src.GetVariantVector();
        break;

    case VariantType::StringVector:
        *(reinterpret_cast<StringVector*>(dest)) = src.GetStringVector();
        break;

    case VariantType::VariantMap:
        *(reinterpret_cast<VariantMap*>(dest)) = src.GetVariantMap();
        break;

    case VariantType::IntRect:
        *(reinterpret_cast<IntRect*>(dest)) = src.GetIntRect();
        break;

    case VariantType::IntVector2:
        *(reinterpret_cast<IntVector2*>(dest)) = src.GetIntVector2();
        break;

    case VariantType::IntVector3:
        *(reinterpret_cast<IntVector3*>(dest)) = src.GetIntVector3();
        break;

    case VariantType::Double:
        *(reinterpret_cast<double*>(dest)) = src.GetDouble();
        break;

    default:
        URHO3D_LOGERROR("Unsupported attribute type for OnSetAttribute()");
        return;
    }

    // If it is a network attribute then mark it for next network update
    if (attr.mode_ & AttributeMode::Net)
        MarkNetworkUpdate();
}

void Serializable::OnGetAttribute(const AttributeInfo& attr, Variant& dest) const
{
    // Check for accessor function mode
    if (attr.accessor_)
    {
        attr.accessor_->Get(this, dest);
        return;
    }

    // Get the source address
    assert(attr.ptr_);
    const void* src = attr.ptr_;

    switch (attr.type_)
    {
    case VariantType::Int:
        // If enum type, use the low 8 bits only
        if (attr.enumNames_)
            dest = *(reinterpret_cast<const unsigned char*>(src));
        else
            dest = *(reinterpret_cast<const int*>(src));
        break;

    case VariantType::Int64:
        dest = *(reinterpret_cast<const long long*>(src));
        break;

    case VariantType::Bool:
        dest = *(reinterpret_cast<const bool*>(src));
        break;

    case VariantType::Float:
        dest = *(reinterpret_cast<const float*>(src));
        break;

    case VariantType::Vector2:
        dest = *(reinterpret_cast<const Vector2*>(src));
        break;

    case VariantType::Vector3:
        dest = *(reinterpret_cast<const Vector3*>(src));
        break;

    case VariantType::Vector4:
        dest = *(reinterpret_cast<const Vector4*>(src));
        break;

    case VariantType::Quaternion:
        dest = *(reinterpret_cast<const Quaternion*>(src));
        break;

    case VariantType::Color:
        dest = *(reinterpret_cast<const Color*>(src));
        break;

    case VariantType::String:
        dest = *(reinterpret_cast<const ea::string*>(src));
        break;

    case VariantType::Buffer:
        dest = *(reinterpret_cast<const ea::vector<unsigned char>*>(src));
        break;

    case VariantType::ResourceRef:
        dest = *(reinterpret_cast<const ResourceRef*>(src));
        break;

    case VariantType::ResourceRefList:
        dest = *(reinterpret_cast<const ResourceRefList*>(src));
        break;

    case VariantType::VariantVector:
        dest = *(reinterpret_cast<const VariantVector*>(src));
        break;

    case VariantType::StringVector:
        dest = *(reinterpret_cast<const StringVector*>(src));
        break;

    case VariantType::VariantMap:
        dest = *(reinterpret_cast<const VariantMap*>(src));
        break;

    case VariantType::IntRect:
        dest = *(reinterpret_cast<const IntRect*>(src));
        break;

    case VariantType::IntVector2:
        dest = *(reinterpret_cast<const IntVector2*>(src));
        break;

    case VariantType::IntVector3:
        dest = *(reinterpret_cast<const IntVector3*>(src));
        break;

    case VariantType::Double:
        dest = *(reinterpret_cast<const double*>(src));
        break;

    default:
        URHO3D_LOGERROR("Unsupported attribute type for OnGetAttribute()");
        return;
    }
}

const ea::vector<AttributeInfo>* Serializable::GetAttributes() const
{
    return context_->GetAttributes(GetType());
}

const ea::vector<AttributeInfo>* Serializable::GetNetworkAttributes() const
{
    return networkState_ ? networkState_->attributes_ : context_->GetNetworkAttributes(GetType());
}

bool Serializable::Load(Deserializer& source)
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.ShouldLoad())
            continue;

        if (source.IsEof())
        {
            URHO3D_LOGERROR("Could not load " + GetTypeName() + ", stream not open or at end");
            return false;
        }

        Variant varValue = source.ReadVariant(attr.type_, context_);
        OnSetAttribute(attr, varValue);
    }

    return true;
}

bool Serializable::Save(Serializer& dest) const
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.ShouldSave())
            continue;

        OnGetAttribute(attr, value);

        if (!dest.WriteVariantData(value))
        {
            URHO3D_LOGERROR("Could not save " + GetTypeName() + ", writing to stream failed");
            return false;
        }
    }

    return true;
}

bool Serializable::LoadXML(const XMLElement& source)
{
    if (source.IsNull())
    {
        URHO3D_LOGERROR("Could not load " + GetTypeName() + ", null source element");
        return false;
    }

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    XMLElement attrElem = source.GetChild("attribute");
    unsigned startIndex = 0;

    while (attrElem)
    {
        ea::string name = attrElem.GetAttribute("name");
        unsigned i = startIndex;
        unsigned attempts = attributes->size();

        while (attempts)
        {
            const AttributeInfo& attr = attributes->at(i);
            if (attr.ShouldLoad() && !attr.name_.compare(name))
            {
                Variant varValue;

                // If enums specified, do enum lookup and int assignment. Otherwise assign the variant directly
                if (attr.enumNames_ && attr.type_ == VariantType::Int)
                {
                    ea::string value = attrElem.GetAttribute("value");
                    bool enumFound = false;
                    int enumValue = 0;
                    const char** enumPtr = attr.enumNames_;
                    while (*enumPtr)
                    {
                        if (!value.comparei(*enumPtr))
                        {
                            enumFound = true;
                            break;
                        }
                        ++enumPtr;
                        ++enumValue;
                    }
                    if (enumFound)
                        varValue = enumValue;
                    else
                        URHO3D_LOGWARNING("Unknown enum value " + value + " in attribute " + attr.name_);
                }
                else
                    varValue = attrElem.GetVariantValue(attr.type_, context_);

                if (!varValue.IsEmpty())
                    OnSetAttribute(attr, varValue);

                startIndex = (i + 1) % attributes->size();
                break;
            }
            else
            {
                i = (i + 1) % attributes->size();
                --attempts;
            }
        }

        if (!attempts)
            URHO3D_LOGWARNING("Unknown attribute " + name + " in XML data");

        attrElem = attrElem.GetNext("attribute");
    }

    return true;
}

bool Serializable::LoadJSON(const JSONValue& source)
{
    if (source.IsNull())
    {
        URHO3D_LOGERROR("Could not load " + GetTypeName() + ", null JSON source element");
        return false;
    }

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    // Get attributes value
    JSONValue attributesValue = source.Get("attributes");
    if (attributesValue.IsNull())
        return true;
    // Warn if the attributes value isn't an object
    if (!attributesValue.IsObject())
    {
        URHO3D_LOGWARNING("'attributes' object is present in " + GetTypeName() + " but is not a JSON object; skipping load");
        return true;
    }

    const JSONObject& attributesObject = attributesValue.GetObject();

    for (const AttributeInfo& attr : *attributes)
    {
        if (attr.ShouldLoad())
        {
            const JSONValue& value = attributesValue[attr.name_];
            if (value.GetValueType() == JSON_NULL)
                continue;

            Variant varValue;
            // If enums specified, do enum lookup ad int assignment. Otherwise assign variant directly
            if (attr.enumNames_ && attr.type_ == VariantType::Int)
            {
                const ea::string& valueStr = value.GetString();
                bool enumFound = false;
                int enumValue = 0;
                const char** enumPtr = attr.enumNames_;
                while (*enumPtr)
                {
                    if (!valueStr.comparei(*enumPtr))
                    {
                        enumFound = true;
                        break;
                    }
                    ++enumPtr;
                    ++enumValue;
                }
                if (enumFound)
                    varValue = enumValue;
                else
                    URHO3D_LOGWARNING("Unknown enum value " + valueStr + " in attribute " + attr.name_);
            }
            else
                varValue = value.GetVariantValue(attr.type_, context_);

            if (!varValue.IsEmpty())
                OnSetAttribute(attr, varValue);
        }
    }

    // Report missing attributes.
    for (const auto& pair : attributesObject)
    {
        bool found = false;
        for (int i = 0; i < attributes->size() && !found; i++)
            found |= attributes->at(i).name_ == pair.first;
        if (!found)
            URHO3D_LOGWARNING("Unknown attribute {} in JSON data", pair.first);
    }

    return true;
}

bool Serializable::SaveXML(XMLElement& dest) const
{
    if (dest.IsNull())
    {
        URHO3D_LOGERROR("Could not save " + GetTypeName() + ", null destination element");
        return false;
    }

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    Variant value;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.ShouldSave())
            continue;

        OnGetAttribute(attr, value);
        Variant defaultValue(GetAttributeDefault(i));

        // In XML serialization default values can be skipped. This will make the file easier to read or edit manually
        if (value == defaultValue && !SaveDefaultAttributes(attr))
            continue;

        XMLElement attrElem = dest.CreateChild("attribute");
        attrElem.SetAttribute("name", attr.name_);
        // If enums specified, set as an enum string. Otherwise set directly as a Variant
        if (attr.enumNames_ && attr.type_ == VariantType::Int)
        {
            int enumValue = value.GetInt();
            attrElem.SetAttribute("value", attr.enumNames_[enumValue]);
        }
        else
            attrElem.SetVariantValue(value);
    }

    return true;
}

bool Serializable::SaveJSON(JSONValue& dest) const
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    Variant value;
    JSONValue attributesValue;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.ShouldSave())
            continue;

        OnGetAttribute(attr, value);
        Variant defaultValue(GetAttributeDefault(i));

        // In JSON serialization default values can be skipped. This will make the file easier to read or edit manually
        if (value == defaultValue && !SaveDefaultAttributes(attr))
            continue;

        JSONValue attrVal;
        // If enums specified, set as an enum string. Otherwise set directly as a Variant
        if (attr.enumNames_ && attr.type_ == VariantType::Int)
        {
            int enumValue = value.GetInt();
            attrVal = attr.enumNames_[enumValue];
        }
        else
            attrVal.SetVariantValue(value, context_);

        attributesValue.Set(attr.name_, attrVal);
    }
    dest.Set("attributes", attributesValue);

    return true;
}

bool Serializable::Load(const ea::string& resourceName)
{
    SharedPtr<File> file(GetSubsystem<ResourceCache>()->GetFile(resourceName, false));
    if (file)
        return Load(*file);
    return false;
}

bool Serializable::LoadXML(const ea::string& resourceName)
{
    SharedPtr<XMLFile> file(GetSubsystem<ResourceCache>()->GetResource<XMLFile>(resourceName, false));
    if (file)
        return LoadXML(file->GetRoot());
    return false;
}

bool Serializable::LoadJSON(const ea::string& resourceName)
{
    SharedPtr<JSONFile> file(GetSubsystem<ResourceCache>()->GetResource<JSONFile>(resourceName, false));
    if (file)
        return LoadJSON(file->GetRoot());
    return false;
}

bool Serializable::LoadFile(const ea::string& resourceName)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    // Router may redirect to different file.
    ea::string realResourceName = resourceName;
    cache->RouteResourceName(realResourceName, RESOURCE_CHECKEXISTS);
    ea::string extension = GetExtension(realResourceName);

    if (extension == ".xml")
        return LoadXML(realResourceName);
    if (extension == ".json")
        return LoadJSON(realResourceName);
    return Load(realResourceName);
}

bool Serializable::Serialize(Archive& archive)
{
    if (ArchiveBlock block = archive.OpenUnorderedBlock("serializable"))
        return Serialize(archive, block);
    return false;
}

bool Serializable::Serialize(Archive& archive, ArchiveBlock& block)
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return true;

    // Prepare for serialization
    ea::fixed_vector<Variant, MAX_STACK_ATTRIBUTE_COUNT> attributeValues;
    const unsigned numAttributes = attributes->size();
    const bool saveDefaults = !archive.IsHumanReadable();

    // Caclculate number of attributes to write
    unsigned numAttributesToWrite = 0;
    if (!archive.IsInput())
    {
        if (!saveDefaults)
            attributeValues.resize(numAttributes);

        for (unsigned index = 0; index < numAttributes; ++index)
        {
            const AttributeInfo& attr = (*attributes)[index];

            if (!attr.ShouldSave())
                continue;

            // Code is optimized for binary archives that always save defaults
            ++numAttributesToWrite;

            // Check the value
            if (!saveDefaults)
            {
                OnGetAttribute(attr, attributeValues[index]);

                if (!SaveDefaultAttributes(attr))
                {
                    const Variant defaultValue = GetAttributeDefault(index);
                    if (attributeValues[index] == defaultValue)
                    {
                        // Skip attribute with default value
                        attributeValues[index] = Variant::EMPTY;
                        --numAttributesToWrite;
                    }
                }
            }
        }
    }

    // Serialize attributes
    if (auto block = archive.OpenMapBlock("attributes", numAttributesToWrite))
    {
        if (archive.IsInput())
        {
            ea::fixed_vector<StringHash, MAX_STACK_ATTRIBUTE_COUNT> attributeNames;

            // Try to load attributes sequentially
            unsigned nextAttributeIndex = 0;
            unsigned numSkippedAttributes = 0;
            for (unsigned i = 0; i < block.GetSizeHint(); ++i)
            {
                // Skip attributes that are not serialized
                while (nextAttributeIndex < numAttributes && !(*attributes)[nextAttributeIndex].ShouldLoad())
                    ++nextAttributeIndex;

                // Read attribute name hash
                StringHash attrNameHash;
                if (!SerializeStringHashKey(archive, attrNameHash, EMPTY_STRING))
                {
                    URHO3D_LOGERROR("Could not load " + GetTypeName() + ", failed to read attribute name");
                    return false;
                }

                // If attribute is expected, just read it
                if (nextAttributeIndex < numAttributes && (*attributes)[nextAttributeIndex].nameHash_ == attrNameHash)
                {
                    const AttributeInfo& attr = (*attributes)[nextAttributeIndex];
                    Variant value;
                    if (!LoadAttribute(archive, attr, value))
                    {
                        URHO3D_LOGERROR("Could not load " + GetTypeName() + ", failed to read attribute " + attr.name_);
                        return false;
                    }

                    OnSetAttribute(attr, value);
                    ++nextAttributeIndex;
                    continue;
                }

                // Lazy initialize attribute names
                if (attributeNames.empty())
                {
                    for (unsigned j = 0; j < numAttributes; ++j)
                        attributeNames.push_back(attributes->at(j).nameHash_);
                }

                // Try to find the attribute
                const unsigned attributeIndex = attributeNames.index_of(attrNameHash);

                // Skip if not found or invalid
                if (attributeIndex >= numAttributes || !((*attributes)[attributeIndex].mode_ & AttributeMode::File))
                {
                    ++numSkippedAttributes;
                    continue;
                }

                // Lazy allocate deferred array
                if (attributeValues.empty())
                    attributeValues.resize(numAttributes);

                // Load attribute into deferred array
                const AttributeInfo& attr = (*attributes)[attributeIndex];
                if (!LoadAttribute(archive, attr, attributeValues[attributeIndex]))
                {
                    URHO3D_LOGERROR("Could not load " + GetTypeName() + ", failed to read attribute " + attr.name_);
                    return false;
                }
            }

            // Apply deferred attributes
            if (!attributeValues.empty())
            {
                for (unsigned attributeIndex = 0; attributeIndex < numAttributes; ++attributeIndex)
                {
                    const Variant& value = attributeValues[attributeIndex];
                    if (!value.IsEmpty())
                        OnSetAttribute((*attributes)[attributeIndex], value);
                }
            }

            // Warn about skipped attributes
            if (numSkippedAttributes > 0)
            {
                URHO3D_LOGWARNING(ea::to_string(numSkippedAttributes) + " atributes were skipped while loading " + GetTypeName());
            }
        }
        else
        {
            if (saveDefaults)
            {
                // Get and save attributes
                Variant value;
                for (const AttributeInfo& attr : *attributes)
                {
                    if (!attr.ShouldSave())
                        continue;

                    OnGetAttribute(attr, value);

                    if (!SaveAttributeWithName(archive, attr, value))
                    {
                        URHO3D_LOGERROR("Could not save " + GetTypeName() + ", failed to write attribute " + attr.name_);
                        return false;
                    }
                }
            }
            else
            {
                // Just save attributes because they are already acquired
                for (unsigned attributeIndex = 0; attributeIndex < numAttributes; ++attributeIndex)
                {
                    const AttributeInfo& attr = (*attributes)[attributeIndex];
                    const Variant& value = attributeValues[attributeIndex];

                    if (value.IsEmpty())
                        continue;

                    if (!SaveAttributeWithName(archive, attr, value))
                    {
                        URHO3D_LOGERROR("Could not save " + GetTypeName() + ", failed to write attribute " + attr.name_);
                        return false;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

bool Serializable::SetAttribute(unsigned index, const Variant& value)
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return false;
    }

    const AttributeInfo& attr = attributes->at(index);

    // Check that the new value's type matches the attribute type
    if (value.GetType() == attr.type_)
    {
        OnSetAttribute(attr, value);
        return true;
    }
    else
    {
        URHO3D_LOGERROR("Could not set attribute " + attr.name_ + ": expected type " + Variant::GetTypeName(attr.type_) +
                 " but got " + value.GetTypeName());
        return false;
    }
}

bool Serializable::SetAttribute(const ea::string& name, const Variant& value)
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return false;
    }

    for (auto i = attributes->begin(); i != attributes->end(); ++i)
    {
        if (!i->name_.compare(name))
        {
            // Check that the new value's type matches the attribute type
            if (value.GetType() == i->type_)
            {
                OnSetAttribute(*i, value);
                return true;
            }
            else
            {
                URHO3D_LOGERROR("Could not set attribute " + i->name_ + ": expected type " + Variant::GetTypeName(i->type_)
                         + " but got " + value.GetTypeName());
                return false;
            }
        }
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return false;
}

void Serializable::ResetToDefault()
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
        return;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.mode_ & (AttributeMode::NoEdit | AttributeMode::NodeID | AttributeMode::ComponentID | AttributeMode::NodeIDVector))
            continue;

        Variant defaultValue = GetInstanceDefault(attr.name_);
        if (defaultValue.IsEmpty())
            defaultValue = attr.defaultValue_;

        OnSetAttribute(attr, defaultValue);
    }
}

void Serializable::RemoveInstanceDefault()
{
    instanceDefaultValues_.reset();
}

void Serializable::SetTemporary(bool enable)
{
    if (enable != temporary_)
    {
        temporary_ = enable;

        using namespace TemporaryChanged;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_SERIALIZABLE] = this;

        SendEvent(E_TEMPORARYCHANGED, eventData);
    }
}

void Serializable::SetInterceptNetworkUpdate(const ea::string& attributeName, bool enable)
{
    AllocateNetworkState();

    const ea::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.name_.compare(attributeName))
        {
            if (enable)
                networkState_->interceptMask_ |= 1ULL << i;
            else
                networkState_->interceptMask_ &= ~(1ULL << i);
            break;
        }
    }
}

void Serializable::AllocateNetworkState()
{
    if (networkState_)
        return;

    const ea::vector<AttributeInfo>* networkAttributes = GetNetworkAttributes();
    networkState_ = ea::make_unique<NetworkState>();
    networkState_->attributes_ = networkAttributes;

    if (!networkAttributes)
        return;

    unsigned numAttributes = networkAttributes->size();

    if (networkState_->currentValues_.size() != numAttributes)
    {
        networkState_->currentValues_.resize(numAttributes);
        networkState_->previousValues_.resize(numAttributes);

        // Copy the default attribute values to the previous state as a starting point
        for (unsigned i = 0; i < numAttributes; ++i)
            networkState_->previousValues_[i] = networkAttributes->at(i).defaultValue_;
    }
}

void Serializable::WriteInitialDeltaUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (!networkState_)
    {
        URHO3D_LOGERROR("WriteInitialDeltaUpdate called without allocated NetworkState");
        return;
    }

    const ea::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;

    // Compare against defaults
    for (unsigned i = 0; i < numAttributes; ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (networkState_->currentValues_[i] != attr.defaultValue_)
            attributeBits.Set(i);
    }

    // First write the change bitfield, then attribute data for non-default attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3u);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteDeltaUpdate(Serializer& dest, const DirtyBits& attributeBits, unsigned char timeStamp)
{
    if (!networkState_)
    {
        URHO3D_LOGERROR("WriteDeltaUpdate called without allocated NetworkState");
        return;
    }

    const ea::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();

    // First write the change bitfield, then attribute data for changed attributes
    // Note: the attribute bits should not contain LATESTDATA attributes
    dest.WriteUByte(timeStamp);
    dest.Write(attributeBits.data_, (numAttributes + 7) >> 3u);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributeBits.IsSet(i))
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

void Serializable::WriteLatestDataUpdate(Serializer& dest, unsigned char timeStamp)
{
    if (!networkState_)
    {
        URHO3D_LOGERROR("WriteLatestDataUpdate called without allocated NetworkState");
        return;
    }

    const ea::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (!attributes)
        return;

    unsigned numAttributes = attributes->size();

    dest.WriteUByte(timeStamp);

    for (unsigned i = 0; i < numAttributes; ++i)
    {
        if (attributes->at(i).mode_ & AttributeMode::LatestData)
            dest.WriteVariantData(networkState_->currentValues_[i]);
    }
}

bool Serializable::ReadDeltaUpdate(Deserializer& source)
{
    const ea::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned numAttributes = attributes->size();
    DirtyBits attributeBits;
    bool changed = false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();
    source.Read(attributeBits.data_, (numAttributes + 7) >> 3u);

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        if (attributeBits.IsSet(i))
        {
            const AttributeInfo& attr = attributes->at(i);
            if (!(interceptMask & (1ULL << i)))
            {
                OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
            }
            else
            {
                using namespace InterceptNetworkUpdate;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_SERIALIZABLE] = this;
                eventData[P_TIMESTAMP] = (unsigned)timeStamp;
                eventData[P_INDEX] = RemapAttributeIndex(GetAttributes(), attr, i);
                eventData[P_NAME] = attr.name_;
                eventData[P_VALUE] = source.ReadVariant(attr.type_);
                SendEvent(E_INTERCEPTNETWORKUPDATE, eventData);
            }
        }
    }

    return changed;
}

bool Serializable::ReadLatestDataUpdate(Deserializer& source)
{
    const ea::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned numAttributes = attributes->size();
    bool changed = false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;
    unsigned char timeStamp = source.ReadUByte();

    for (unsigned i = 0; i < numAttributes && !source.IsEof(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (attr.mode_ & AttributeMode::LatestData)
        {
            if (!(interceptMask & (1ULL << i)))
            {
                OnSetAttribute(attr, source.ReadVariant(attr.type_));
                changed = true;
            }
            else
            {
                using namespace InterceptNetworkUpdate;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_SERIALIZABLE] = this;
                eventData[P_TIMESTAMP] = (unsigned)timeStamp;
                eventData[P_INDEX] = RemapAttributeIndex(GetAttributes(), attr, i);
                eventData[P_NAME] = attr.name_;
                eventData[P_VALUE] = source.ReadVariant(attr.type_);
                SendEvent(E_INTERCEPTNETWORKUPDATE, eventData);
            }
        }
    }

    return changed;
}

Variant Serializable::GetAttribute(unsigned index) const
{
    Variant ret;

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return ret;
    }

    OnGetAttribute(attributes->at(index), ret);
    return ret;
}

Variant Serializable::GetAttribute(const ea::string& name) const
{
    Variant ret;

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return ret;
    }

    for (auto i = attributes->begin(); i != attributes->end(); ++i)
    {
        if (!i->name_.compare(name))
        {
            OnGetAttribute(*i, ret);
            return ret;
        }
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return ret;
}

Variant Serializable::GetAttributeDefault(unsigned index) const
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }
    if (index >= attributes->size())
    {
        URHO3D_LOGERROR("Attribute index out of bounds");
        return Variant::EMPTY;
    }

    AttributeInfo attr = attributes->at(index);
    Variant defaultValue = GetInstanceDefault(attr.name_);
    return defaultValue.IsEmpty() ? attr.defaultValue_ : defaultValue;
}

Variant Serializable::GetAttributeDefault(const ea::string& name) const
{
    Variant defaultValue = GetInstanceDefault(name);
    if (!defaultValue.IsEmpty())
        return defaultValue;

    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    if (!attributes)
    {
        URHO3D_LOGERROR(GetTypeName() + " has no attributes");
        return Variant::EMPTY;
    }

    for (auto i = attributes->begin(); i != attributes->end(); ++i)
    {
        if (!i->name_.compare(name))
            return i->defaultValue_;
    }

    URHO3D_LOGERROR("Could not find attribute " + name + " in " + GetTypeName());
    return Variant::EMPTY;
}

unsigned Serializable::GetNumAttributes() const
{
    const ea::vector<AttributeInfo>* attributes = GetAttributes();
    return attributes ? attributes->size() : 0;
}

unsigned Serializable::GetNumNetworkAttributes() const
{
    const ea::vector<AttributeInfo>* attributes = networkState_ ? networkState_->attributes_ :
        context_->GetNetworkAttributes(GetType());
    return attributes ? attributes->size() : 0;
}

bool Serializable::GetInterceptNetworkUpdate(const ea::string& attributeName) const
{
    const ea::vector<AttributeInfo>* attributes = GetNetworkAttributes();
    if (!attributes)
        return false;

    unsigned long long interceptMask = networkState_ ? networkState_->interceptMask_ : 0;

    for (unsigned i = 0; i < attributes->size(); ++i)
    {
        const AttributeInfo& attr = attributes->at(i);
        if (!attr.name_.compare(attributeName))
            return interceptMask & (1ULL << i) ? true : false;
    }

    return false;
}

void Serializable::SetInstanceDefault(const ea::string& name, const Variant& defaultValue)
{
    // Allocate the instance level default value
    if (!instanceDefaultValues_)
        instanceDefaultValues_ = ea::make_unique<VariantMap>();
    instanceDefaultValues_->operator [](name) = defaultValue;
}

Variant Serializable::GetInstanceDefault(const ea::string& name) const
{
    if (instanceDefaultValues_)
    {
        auto i = instanceDefaultValues_->find(name);
        if (i != instanceDefaultValues_->end())
            return i->second;
    }

    return Variant::EMPTY;
}

}
