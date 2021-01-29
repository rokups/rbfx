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
#include "../IO/Log.h"
#include "../Resource/XMLFile.h"
#include "../Scene/Serializable.h"

#include <PugiXml/pugixml.hpp>

#include "../DebugNew.h"

namespace Urho3D
{

const XMLElement XMLElement::EMPTY;

XMLElement::XMLElement() :
    node_(nullptr),
    xpathResultSet_(nullptr),
    xpathNode_(nullptr),
    xpathResultIndex_(0)
{
}

XMLElement::XMLElement(XMLFile* file, pugi::xml_node_struct* node) :
    file_(file),
    node_(node),
    xpathResultSet_(nullptr),
    xpathNode_(nullptr),
    xpathResultIndex_(0)
{
}

XMLElement::XMLElement(XMLFile* file, const XPathResultSet* resultSet, const pugi::xpath_node* xpathNode,
    unsigned xpathResultIndex) :
    file_(file),
    node_(nullptr),
    xpathResultSet_(resultSet),
    xpathNode_(resultSet ? xpathNode : (xpathNode ? new pugi::xpath_node(*xpathNode) : nullptr)),
    xpathResultIndex_(xpathResultIndex)
{
}

XMLElement::XMLElement(const XMLElement& rhs) :
    file_(rhs.file_),
    node_(rhs.node_),
    xpathResultSet_(rhs.xpathResultSet_),
    xpathNode_(rhs.xpathResultSet_ ? rhs.xpathNode_ : (rhs.xpathNode_ ? new pugi::xpath_node(*rhs.xpathNode_) : nullptr)),
    xpathResultIndex_(rhs.xpathResultIndex_)
{
}

XMLElement::~XMLElement()
{
    // XMLElement class takes the ownership of a single xpath_node object, so destruct it now
    if (!xpathResultSet_ && xpathNode_)
    {
        delete xpathNode_;
        xpathNode_ = nullptr;
    }
}

XMLElement& XMLElement::operator =(const XMLElement& rhs)
{
    file_ = rhs.file_;
    node_ = rhs.node_;
    xpathResultSet_ = rhs.xpathResultSet_;
    xpathNode_ = rhs.xpathResultSet_ ? rhs.xpathNode_ : (rhs.xpathNode_ ? new pugi::xpath_node(*rhs.xpathNode_) : nullptr);
    xpathResultIndex_ = rhs.xpathResultIndex_;
    return *this;
}

void XMLElement::SetName(const ea::string& name)
{
    SetName(name.c_str());
}

void XMLElement::SetName(const char* name)
{
    if (!file_ || (!node_ && !xpathNode_))
        return;

    pugi::xml_node node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    node.set_name(name);
}

XMLElement XMLElement::CreateChild(const ea::string& name)
{
    return CreateChild(name.c_str());
}

XMLElement XMLElement::CreateChild(const char* name)
{
    if (!file_ || (!node_ && !xpathNode_))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xml_node child = const_cast<pugi::xml_node&>(node).append_child(name);
    return XMLElement(file_, child.internal_object());
}

XMLElement XMLElement::GetOrCreateChild(const ea::string& name)
{
    XMLElement child = GetChild(name);
    if (child.NotNull())
        return child;
    else
        return CreateChild(name);
}

XMLElement XMLElement::GetOrCreateChild(const char* name)
{
    XMLElement child = GetChild(name);
    if (child.NotNull())
        return child;
    else
        return CreateChild(name);
}

bool XMLElement::AppendChild(XMLElement element, bool asCopy)
{
    if (!element.file_ || (!element.node_ && !element.xpathNode_) || !file_ || (!node_ && !xpathNode_))
        return false;

    pugi::xml_node node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    const pugi::xml_node& child = element.xpathNode_ ? element.xpathNode_->node() : pugi::xml_node(element.node_);

    if (asCopy)
        node.append_copy(child);
    else
        node.append_move(child);
    return true;
}

bool XMLElement::Remove()
{
    return GetParent().RemoveChild(*this);
}

bool XMLElement::RemoveChild(const XMLElement& element)
{
    if (!element.file_ || (!element.node_ && !element.xpathNode_) || !file_ || (!node_ && !xpathNode_))
        return false;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    const pugi::xml_node& child = element.xpathNode_ ? element.xpathNode_->node() : pugi::xml_node(element.node_);
    return const_cast<pugi::xml_node&>(node).remove_child(child);
}

bool XMLElement::RemoveChild(const ea::string& name)
{
    return RemoveChild(name.c_str());
}

bool XMLElement::RemoveChild(const char* name)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return const_cast<pugi::xml_node&>(node).remove_child(name);
}

