//
// Copyright (c) 2008-2020 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:addmember
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
#include "../Core/StringUtils.h"
#include "../IO/Log.h"
#include "../Scene/Serializable.h"
#include "../Resource/JSONValue.h"

#include "../DebugNew.h"

namespace Urho3D
{

static const char* valueTypeNames[] =
{
    "Null",
    "Bool",
    "Number",
    "String",
    "Array",
    "Object",
    nullptr
};

static const char* numberTypeNames[] =
{
    "NaN",
    "Int",
    "Unsigned",
    "Real",
    nullptr
};

const JSONValue JSONValue::EMPTY;
const JSONArray JSONValue::emptyArray { };
const JSONObject JSONValue::emptyObject;

JSONValue& JSONValue::operator =(bool rhs)
{
    SetType(JSON_BOOL);
    boolValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(int rhs)
{
    SetType(JSON_NUMBER, JSONNT_INT);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(unsigned rhs)
{
    SetType(JSON_NUMBER, JSONNT_UINT);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(float rhs)
{
    SetType(JSON_NUMBER, JSONNT_FLOAT_DOUBLE);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(double rhs)
{
    SetType(JSON_NUMBER, JSONNT_FLOAT_DOUBLE);
    numberValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const ea::string& rhs)
{
    SetType(JSON_STRING);
    *stringValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const char* rhs)
{
    SetType(JSON_STRING);
    *stringValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONArray& rhs)
{
    SetType(JSON_ARRAY);
    *arrayValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONObject& rhs)
{
    SetType(JSON_OBJECT);
    *objectValue_ = rhs;

    return *this;
}

JSONValue& JSONValue::operator =(const JSONValue& rhs)
{
    if (this == &rhs)
        return *this;

    SetType(rhs.GetValueType(), rhs.GetNumberType());

    switch (GetValueType())
    {
    case JSON_BOOL:
        boolValue_ = rhs.boolValue_;
        break;

    case JSON_NUMBER:
        numberValue_ = rhs.numberValue_;
        break;

    case JSON_STRING:
        *stringValue_ = *rhs.stringValue_;
        break;

    case JSON_ARRAY:
        *arrayValue_ = *rhs.arrayValue_;
        break;

    case JSON_OBJECT:
        *objectValue_ = *rhs.objectValue_;

    default:
        break;
    }

    return *this;
}

JSONValue& JSONValue::operator=(JSONValue && rhs)
{
    assert(this != &rhs);

    SetType(rhs.GetValueType(), rhs.GetNumberType());

    switch (GetValueType())
    {
    case JSON_BOOL:
        boolValue_ = rhs.boolValue_;
        break;

    case JSON_NUMBER:
        numberValue_ = rhs.numberValue_;
        break;

    case JSON_STRING:
        *stringValue_ = std::move(*rhs.stringValue_);
        break;

    case JSON_ARRAY:
        *arrayValue_ = std::move(*rhs.arrayValue_);
        break;

    case JSON_OBJECT:
        *objectValue_ = std::move(*rhs.objectValue_);

    default:
        break;
    }

    return *this;
}

bool JSONValue::operator ==(const JSONValue& rhs) const
{
    // Value type without number type is checked. JSON does not make a distinction between number types. It is possible
    // that we serialized number (for example `1`) as unsigned integer. It will not necessarily be unserialized as same
    // number type. Number value equality check below will make sure numbers match anyway.
    if (GetValueType() != rhs.GetValueType())
        return false;

    switch (GetValueType())
    {
    case JSON_BOOL:
        return boolValue_ == rhs.boolValue_;

    case JSON_NUMBER:
        return numberValue_ == rhs.numberValue_;

    case JSON_STRING:
        return *stringValue_ == *rhs.stringValue_;

    case JSON_ARRAY:
        return *arrayValue_ == *rhs.arrayValue_;

    case JSON_OBJECT:
        return *objectValue_ == *rhs.objectValue_;

    default:
        break;
    }

    return false;
}

bool JSONValue::operator !=(const JSONValue& rhs) const
{
    return !operator ==(rhs);
}

JSONValueType JSONValue::GetValueType() const
{
    return (JSONValueType)(type_ >> 16u);
}

JSONNumberType JSONValue::GetNumberType() const
{
    return (JSONNumberType)(type_ & 0xffffu);
}

ea::string JSONValue::GetValueTypeName() const
{
    return GetValueTypeName(GetValueType());
}

ea::string JSONValue::GetNumberTypeName() const
{
    return GetNumberTypeName(GetNumberType());
}

JSONValue& JSONValue::operator [](unsigned index)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    return (*arrayValue_)[index];
}

const JSONValue& JSONValue::operator [](unsigned index) const
{
    if (GetValueType() != JSON_ARRAY)
        return EMPTY;

    return (*arrayValue_)[index];
}

void JSONValue::Push(JSONValue value)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    arrayValue_->push_back(std::move(value));
}

void JSONValue::Pop()
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->pop_back();
}

void JSONValue::Insert(unsigned pos, JSONValue value)
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->insert(arrayValue_->begin() + pos, std::move(value));
}

void JSONValue::Erase(unsigned pos, unsigned length)
{
    if (GetValueType() != JSON_ARRAY)
        return;

    arrayValue_->erase(arrayValue_->begin() + pos, arrayValue_->begin() + pos + length);
}

void JSONValue::Resize(unsigned newSize)
{
    // Convert to array type
    SetType(JSON_ARRAY);

    arrayValue_->resize(newSize);
}

unsigned JSONValue::Size() const
{
    if (GetValueType() == JSON_ARRAY)
        return arrayValue_->size();
    else if (GetValueType() == JSON_OBJECT)
        return objectValue_->size();

    return 0;
}

JSONValue& JSONValue::operator [](const ea::string& key)
{
    // Convert to object type
    SetType(JSON_OBJECT);

    return (*objectValue_)[key];
}

const JSONValue& JSONValue::operator [](const ea::string& key) const
{
    if (GetValueType() != JSON_OBJECT)
        return EMPTY;

    return (*objectValue_)[key];
}

void JSONValue::Set(const ea::string& key, JSONValue value)
{
    // Convert to object type
    SetType(JSON_OBJECT);

    (*objectValue_)[key] = std::move(value);
}

const JSONValue& JSONValue::Get(const ea::string& key) const
{
    if (GetValueType() != JSON_OBJECT)
        return EMPTY;

    auto i = objectValue_->find(key);
    if (i == objectValue_->end())
        return EMPTY;

    return i->second;
}

const JSONValue& JSONValue::Get(int index) const
{
    if (GetValueType() != JSON_ARRAY)
        return EMPTY;

    if (index < 0 || index >= arrayValue_->size())
        return EMPTY;

    return arrayValue_->at(index);
}

bool JSONValue::Erase(const ea::string& key)
{
    if (GetValueType() != JSON_OBJECT)
        return false;

    return objectValue_->erase(key);
}

bool JSONValue::Contains(const ea::string& key) const
{
    if  (GetValueType() != JSON_OBJECT)
        return false;

    return objectValue_->contains(key);
}

void JSONValue::Clear()
{
    if (GetValueType() == JSON_ARRAY)
        arrayValue_->clear();
    else if (GetValueType() == JSON_OBJECT)
        objectValue_->clear();
}

void JSONValue::SetType(JSONValueType valueType, JSONNumberType numberType)
{
    int type = valueType << 16u | numberType;
    if (type == type_)
        return;

    switch (GetValueType())
    {
    case JSON_STRING:
        delete stringValue_;
        break;

    case JSON_ARRAY:
        delete arrayValue_;
        break;

    case JSON_OBJECT:
        delete objectValue_;
        break;

    default:
        break;
    }

    type_ = type;

    switch (GetValueType())
    {
    case JSON_STRING:
        stringValue_ = new ea::string();
        break;

    case JSON_ARRAY:
        arrayValue_ = new JSONArray();
        break;

    case JSON_OBJECT:
        objectValue_ = new JSONObject();
        break;

    default:
        break;
    }
}

void JSONValue::SetVariant(const Variant& variant, Context* context)
{
    if (!IsNull())
    {
        URHO3D_LOGWARNING("JsonValue is not null");
    }

    (*this)["type"] = variant.GetTypeName();
    (*this)["value"].SetVariantValue(variant, context);
}

Variant JSONValue::GetVariant() const
{
    VariantType type = Variant::GetTypeFromName((*this)["type"].GetString());
    return (*this)["value"].GetVariantValue(type);
}

void JSONValue::SetVariantValue(const Variant& variant, Context* context)
{
    if (!IsNull())
    {
        URHO3D_LOGWARNING("JsonValue is not null");
    }

    switch (variant.GetType())
    {
    case VariantType::Bool:
        *this = variant.GetBool();
        return;

    case VariantType::Int:
        *this = variant.GetInt();
        return;

    case VariantType::Float:
        *this = variant.GetFloat();
        return;

    case VariantType::Double:
        *this = variant.GetDouble();
        return;

    case VariantType::String:
        *this = variant.GetString();
        return;

    case VariantType::VariantVector:
        SetVariantVector(variant.GetVariantVector(), context);
        return;

    case VariantType::VariantMap:
        SetVariantMap(variant.GetVariantMap(), context);
        return;

    case VariantType::ResourceRef:
        {
            if (!context)
            {
                URHO3D_LOGERROR("Context must not be null for ResourceRef");
                return;
            }

            const ResourceRef& ref = variant.GetResourceRef();
            *this = ea::string(context->GetTypeName(ref.type_)) + ";" + ref.name_;
        }
        return;

    case VariantType::ResourceRefList:
        {
            if (!context)
            {
                URHO3D_LOGERROR("Context must not be null for ResourceRefList");
                return;
            }

            const ResourceRefList& refList = variant.GetResourceRefList();
            ea::string str(context->GetTypeName(refList.type_));
            for (unsigned i = 0; i < refList.names_.size(); ++i)
            {
                str += ";";
                str += refList.names_[i];
            }
            *this = str;
        }
        return;

    case VariantType::StringVector:
        {
            const StringVector& vector = variant.GetStringVector();
            Resize(vector.size());
            for (unsigned i = 0; i < vector.size(); ++i)
                (*this)[i] = vector[i];
        }
        return;

    case VariantType::Custom:
        {
            if (const Serializable* object = variant.GetCustom<SharedPtr<Serializable>>())
            {
                JSONValue value;
                if (object->SaveJSON(value))
                {
                    Set("type", object->GetTypeName());
                    Set("value", value);
                }
                else
                    SetType(JSON_NULL);
            }
            else
            {
                SetType(JSON_NULL);
                URHO3D_LOGERROR("Serialization of objects other than SharedPtr<Serializable> is not supported.");
            }
        }
        return;

    default:
        *this = variant.ToString();
    }
}

Variant JSONValue::GetVariantValue(VariantType type, Context* context) const
{
    Variant variant;
    switch (type)
    {
    case VariantType::Bool:
        variant = GetBool();
        break;

    case VariantType::Int:
        variant = GetInt();
        break;

    case VariantType::Float:
        variant = GetFloat();
        break;

    case VariantType::Double:
        variant = GetDouble();
        break;

    case VariantType::String:
        variant = GetString();
        break;

    case VariantType::VariantVector:
        variant = GetVariantVector();
        break;

    case VariantType::VariantMap:
        variant = GetVariantMap();
        break;

    case VariantType::ResourceRef:
        {
            ResourceRef ref;
            ea::vector<ea::string> values = GetString().split(';');
            if (values.size() == 2)
            {
                ref.type_ = values[0];
                ref.name_ = values[1];
            }
            variant = ref;
        }
        break;

    case VariantType::ResourceRefList:
        {
            ResourceRefList refList;
            ea::vector<ea::string> values = GetString().split(';', true);
            if (values.size() >= 1)
            {
                refList.type_ = values[0];
                refList.names_.resize(values.size() - 1);
                for (unsigned i = 1; i < values.size(); ++i)
                    refList.names_[i - 1] = values[i];
            }
            variant = refList;
        }
        break;

    case VariantType::StringVector:
        {
            StringVector vector;
            for (unsigned i = 0; i < Size(); ++i)
                vector.push_back((*this)[i].GetString());
            variant = vector;
        }
        break;

    case VariantType::Custom:
        {
            if (!context)
            {
                URHO3D_LOGERROR("Context must not be null for SharedPtr<Serializable>");
                break;
            }

            if (GetValueType() == JSON_NULL)
                return Variant::EMPTY;

            if (GetValueType() != JSON_OBJECT)
            {
                URHO3D_LOGERROR("SharedPtr<Serializable> expects json object");
                break;
            }

            const ea::string& typeName = (*this)["type"].GetString();
            if (!typeName.empty())
            {
                SharedPtr<Serializable> object;
                object.StaticCast(context->CreateObject(typeName));

                if (object.NotNull())
                {
                    // Restore proper refcount.
                    if (object->LoadJSON((*this)["value"]))
                        variant.SetCustom(object);
                    else
                        URHO3D_LOGERROR("Deserialization of '{}' failed", typeName);
                }
                else
                    URHO3D_LOGERROR("Creation of type '{}' failed because it has no factory registered", typeName);
            }
            else
                URHO3D_LOGERROR("Malformed json input: 'type' is required when deserializing an object");
        }
        break;

    default:
        variant.FromString(type, GetString());
    }

    return variant;
}

void JSONValue::SetVariantMap(const VariantMap& variantMap, Context* context)
{
    SetType(JSON_OBJECT);
    for (auto i = variantMap.begin(); i != variantMap.end(); ++i)
        (*this)[i->first.ToString()].SetVariant(i->second);
}

VariantMap JSONValue::GetVariantMap() const
{
    VariantMap variantMap;
    if (!IsObject())
    {
        URHO3D_LOGERROR("JSONValue is not a object");
        return variantMap;
    }

    for (const auto& i : *this)
    {
        /// \todo Ideally this should allow any strings, but for now the convention is that the keys need to be hexadecimal StringHashes
        StringHash key(ToUInt(i.first, 16));
        Variant variant = i.second.GetVariant();
        variantMap[key] = variant;
    }

    return variantMap;
}

void JSONValue::SetVariantVector(const VariantVector& variantVector, Context* context)
{
    SetType(JSON_ARRAY);
    arrayValue_->reserve(variantVector.size());
    for (unsigned i = 0; i < variantVector.size(); ++i)
    {
        JSONValue val;
        val.SetVariant(variantVector[i], context);
        arrayValue_->push_back(val);
    }
}

VariantVector JSONValue::GetVariantVector() const
{
    VariantVector variantVector;
    if (!IsArray())
    {
        URHO3D_LOGERROR("JSONValue is not a array");
        return variantVector;
    }

    for (unsigned i = 0; i < Size(); ++i)
    {
        Variant variant = (*this)[i].GetVariant();
        variantVector.push_back(variant);
    }

    return variantVector;
}

ea::string JSONValue::GetValueTypeName(JSONValueType type)
{
    return valueTypeNames[type];
}

ea::string JSONValue::GetNumberTypeName(JSONNumberType type)
{
    return numberTypeNames[type];
}

JSONValueType JSONValue::GetValueTypeFromName(const ea::string& typeName)
{
    return GetValueTypeFromName(typeName.c_str());
}

JSONValueType JSONValue::GetValueTypeFromName(const char* typeName)
{
    return (JSONValueType)GetStringListIndex(typeName, valueTypeNames, JSON_NULL);
}

JSONNumberType JSONValue::GetNumberTypeFromName(const ea::string& typeName)
{
    return GetNumberTypeFromName(typeName.c_str());
}

JSONNumberType JSONValue::GetNumberTypeFromName(const char* typeName)
{
    return (JSONNumberType)GetStringListIndex(typeName, numberTypeNames, JSONNT_NAN);
}

JSONObjectIterator begin(JSONValue& value)
{
    // Convert to object type.
    value.SetType(JSON_OBJECT);

    return value.objectValue_->begin();
}

ConstJSONObjectIterator begin(const JSONValue& value)
{
    if (value.GetValueType() != JSON_OBJECT)
        return JSONValue::emptyObject.begin();

    return value.objectValue_->begin();
}

JSONObjectIterator end(JSONValue& value)
{
    // Convert to object type.
    value.SetType(JSON_OBJECT);

    return value.objectValue_->end();
}

ConstJSONObjectIterator end(const JSONValue& value)
{
    if (value.GetValueType() != JSON_OBJECT)
        return JSONValue::emptyObject.end();

    return value.objectValue_->end();
}

}