bool XMLElement::RemoveChildren(const ea::string& name)
{
    return RemoveChildren(name.c_str());
}

bool XMLElement::RemoveChildren(const char* name)
{
    if ((!file_ || !node_) && !xpathNode_)
        return false;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    if (!CStringLength(name))
    {
        for (;;)
        {
            pugi::xml_node child = node.last_child();
            if (child.empty())
                break;
            const_cast<pugi::xml_node&>(node).remove_child(child);
        }
    }
    else
    {
        for (;;)
        {
            pugi::xml_node child = node.child(name);
            if (child.empty())
                break;
            const_cast<pugi::xml_node&>(node).remove_child(child);
        }
    }

    return true;
}

bool XMLElement::RemoveAttribute(const ea::string& name)
{
    return RemoveAttribute(name.c_str());
}

bool XMLElement::RemoveAttribute(const char* name)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    // If xpath_node contains just attribute, remove it regardless of the specified name
    if (xpathNode_ && xpathNode_->attribute())
        return xpathNode_->parent().remove_attribute(
            xpathNode_->attribute());  // In attribute context, xpath_node's parent is the parent node of the attribute itself

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return const_cast<pugi::xml_node&>(node).remove_attribute(node.attribute(name));
}

XMLElement XMLElement::SelectSingle(const ea::string& query, pugi::xpath_variable_set* variables) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xpath_node result = node.select_node(query.c_str(), variables);
    return XMLElement(file_, nullptr, &result, 0);
}

XMLElement XMLElement::SelectSinglePrepared(const XPathQuery& query) const
{
    if (!file_ || (!node_ && !xpathNode_ && !query.GetXPathQuery()))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xpath_node result = node.select_node(*query.GetXPathQuery());
    return XMLElement(file_, nullptr, &result, 0);
}

XPathResultSet XMLElement::Select(const ea::string& query, pugi::xpath_variable_set* variables) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return XPathResultSet();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xpath_node_set result = node.select_nodes(query.c_str(), variables);
    return XPathResultSet(file_, &result);
}

XPathResultSet XMLElement::SelectPrepared(const XPathQuery& query) const
{
    if (!file_ || (!node_ && !xpathNode_ && query.GetXPathQuery()))
        return XPathResultSet();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xpath_node_set result = node.select_nodes(*query.GetXPathQuery());
    return XPathResultSet(file_, &result);
}

bool XMLElement::SetValue(const ea::string& value)
{
    return SetValue(value.c_str());
}

bool XMLElement::SetValue(const char* value)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);

    // Search for existing value first
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
    {
        if (child.type() == pugi::node_pcdata)
            return const_cast<pugi::xml_node&>(child).set_value(value);
    }

    // If no previous value found, append new
    return const_cast<pugi::xml_node&>(node).append_child(pugi::node_pcdata).set_value(value);
}

bool XMLElement::SetAttribute(const ea::string& name, const ea::string& value)
{
    return SetAttribute(name.c_str(), value.c_str());
}

bool XMLElement::SetAttribute(const char* name, const char* value)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    // If xpath_node contains just attribute, set its value regardless of the specified name
    if (xpathNode_ && xpathNode_->attribute())
        return xpathNode_->attribute().set_value(value);

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    pugi::xml_attribute attr = node.attribute(name);
    if (attr.empty())
        attr = const_cast<pugi::xml_node&>(node).append_attribute(name);
    return attr.set_value(value);
}

bool XMLElement::SetAttribute(const ea::string& value)
{
    return SetAttribute(value.c_str());
}

bool XMLElement::SetAttribute(const char* value)
{
    // If xpath_node contains just attribute, set its value
    return xpathNode_ && xpathNode_->attribute() && xpathNode_->attribute().set_value(value);
}

bool XMLElement::SetBool(const ea::string& name, bool value)
{
    return SetAttribute(name, ToStringBool(value));
}

bool XMLElement::SetBoundingBox(const BoundingBox& value)
{
    if (!SetVector3("min", value.min_))
        return false;
    return SetVector3("max", value.max_);
}

bool XMLElement::SetBuffer(const ea::string& name, const void* data, unsigned size)
{
    ea::string dataStr;
    BufferToString(dataStr, data, size);
    return SetAttribute(name, dataStr);
}

bool XMLElement::SetBuffer(const ea::string& name, const ea::vector<unsigned char>& value)
{
    if (!value.size())
        return SetAttribute(name, EMPTY_STRING);
    else
        return SetBuffer(name, &value[0], value.size());
}

bool XMLElement::SetColor(const ea::string& name, const Color& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetFloat(const ea::string& name, float value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetDouble(const ea::string& name, double value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetUInt(const ea::string& name, unsigned value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetInt(const ea::string& name, int value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetUInt64(const ea::string& name, unsigned long long value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetInt64(const ea::string& name, long long value)
{
    return SetAttribute(name, ea::to_string(value));
}

bool XMLElement::SetIntRect(const ea::string& name, const IntRect& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetIntVector2(const ea::string& name, const IntVector2& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetIntVector3(const ea::string& name, const IntVector3& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetRect(const ea::string& name, const Rect& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetQuaternion(const ea::string& name, const Quaternion& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetString(const ea::string& name, const ea::string& value)
{
    return SetAttribute(name, value);
}

bool XMLElement::SetVariant(const Variant& value)
{
    if (!SetAttribute("type", value.GetTypeName()))
        return false;

    return SetVariantValue(value);
}

bool XMLElement::SetVariantValue(const Variant& value)
{
    switch (value.GetType())
    {
    case VariantType::ResourceRef:
        return SetResourceRef(value.GetResourceRef());

    case VariantType::ResourceRefList:
        return SetResourceRefList(value.GetResourceRefList());

    case VariantType::VariantVector:
        return SetVariantVector(value.GetVariantVector());

    case VariantType::StringVector:
        return SetStringVector(value.GetStringVector());

    case VariantType::VariantMap:
        return SetVariantMap(value.GetVariantMap());

    case VariantType::Custom:
    {
        if (const Serializable* object = value.GetCustom<SharedPtr<Serializable>>())
        {
            SetAttribute("type", object->GetTypeName());
            if (!object->SaveXML(*this))
            {
                RemoveAttribute("type");
                RemoveChildren();
            }
            else
                return true;
        }
        else
            URHO3D_LOGERROR("Serialization of objects other than SharedPtr<Serializable> is not supported.");
        return false;
    }

    default:
        return SetAttribute("value", value.ToString().c_str());
    }
}

bool XMLElement::SetResourceRef(const ResourceRef& value)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    // Need the context to query for the type
    Context* context = file_->GetContext();

    return SetAttribute("value", ea::string(context->GetTypeName(value.type_)) + ";" + value.name_);
}

bool XMLElement::SetResourceRefList(const ResourceRefList& value)
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    // Need the context to query for the type
    Context* context = file_->GetContext();

    ea::string str(context->GetTypeName(value.type_));
    for (unsigned i = 0; i < value.names_.size(); ++i)
    {
        str += ";";
        str += value.names_[i];
    }

    return SetAttribute("value", str.c_str());
}

bool XMLElement::SetVariantVector(const VariantVector& value)
{
    // Must remove all existing variant child elements (if they exist) to not cause confusion
    if (!RemoveChildren("variant"))
        return false;

    for (auto i = value.begin(); i != value.end(); ++i)
    {
        XMLElement variantElem = CreateChild("variant");
        if (!variantElem)
            return false;
        variantElem.SetVariant(*i);
    }

    return true;
}

bool XMLElement::SetStringVector(const StringVector& value)
{
    if (!RemoveChildren("string"))
        return false;

    for (auto i = value.begin(); i != value.end(); ++i)
    {
        XMLElement stringElem = CreateChild("string");
        if (!stringElem)
            return false;
        stringElem.SetAttribute("value", *i);
    }

    return true;
}

bool XMLElement::SetVariantMap(const VariantMap& value)
{
    if (!RemoveChildren("variant"))
        return false;

    for (auto i = value.begin(); i != value.end(); ++i)
    {
        XMLElement variantElem = CreateChild("variant");
        if (!variantElem)
            return false;
        variantElem.SetUInt("hash", i->first.Value());
        variantElem.SetVariant(i->second);
    }

    return true;
}

bool XMLElement::SetVector2(const ea::string& name, const Vector2& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetVector3(const ea::string& name, const Vector3& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetVector4(const ea::string& name, const Vector4& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetVectorVariant(const ea::string& name, const Variant& value)
{
    VariantType type = value.GetType();
    if (type == VariantType::Float || type == VariantType::Vector2 || type == VariantType::Vector3 || type == VariantType::Vector4 || type == VariantType::Matrix3 ||
        type == VariantType::Matrix3X4 || type == VariantType::Matrix4)
        return SetAttribute(name, value.ToString());
    else
        return false;
}

bool XMLElement::SetMatrix3(const ea::string& name, const Matrix3& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetMatrix3x4(const ea::string& name, const Matrix3x4& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::SetMatrix4(const ea::string& name, const Matrix4& value)
{
    return SetAttribute(name, value.ToString());
}

bool XMLElement::IsNull() const
{
    return !NotNull();
}

bool XMLElement::NotNull() const
{
    return node_ || (xpathNode_ && !xpathNode_->operator !());
}

XMLElement::operator bool() const
{
    return NotNull();
}

ea::string XMLElement::GetName() const
{
    if ((!file_ || !node_) && !xpathNode_)
        return ea::string();

    // If xpath_node contains just attribute, return its name instead
    if (xpathNode_ && xpathNode_->attribute())
        return ea::string(xpathNode_->attribute().name());

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return ea::string(node.name());
}

bool XMLElement::HasChild(const ea::string& name) const
{
    return HasChild(name.c_str());
}

bool XMLElement::HasChild(const char* name) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return !node.child(name).empty();
}

XMLElement XMLElement::GetChild(const ea::string& name) const
{
    return GetChild(name.c_str());
}

XMLElement XMLElement::GetChild(const char* name) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    if (!CStringLength(name))
        return XMLElement(file_, node.first_child().internal_object());
    else
        return XMLElement(file_, node.child(name).internal_object());
}

XMLElement XMLElement::GetNext(const ea::string& name) const
{
    return GetNext(name.c_str());
}

XMLElement XMLElement::GetNext(const char* name) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    if (!CStringLength(name))
        return XMLElement(file_, node.next_sibling().internal_object());
    else
        return XMLElement(file_, node.next_sibling(name).internal_object());
}

XMLElement XMLElement::GetParent() const
{
    if (!file_ || (!node_ && !xpathNode_))
        return XMLElement();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return XMLElement(file_, node.parent().internal_object());
}

unsigned XMLElement::GetNumAttributes() const
{
    if (!file_ || (!node_ && !xpathNode_))
        return 0;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    unsigned ret = 0;

    pugi::xml_attribute attr = node.first_attribute();
    while (!attr.empty())
    {
        ++ret;
        attr = attr.next_attribute();
    }

    return ret;
}

bool XMLElement::HasAttribute(const ea::string& name) const
{
    return HasAttribute(name.c_str());
}

bool XMLElement::HasAttribute(const char* name) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return false;

    // If xpath_node contains just attribute, check against it
    if (xpathNode_ && xpathNode_->attribute())
        return ea::string(xpathNode_->attribute().name()) == name;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return !node.attribute(name).empty();
}

ea::string XMLElement::GetValue() const
{
    if (!file_ || (!node_ && !xpathNode_))
        return EMPTY_STRING;

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return ea::string(node.child_value());
}

ea::string XMLElement::GetAttribute(const ea::string& name) const
{
    return ea::string(GetAttributeCString(name.c_str()));
}

ea::string XMLElement::GetAttribute(const char* name) const
{
    return ea::string(GetAttributeCString(name));
}

const char* XMLElement::GetAttributeCString(const char* name) const
{
    if (!file_ || (!node_ && !xpathNode_))
        return "";

    // If xpath_node contains just attribute, return it regardless of the specified name
    if (xpathNode_ && xpathNode_->attribute())
        return xpathNode_->attribute().value();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    return node.attribute(name).value();
}

ea::string XMLElement::GetAttributeLower(const ea::string& name) const
{
    return GetAttribute(name).to_lower();
}

ea::string XMLElement::GetAttributeLower(const char* name) const
{
    return ea::string(GetAttribute(name)).to_lower();
}

ea::string XMLElement::GetAttributeUpper(const ea::string& name) const
{
    return GetAttribute(name).to_upper();
}

ea::string XMLElement::GetAttributeUpper(const char* name) const
{
    return ea::string(GetAttribute(name)).to_upper();
}

ea::vector<ea::string> XMLElement::GetAttributeNames() const
{
    if (!file_ || (!node_ && !xpathNode_))
        return ea::vector<ea::string>();

    const pugi::xml_node& node = xpathNode_ ? xpathNode_->node() : pugi::xml_node(node_);
    ea::vector<ea::string> ret;

    pugi::xml_attribute attr = node.first_attribute();
    while (!attr.empty())
    {
        ret.push_back(ea::string(attr.name()));
        attr = attr.next_attribute();
    }

    return ret;
}

bool XMLElement::GetBool(const ea::string& name) const
{
    return ToBool(GetAttribute(name));
}

BoundingBox XMLElement::GetBoundingBox() const
{
    BoundingBox ret;

    ret.min_ = GetVector3("min");
    ret.max_ = GetVector3("max");
    return ret;
}

ea::vector<unsigned char> XMLElement::GetBuffer(const ea::string& name) const
{
    ea::vector<unsigned char> ret;
    StringToBuffer(ret, GetAttribute(name));
    return ret;
}

bool XMLElement::GetBuffer(const ea::string& name, void* dest, unsigned size) const
{
    ea::vector<ea::string> bytes = GetAttribute(name).split(' ');
    if (size < bytes.size())
        return false;

    auto* destBytes = (unsigned char*)dest;
    for (unsigned i = 0; i < bytes.size(); ++i)
        destBytes[i] = (unsigned char)ToInt(bytes[i]);
    return true;
}

Color XMLElement::GetColor(const ea::string& name) const
{
    return ToColor(GetAttribute(name));
}

float XMLElement::GetFloat(const ea::string& name) const
{
    return ToFloat(GetAttribute(name));
}

double XMLElement::GetDouble(const ea::string& name) const
{
    return ToDouble(GetAttribute(name));
}

unsigned XMLElement::GetUInt(const ea::string& name) const
{
    return ToUInt(GetAttribute(name));
}

int XMLElement::GetInt(const ea::string& name) const
{
    return ToInt(GetAttribute(name));
}

unsigned long long XMLElement::GetUInt64(const ea::string& name) const
{
    return ToUInt64(GetAttribute(name));
}

long long XMLElement::GetInt64(const ea::string& name) const
{
    return ToInt64(GetAttribute(name));
}

IntRect XMLElement::GetIntRect(const ea::string& name) const
{
    return ToIntRect(GetAttribute(name));
}

IntVector2 XMLElement::GetIntVector2(const ea::string& name) const
{
    return ToIntVector2(GetAttribute(name));
}

IntVector3 XMLElement::GetIntVector3(const ea::string& name) const
{
    return ToIntVector3(GetAttribute(name));
}

Quaternion XMLElement::GetQuaternion(const ea::string& name) const
{
    return ToQuaternion(GetAttribute(name));
}

Rect XMLElement::GetRect(const ea::string& name) const
{
    return ToRect(GetAttribute(name));
}

Variant XMLElement::GetVariant() const
{
    VariantType type = Variant::GetTypeFromName(GetAttribute("type"));
    return GetVariantValue(type);
}

Variant XMLElement::GetVariantValue(VariantType type, Context* context) const
{
    Variant ret;

    if (type == VariantType::ResourceRef)
        ret = GetResourceRef();
    else if (type == VariantType::ResourceRefList)
        ret = GetResourceRefList();
    else if (type == VariantType::VariantVector)
        ret = GetVariantVector();
    else if (type == VariantType::StringVector)
        ret = GetStringVector();
    else if (type == VariantType::VariantMap)
        ret = GetVariantMap();
    else if (type == VariantType::Custom)
    {
        if (!context)
        {
            URHO3D_LOGERROR("Context must not be null for SharedPtr<Serializable>");
            return ret;
        }

        const ea::string& typeName = GetAttribute("type");
        if (!typeName.empty())
        {
            SharedPtr<Serializable> object;
            object.StaticCast(context->CreateObject(typeName));

            if (object.NotNull())
            {
                // Restore proper refcount.
                if (object->LoadXML(*this))
                    ret.SetCustom(object);
                else
                    URHO3D_LOGERROR("Deserialization of '{}' failed", typeName);
            }
            else
                URHO3D_LOGERROR("Creation of type '{}' failed because it has no factory registered", typeName);
        }
        else if (!GetChild().IsNull())
            URHO3D_LOGERROR("Malformed xml input: 'type' attribute is required when deserializing an object");
    }
    else
        ret.FromString(type, GetAttributeCString("value"));

    return ret;
}

ResourceRef XMLElement::GetResourceRef() const
{
    ResourceRef ret;

    ea::vector<ea::string> values = GetAttribute("value").split(';');
    if (values.size() == 2)
    {
        ret.type_ = values[0];
        ret.name_ = values[1];
    }

    return ret;
}

ResourceRefList XMLElement::GetResourceRefList() const
{
    ResourceRefList ret;

    ea::vector<ea::string> values = GetAttribute("value").split(';', true);
    if (values.size() >= 1)
    {
        ret.type_ = values[0];
        ret.names_.resize(values.size() - 1);
        for (unsigned i = 1; i < values.size(); ++i)
            ret.names_[i - 1] = values[i];
    }

    return ret;
}

VariantVector XMLElement::GetVariantVector() const
{
    VariantVector ret;

    XMLElement variantElem = GetChild("variant");
    while (variantElem)
    {
        ret.push_back(variantElem.GetVariant());
        variantElem = variantElem.GetNext("variant");
    }

    return ret;
}

StringVector XMLElement::GetStringVector() const
{
    StringVector ret;

    XMLElement stringElem = GetChild("string");
    while (stringElem)
    {
        ret.push_back(stringElem.GetAttributeCString("value"));
        stringElem = stringElem.GetNext("string");
    }

    return ret;
}

VariantMap XMLElement::GetVariantMap() const
{
    VariantMap ret;

    XMLElement variantElem = GetChild("variant");
    while (variantElem)
    {
        // If this is a manually edited map, user can not be expected to calculate hashes manually. Also accept "name" attribute
        if (variantElem.HasAttribute("name"))
            ret[StringHash(variantElem.GetAttribute("name"))] = variantElem.GetVariant();
        else if (variantElem.HasAttribute("hash"))
            ret[StringHash(variantElem.GetUInt("hash"))] = variantElem.GetVariant();

        variantElem = variantElem.GetNext("variant");
    }

    return ret;
}

Vector2 XMLElement::GetVector2(const ea::string& name) const
{
    return ToVector2(GetAttribute(name));
}

Vector3 XMLElement::GetVector3(const ea::string& name) const
{
    return ToVector3(GetAttribute(name));
}

Vector4 XMLElement::GetVector4(const ea::string& name) const
{
    return ToVector4(GetAttribute(name));
}

Vector4 XMLElement::GetVector(const ea::string& name) const
{
    return ToVector4(GetAttribute(name), true);
}

Variant XMLElement::GetVectorVariant(const ea::string& name) const
{
    return ToVectorVariant(GetAttribute(name));
}

Matrix3 XMLElement::GetMatrix3(const ea::string& name) const
{
    return ToMatrix3(GetAttribute(name));
}

Matrix3x4 XMLElement::GetMatrix3x4(const ea::string& name) const
{
    return ToMatrix3x4(GetAttribute(name));
}

Matrix4 XMLElement::GetMatrix4(const ea::string& name) const
{
    return ToMatrix4(GetAttribute(name));
}

XMLFile* XMLElement::GetFile() const
{
    return file_;
}

XMLElement XMLElement::NextResult() const
{
    if (!xpathResultSet_ || !xpathNode_)
        return XMLElement();

    return xpathResultSet_->operator [](++xpathResultIndex_);
}

XPathResultSet::XPathResultSet() :
    resultSet_(nullptr)
{
}

XPathResultSet::XPathResultSet(XMLFile* file, pugi::xpath_node_set* resultSet) :
    file_(file),
    resultSet_(resultSet ? new pugi::xpath_node_set(resultSet->begin(), resultSet->end()) : nullptr)
{
    // Sort the node set in forward document order
    if (resultSet_)
        resultSet_->sort();
}

XPathResultSet::XPathResultSet(const XPathResultSet& rhs) :
    file_(rhs.file_),
    resultSet_(rhs.resultSet_ ? new pugi::xpath_node_set(rhs.resultSet_->begin(), rhs.resultSet_->end()) : nullptr)
{
}

XPathResultSet::~XPathResultSet()
{
    delete resultSet_;
    resultSet_ = nullptr;
}

XPathResultSet& XPathResultSet::operator =(const XPathResultSet& rhs)
{
    file_ = rhs.file_;
    resultSet_ = rhs.resultSet_ ? new pugi::xpath_node_set(rhs.resultSet_->begin(), rhs.resultSet_->end()) : nullptr;
    return *this;
}

XMLElement XPathResultSet::operator [](unsigned index) const
{
    if (!resultSet_)
        URHO3D_LOGERRORF(
            "Could not return result at index: %u. Most probably this is caused by the XPathResultSet not being stored in a lhs variable.",
            index);

    return resultSet_ && index < Size() ? XMLElement(file_, this, &resultSet_->operator [](index), index) : XMLElement();
}

XMLElement XPathResultSet::FirstResult()
{
    return operator [](0);
}

unsigned XPathResultSet::Size() const
{
    return resultSet_ ? (unsigned)resultSet_->size() : 0;
}

bool XPathResultSet::Empty() const
{
    return resultSet_ ? resultSet_->empty() : true;
}

XPathQuery::XPathQuery() = default;

XPathQuery::XPathQuery(const ea::string& queryString, const ea::string& variableString)
{
    SetQuery(queryString, variableString);
}

XPathQuery::~XPathQuery() = default;

void XPathQuery::Bind()
{
    // Delete previous query object and create a new one binding it with variable set
    query_ = ea::make_unique<pugi::xpath_query>(queryString_.c_str(), variables_.get());
}

bool XPathQuery::SetVariable(const ea::string& name, bool value)
{
    if (!variables_)
        variables_ = ea::make_unique<pugi::xpath_variable_set>();
    return variables_->set(name.c_str(), value);
}

bool XPathQuery::SetVariable(const ea::string& name, float value)
{
    if (!variables_)
        variables_ = ea::make_unique<pugi::xpath_variable_set>();
    return variables_->set(name.c_str(), value);
}

bool XPathQuery::SetVariable(const ea::string& name, const ea::string& value)
{
    return SetVariable(name.c_str(), value.c_str());
}

bool XPathQuery::SetVariable(const char* name, const char* value)
{
    if (!variables_)
        variables_ = ea::make_unique<pugi::xpath_variable_set>();
    return variables_->set(name, value);
}

bool XPathQuery::SetVariable(const ea::string& name, const XPathResultSet& value)
{
    if (!variables_)
        variables_ = ea::make_unique<pugi::xpath_variable_set>();

    pugi::xpath_node_set* nodeSet = value.GetXPathNodeSet();
    if (!nodeSet)
        return false;

    return variables_->set(name.c_str(), *nodeSet);
}

bool XPathQuery::SetQuery(const ea::string& queryString, const ea::string& variableString, bool bind)
{
    if (!variableString.empty())
    {
        Clear();
        variables_ = ea::make_unique<pugi::xpath_variable_set>();

        // Parse the variable string having format "name1:type1,name2:type2,..." where type is one of "Bool", "Float", "String", "ResultSet"
        ea::vector<ea::string> vars = variableString.split(',');
        for (auto i = vars.begin(); i != vars.end(); ++i)
        {
            ea::vector<ea::string> tokens = i->trimmed().split(':');
            if (tokens.size() != 2)
                continue;

            pugi::xpath_value_type type;
            if (tokens[1] == "Bool")
                type = pugi::xpath_type_boolean;
            else if (tokens[1] == "Float")
                type = pugi::xpath_type_number;
            else if (tokens[1] == "String")
                type = pugi::xpath_type_string;
            else if (tokens[1] == "ResultSet")
                type = pugi::xpath_type_node_set;
            else
                return false;

            if (!variables_->add(tokens[0].c_str(), type))
                return false;
        }
    }

    queryString_ = queryString;

    if (bind)
        Bind();

    return true;
}

void XPathQuery::Clear()
{
    queryString_.clear();

    variables_.reset();
    query_.reset();
}

bool XPathQuery::EvaluateToBool(const XMLElement& element) const
{
    if (!query_ || ((!element.GetFile() || !element.GetNode()) && !element.GetXPathNode()))
        return false;

    const pugi::xml_node& node = element.GetXPathNode() ? element.GetXPathNode()->node() : pugi::xml_node(element.GetNode());
    return query_->evaluate_boolean(node);
}

float XPathQuery::EvaluateToFloat(const XMLElement& element) const
{
    if (!query_ || ((!element.GetFile() || !element.GetNode()) && !element.GetXPathNode()))
        return 0.0f;

    const pugi::xml_node& node = element.GetXPathNode() ? element.GetXPathNode()->node() : pugi::xml_node(element.GetNode());
    return (float)query_->evaluate_number(node);
}

ea::string XPathQuery::EvaluateToString(const XMLElement& element) const
{
    if (!query_ || ((!element.GetFile() || !element.GetNode()) && !element.GetXPathNode()))
        return EMPTY_STRING;

    const pugi::xml_node& node = element.GetXPathNode() ? element.GetXPathNode()->node() : pugi::xml_node(element.GetNode());
    ea::string result;
    // First call get the size
    result.reserve((unsigned) query_->evaluate_string(nullptr, 0, node));
    // Second call get the actual string
    query_->evaluate_string(const_cast<pugi::char_t*>(result.c_str()), result.capacity(), node);
    return result;
}

XPathResultSet XPathQuery::Evaluate(const XMLElement& element) const
{
    if (!query_ || ((!element.GetFile() || !element.GetNode()) && !element.GetXPathNode()))
        return XPathResultSet();

    const pugi::xml_node& node = element.GetXPathNode() ? element.GetXPathNode()->node() : pugi::xml_node(element.GetNode());
    pugi::xpath_node_set result = query_->evaluate_node_set(node);
    return XPathResultSet(element.GetFile(), &result);
}

}
