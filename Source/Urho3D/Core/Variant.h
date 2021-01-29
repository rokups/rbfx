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

#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unique_ptr.h>

#include "../Container/Ptr.h"
#include "../Container/ByteVector.h"
#include "../Core/EnumIndex.h"
#include "../Core/TypeTrait.h"
#include "../Math/Color.h"
#include "../Math/Matrix3.h"
#include "../Math/Matrix3x4.h"
#include "../Math/Rect.h"
#include "../Math/StringHash.h"

#include <typeinfo>

namespace Urho3D
{

class Archive;

/// Variant's supported types.
enum class VariantType : unsigned char
{
    None = 0,
    Int,
    Bool,
    Float,
    Vector2,
    Vector3,
    Vector4,
    Quaternion,
    Color,
    String,
    Buffer,
    VoidPtr,
    ResourceRef,
    ResourceRefList,
    VariantVector,
    VariantMap,
    IntRect,
    IntVector2,
    Ptr,
    Matrix3,
    Matrix3X4,
    Matrix4,
    Double,
    StringVector,
    Rect,
    IntVector3,
    Int64,
    // Add new types here
    Custom,
    MaxVarTypes
};

static const auto MaxVarTypes = static_cast<unsigned>(VariantType::MaxVarTypes);

URHO3D_ENUM_INDEX(VariantType);

#ifdef URHO3D_LEGACY_ENUMS
static const VariantType VAR_NONE = VariantType::None;
static const VariantType VAR_INT = VariantType::Int;
static const VariantType VAR_BOOL = VariantType::Bool;
static const VariantType VAR_FLOAT = VariantType::Float;
static const VariantType VAR_VECTOR2 = VariantType::Vector2;
static const VariantType VAR_VECTOR3 = VariantType::Vector3;
static const VariantType VAR_VECTOR4 = VariantType::Vector4;
static const VariantType VAR_QUATERNION = VariantType::Quaternion;
static const VariantType VAR_COLOR = VariantType::Color;
static const VariantType VAR_STRING = VariantType::String;
static const VariantType VAR_BUFFER = VariantType::Buffer;
static const VariantType VAR_VOIDPTR = VariantType::VoidPtr;
static const VariantType VAR_RESOURCEREF = VariantType::ResourceRef;
static const VariantType VAR_RESOURCEREFLIST = VariantType::ResourceRefList;
static const VariantType VAR_VARIANTVECTOR = VariantType::VariantVector;
static const VariantType VAR_VARIANTMAP = VariantType::VariantMap;
static const VariantType VAR_INTRECT = VariantType::IntRect;
static const VariantType VAR_INTVECTOR2 = VariantType::IntVector2;
static const VariantType VAR_PTR = VariantType::Ptr;
static const VariantType VAR_MATRIX3 = VariantType::Matrix3;
static const VariantType VAR_MATRIX3X4 = VariantType::Matrix3X4;
static const VariantType VAR_MATRIX4 = VariantType::Matrix4;
static const VariantType VAR_DOUBLE = VariantType::Double;
static const VariantType VAR_STRINGVECTOR = VariantType::StringVector;
static const VariantType VAR_RECT = VariantType::Rect;
static const VariantType VAR_INTVECTOR3 = VariantType::IntVector3;
static const VariantType VAR_INT64 = VariantType::Int64;
static const VariantType VAR_CUSTOM = VariantType::Custom;
static const VariantType MAX_VAR_TYPES = VariantType::MaxVarTypes;
#endif


class Variant;
class VectorBuffer;

/// Vector of variants.
using VariantVector = ea::vector<Variant>;

/// Vector of strings.
using StringVector = ea::vector<ea::string>;

/// Map of variants.
using VariantMap = ea::unordered_map<StringHash, Variant>;

/// Map from string to Variant.
using StringVariantMap = ea::unordered_map<ea::string, Variant>;

/// Deprecated. Use ByteVector instead.
/// TODO: Rename all instances of VariantBuffer to ByteVector.
using VariantBuffer = ea::vector<unsigned char>;

/// Typed resource reference.
struct URHO3D_API ResourceRef
{
    /// Construct.
    ResourceRef() = default;

    /// Construct with type only and empty id.
    explicit ResourceRef(StringHash type) :
        type_(type)
    {
    }

    /// Construct with type and resource name.
    ResourceRef(StringHash type, const ea::string& name) :
        type_(type),
        name_(name)
    {
    }

    /// Construct with type and resource name.
    ResourceRef(const ea::string& type, const ea::string& name) :
        type_(type),
        name_(name)
    {
    }

    /// Construct with type and resource name.
    ResourceRef(const char* type, const char* name) :
        type_(type),
        name_(name)
    {
    }

    /// Construct from another ResourceRef.
    ResourceRef(const ResourceRef& rhs) = default;

    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const
    {
        return type_.ToHash() * 31 + ea::hash<ea::string>()(name_);
    }

    /// Object type.
    StringHash type_;
    /// Object name.
    ea::string name_;

    /// Test for equality with another reference.
    bool operator ==(const ResourceRef& rhs) const { return type_ == rhs.type_ && name_ == rhs.name_; }

    /// Test for inequality with another reference.
    bool operator !=(const ResourceRef& rhs) const { return type_ != rhs.type_ || name_ != rhs.name_; }
};

/// %List of typed resource references.
struct URHO3D_API ResourceRefList
{
    /// Construct.
    ResourceRefList() = default;

    /// Construct with type only.
    explicit ResourceRefList(StringHash type) :
        type_(type)
    {
    }

    /// Construct with type and id list.
    ResourceRefList(StringHash type, const StringVector& names) :
        type_(type),
        names_(names)
    {
    }

    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const
    {
        return type_.ToHash() * 31 + ea::hash<StringVector>()(names_);
    }

    /// Object type.
    StringHash type_;
    /// List of object names.
    StringVector names_;

    /// Test for equality with another reference list.
    bool operator ==(const ResourceRefList& rhs) const { return type_ == rhs.type_ && names_ == rhs.names_; }

    /// Test for inequality with another reference list.
    bool operator !=(const ResourceRefList& rhs) const { return type_ != rhs.type_ || names_ != rhs.names_; }
};

/// Custom variant value. This type is not abstract to store it in the VariantValue by value.
/// @nobindtemp
class CustomVariantValue
{
    // GetValuePtr expects that CustomVariantValue is always convertible to CustomVariantValueImpl<T>.
    template <class T> friend class CustomVariantValueImpl;

private:
    /// Construct from type info.
    explicit CustomVariantValue(const std::type_info& typeInfo) : typeInfo_(typeInfo) {}

public:
    /// Construct empty.
    CustomVariantValue() = default;
    /// Destruct.
    virtual ~CustomVariantValue() = default;

    /// Get the type info.
    const std::type_info& GetTypeInfo() const { return typeInfo_; }

    /// Return whether the specified type is stored.
    template <class T> bool IsType() const { return GetTypeInfo() == typeid(T); }
    /// Return pointer to value of the specified type. Return null pointer if type does not match.
    template <class T> T* GetValuePtr() { return const_cast<T*>(const_cast<const CustomVariantValue*>(this)->GetValuePtr<T>()); }
    /// Return const pointer to value of the specified type. Return null pointer if type does not match.
    template <class T> const T* GetValuePtr() const;

    /// Assign value to destination.
    virtual bool CopyTo(CustomVariantValue& dest) const { return false; }
    /// Clone object over destination.
    virtual void CloneTo(void* dest) const { }
    /// Get size.
    virtual unsigned GetSize() const { return sizeof(CustomVariantValue); }

    /// Compare to another custom value.
    virtual bool Compare(const CustomVariantValue& /*rhs*/) const { return false; }
    /// Compare to zero.
    virtual bool IsZero() const { return false; }
    /// Convert custom value to string.
    virtual ea::string ToString() const { return EMPTY_STRING; }
    /// Serialize to Archive.
    virtual bool Serialize(Archive& /*archive*/) { return false; }

private:
    /// Type info.
    const std::type_info& typeInfo_{ typeid(void) };
};

URHO3D_TYPE_TRAIT(IsEquallyComparable, std::declval<T>() == std::declval<T>());
URHO3D_TYPE_TRAIT(IsConvertibleToString, std::declval<T>().ToString());
URHO3D_TYPE_TRAIT(IsArchiveSerializable, SerializeValue(std::declval<Archive&>(), "", std::declval<T&>()));

/// Custom variant value type traits. Specialize the template to implement custom type behavior.
template <class T> struct CustomVariantValueTraits
{
    /// Copy value.
    static void Copy(T& dest, const T& src)
    {
        dest = src;
    }
    /// Compare values.
    static bool Compare(const T& lhs, const T& rhs)
    {
        if constexpr (IsEquallyComparable<T>::value)
        {
            return lhs == rhs;
        }
        else
        {
            (void)lhs;
            (void)rhs;
            return false;
        }
    }
    /// Check whether the value is zero.
    static bool IsZero(const T& value)
    {
        if constexpr (IsEquallyComparable<T>::value && std::is_default_constructible<T>::value)
        {
            return value == T{};
        }
        else
        {
            (void)value;
            return false;
        }
    }
    /// Convert type to string.
    static ea::string ToString(const T& value)
    {
        if constexpr (IsConvertibleToString<T>::value)
        {
            return value.ToString();
        }
        else
        {
            (void)value;
            return EMPTY_STRING;
        }
    }
    /// Serialize type.
    static bool Serialize(Archive& archive, T& value)
    {
        if constexpr (IsArchiveSerializable<T>::value)
        {
            return SerializeValue(archive, "value", value);
        }
        else
        {
            (void)value;
            (void)archive;
            return false;
        }
    }
};

/// Custom variant value type traits for unique_ptr.
template <class T> struct CustomVariantValueTraits<ea::unique_ptr<T>>
{
    /// Copy value.
    static void Copy(ea::unique_ptr<T>& dest, const ea::unique_ptr<T>& src) { dest = ea::make_unique<T>(*src); }
    /// Compare types.
    static bool Compare(const ea::unique_ptr<T>& lhs, const ea::unique_ptr<T>& rhs) { return CustomVariantValueTraits<T>::Compare(*lhs, *rhs); }
    /// Check whether the value is zero.
    static bool IsZero(const ea::unique_ptr<T>& value) { return CustomVariantValueTraits<T>::IsZero(*value); }
    /// Convert type to string.
    static ea::string ToString(const ea::unique_ptr<T>& value) { return CustomVariantValueTraits<T>::ToString(*value); }
    /// Serialize type.
    static bool Serialize(Archive& archive, ea::unique_ptr<T>& value) { return CustomVariantValueTraits<T>::Serialize(archive, *value); }
};

/// Custom variant value implementation.
template <class T> class CustomVariantValueImpl final : public CustomVariantValue
{
public:
    /// This class name.
    using ClassName = CustomVariantValueImpl<T>;
    /// Type traits.
    using Traits = CustomVariantValueTraits<T>;
    /// Construct from value.
    explicit CustomVariantValueImpl(const T& value)
        : CustomVariantValue(typeid(T))
    {
        Traits::Copy(value_, value);
    }
    /// Get value.
    T& GetValue() { return value_; }
    /// Get const value.
    const T& GetValue() const { return value_; }

    /// Assign value to destination.
    bool CopyTo(CustomVariantValue& dest) const override
    {
        if (T* destValue = dest.GetValuePtr<T>())
        {
            Traits::Copy(*destValue, value_);
            return true;
        }
        return false;
    }
    /// Clone object over destination.
    void CloneTo(void* dest) const override { new (dest) ClassName(value_); }
    /// Get size.
    unsigned GetSize() const override { return sizeof(ClassName); }

    /// Compare to another custom value.
    bool Compare(const CustomVariantValue& rhs) const override
    {
        const T* rhsValue = rhs.GetValuePtr<T>();
        return rhsValue && Traits::Compare(value_, *rhsValue);
    }
    /// Compare to zero.
    bool IsZero() const override { return Traits::IsZero(value_);}
    /// Convert custom value to string.
    ea::string ToString() const override { return Traits::ToString(value_); }
    /// Serialize to Archive.
    bool Serialize(Archive& archive) override { return Traits::Serialize(archive, value_); }

private:
    /// Value.
    T value_;
};

/// Size of variant value. 16 bytes on 32-bit platform, 32 bytes on 64-bit platform.
static const unsigned VARIANT_VALUE_SIZE = sizeof(void*) * 4;

/// Checks whether the custom variant type could be stored on stack.
template <class T> constexpr bool IsCustomTypeOnStack() { return sizeof(CustomVariantValueImpl<T>) <= VARIANT_VALUE_SIZE; }

/// Union for the possible variant values. Objects exceeding the VARIANT_VALUE_SIZE are allocated on the heap.
union VariantValue
{
    unsigned char storage_[VARIANT_VALUE_SIZE];
    int int_;
    bool bool_;
    float float_;
    double double_;
    long long int64_;
    void* voidPtr_;
    WeakPtr<RefCounted> weakPtr_;
    Vector2 vector2_;
    Vector3 vector3_;
    Vector4 vector4_;
    Rect rect_;
    IntVector2 intVector2_;
    IntVector3 intVector3_;
    IntRect intRect_;
    Matrix3* matrix3_;
    Matrix3x4* matrix3x4_;
    Matrix4* matrix4_;
    Quaternion quaternion_;
    Color color_;
    ea::string string_;
    StringVector stringVector_;
    VariantVector variantVector_;
    VariantMap* variantMap_;
    VariantBuffer buffer_;
    ResourceRef resourceRef_;
    ResourceRefList resourceRefList_;

    /// Construct uninitialized.
    VariantValue() { }      // NOLINT(modernize-use-equals-default)
    /// Non-copyable.
    VariantValue(const VariantValue& value) = delete;
    /// Destruct.
    ~VariantValue() { }     // NOLINT(modernize-use-equals-default)

    /// Get custom variant value.
    CustomVariantValue& AsCustomValue() { return *reinterpret_cast<CustomVariantValue*>(&storage_[0]); }
    /// Get custom variant value.
    const CustomVariantValue& AsCustomValue() const { return *reinterpret_cast<const CustomVariantValue*>(&storage_[0]); }
};

// TODO: static_assert(sizeof(VariantValue) == VARIANT_VALUE_SIZE, "Unexpected size of VariantValue");
static_assert(sizeof(CustomVariantValueImpl<SharedPtr<RefCounted>>) <= VARIANT_VALUE_SIZE, "SharedPtr<> does not fit into variant.");

/// Variable that supports a fixed set of types.
class URHO3D_API Variant
{
public:
    /// Construct empty.
    Variant() = default;

    /// Construct from integer.
    Variant(int value)                  // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from 64 bit integer.
    Variant(long long value)            // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from unsigned integer.
    Variant(unsigned value)             // NOLINT(google-explicit-constructor)
    {
        *this = (int)value;
    }

    /// Construct from unsigned integer.
    Variant(unsigned long long value)   // NOLINT(google-explicit-constructor)
    {
        *this = (long long)value;
    }

    /// Construct from a string hash (convert to integer).
    Variant(const StringHash& value)    // NOLINT(google-explicit-constructor)
    {
        *this = (int)value.Value();
    }

    /// Construct from a bool.
    Variant(bool value)                 // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a float.
    Variant(float value)                // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a double.
    Variant(double value)               // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Vector2.
    Variant(const Vector2& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Vector3.
    Variant(const Vector3& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Vector4.
    Variant(const Vector4& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a quaternion.
    Variant(const Quaternion& value)    // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a color.
    Variant(const Color& value)         // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a string.
    Variant(const ea::string& value)        // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a C string.
    Variant(const char* value)          // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a buffer.
    Variant(const VariantBuffer& value)      // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a %VectorBuffer and store as a buffer.
    Variant(const VectorBuffer& value)  // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a pointer.
    Variant(void* value)                // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a resource reference.
    Variant(const ResourceRef& value)   // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a resource reference list.
    Variant(const ResourceRefList& value)   // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a variant vector.
    Variant(const VariantVector& value) // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a variant map.
    Variant(const VariantMap& value)    // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a string vector.
    Variant(const StringVector& value)  // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a rect.
    Variant(const Rect& value)          // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from an integer rect.
    Variant(const IntRect& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from an IntVector2.
    Variant(const IntVector2& value)    // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from an IntVector3.
    Variant(const IntVector3& value)    // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a RefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant(RefCounted* value)          // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Matrix3.
    Variant(const Matrix3& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Matrix3x4.
    Variant(const Matrix3x4& value)     // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from a Matrix4.
    Variant(const Matrix4& value)       // NOLINT(google-explicit-constructor)
    {
        *this = value;
    }

    /// Construct from type and value.
    Variant(const ea::string& type, const ea::string& value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const ea::string& value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(const char* type, const char* value)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const char* value)
    {
        FromString(type, value);
    }

    /// Construct from type.
    Variant(VariantType type);

    /// Copy-construct from another variant.
    Variant(const Variant& value)
    {
        *this = value;
    }

    /// Destruct.
    ~Variant()
    {
        SetType(VariantType::None);
    }

    /// Reset to empty.
    void Clear()
    {
        SetType(VariantType::None);
    }

    /// Assign from another variant.
    Variant& operator =(const Variant& rhs);

    /// Assign from an integer.
    Variant& operator =(int rhs)
    {
        SetType(VariantType::Int);
        value_.int_ = rhs;
        return *this;
    }

    /// Assign from 64 bit integer.
    Variant& operator =(long long rhs)
    {
        SetType(VariantType::Int64);
        value_.int64_ = rhs;
        return *this;
    }

    /// Assign from unsigned 64 bit integer.
    Variant& operator =(unsigned long long rhs)
    {
        SetType(VariantType::Int64);
        value_.int64_ = static_cast<long long>(rhs);
        return *this;
    }

    /// Assign from an unsigned integer.
    Variant& operator =(unsigned rhs)
    {
        SetType(VariantType::Int);
        value_.int_ = (int)rhs;
        return *this;
    }

    /// Assign from a StringHash (convert to integer).
    Variant& operator =(const StringHash& rhs)
    {
        SetType(VariantType::Int);
        value_.int_ = (int)rhs.Value();
        return *this;
    }

    /// Assign from a bool.
    Variant& operator =(bool rhs)
    {
        SetType(VariantType::Bool);
        value_.bool_ = rhs;
        return *this;
    }

    /// Assign from a float.
    Variant& operator =(float rhs)
    {
        SetType(VariantType::Float);
        value_.float_ = rhs;
        return *this;
    }

    /// Assign from a double.
    Variant& operator =(double rhs)
    {
        SetType(VariantType::Double);
        value_.double_ = rhs;
        return *this;
    }

    /// Assign from a Vector2.
    Variant& operator =(const Vector2& rhs)
    {
        SetType(VariantType::Vector2);
        value_.vector2_ = rhs;
        return *this;
    }

    /// Assign from a Vector3.
    Variant& operator =(const Vector3& rhs)
    {
        SetType(VariantType::Vector3);
        value_.vector3_ = rhs;
        return *this;
    }

    /// Assign from a Vector4.
    Variant& operator =(const Vector4& rhs)
    {
        SetType(VariantType::Vector4);
        value_.vector4_ = rhs;
        return *this;
    }

    /// Assign from a quaternion.
    Variant& operator =(const Quaternion& rhs)
    {
        SetType(VariantType::Quaternion);
        value_.quaternion_ = rhs;
        return *this;
    }

    /// Assign from a color.
    Variant& operator =(const Color& rhs)
    {
        SetType(VariantType::Color);
        value_.color_ = rhs;
        return *this;
    }

    /// Assign from a string.
    Variant& operator =(const ea::string& rhs)
    {
        SetType(VariantType::String);
        value_.string_ = rhs;
        return *this;
    }

    /// Assign from a C string.
    Variant& operator =(const char* rhs)
    {
        SetType(VariantType::String);
        value_.string_ = rhs;
        return *this;
    }

    /// Assign from a buffer.
    Variant& operator =(const VariantBuffer& rhs)
    {
        SetType(VariantType::Buffer);
        value_.buffer_ = rhs;
        return *this;
    }

    /// Assign from a %VectorBuffer and store as a buffer.
    Variant& operator =(const VectorBuffer& rhs);

    /// Assign from a void pointer.
    Variant& operator =(void* rhs)
    {
        SetType(VariantType::VoidPtr);
        value_.voidPtr_ = rhs;
        return *this;
    }

    /// Assign from a resource reference.
    Variant& operator =(const ResourceRef& rhs)
    {
        SetType(VariantType::ResourceRef);
        value_.resourceRef_ = rhs;
        return *this;
    }

    /// Assign from a resource reference list.
    Variant& operator =(const ResourceRefList& rhs)
    {
        SetType(VariantType::ResourceRefList);
        value_.resourceRefList_ = rhs;
        return *this;
    }

    /// Assign from a variant vector.
    Variant& operator =(const VariantVector& rhs)
    {
        SetType(VariantType::VariantVector);
        value_.variantVector_ = rhs;
        return *this;
    }

    /// Assign from a string vector.
    Variant& operator =(const StringVector& rhs)
    {
        SetType(VariantType::StringVector);
        value_.stringVector_ = rhs;
        return *this;
    }

    /// Assign from a variant map.
    Variant& operator =(const VariantMap& rhs)
    {
        SetType(VariantType::VariantMap);
        *value_.variantMap_ = rhs;
        return *this;
    }

    /// Assign from a rect.
    Variant& operator =(const Rect& rhs)
    {
        SetType(VariantType::Rect);
        value_.rect_ = rhs;
        return *this;
    }

    /// Assign from an integer rect.
    Variant& operator =(const IntRect& rhs)
    {
        SetType(VariantType::IntRect);
        value_.intRect_ = rhs;
        return *this;
    }

    /// Assign from an IntVector2.
    Variant& operator =(const IntVector2& rhs)
    {
        SetType(VariantType::IntVector2);
        value_.intVector2_ = rhs;
        return *this;
    }

    /// Assign from an IntVector3.
    Variant& operator =(const IntVector3& rhs)
    {
        SetType(VariantType::IntVector3);
        value_.intVector3_ = rhs;
        return *this;
    }

    /// Assign from a RefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant& operator =(RefCounted* rhs)
    {
        SetType(VariantType::Ptr);
        value_.weakPtr_ = rhs;
        return *this;
    }

    /// Assign from a Matrix3.
    Variant& operator =(const Matrix3& rhs)
    {
        SetType(VariantType::Matrix3);
        *value_.matrix3_ = rhs;
        return *this;
    }

    /// Assign from a Matrix3x4.
    Variant& operator =(const Matrix3x4& rhs)
    {
        SetType(VariantType::Matrix3X4);
        *value_.matrix3x4_ = rhs;
        return *this;
    }

    /// Assign from a Matrix4.
    Variant& operator =(const Matrix4& rhs)
    {
        SetType(VariantType::Matrix4);
        *value_.matrix4_ = rhs;
        return *this;
    }

    /// Test for equality with another variant.
    bool operator ==(const Variant& rhs) const;

    /// Test for equality with an integer. To return true, both the type and value must match.
    bool operator ==(int rhs) const { return type_ == VariantType::Int ? value_.int_ == rhs : false; }

    /// Test for equality with an unsigned 64 bit integer. To return true, both the type and value must match.
    bool operator ==(unsigned rhs) const { return type_ == VariantType::Int ? value_.int_ == static_cast<int>(rhs) : false; }

    /// Test for equality with an 64 bit integer. To return true, both the type and value must match.
    bool operator ==(long long rhs) const { return type_ == VariantType::Int64 ? value_.int64_ == rhs : false; }

    /// Test for equality with an unsigned integer. To return true, both the type and value must match.
    bool operator ==(unsigned long long rhs) const { return type_ == VariantType::Int64 ? value_.int64_ == static_cast<long long>(rhs) : false; }

    /// Test for equality with a bool. To return true, both the type and value must match.
    bool operator ==(bool rhs) const { return type_ == VariantType::Bool ? value_.bool_ == rhs : false; }

    /// Test for equality with a float. To return true, both the type and value must match.
    bool operator ==(float rhs) const { return type_ == VariantType::Float ? value_.float_ == rhs : false; }

    /// Test for equality with a double. To return true, both the type and value must match.
    bool operator ==(double rhs) const { return type_ == VariantType::Double ? value_.double_ == rhs : false; }

    /// Test for equality with a Vector2. To return true, both the type and value must match.
    bool operator ==(const Vector2& rhs) const
    {
        return type_ == VariantType::Vector2 ? value_.vector2_ == rhs : false;
    }

    /// Test for equality with a Vector3. To return true, both the type and value must match.
    bool operator ==(const Vector3& rhs) const
    {
        return type_ == VariantType::Vector3 ? value_.vector3_ == rhs : false;
    }

    /// Test for equality with a Vector4. To return true, both the type and value must match.
    bool operator ==(const Vector4& rhs) const
    {
        return type_ == VariantType::Vector4 ? value_.vector4_ == rhs : false;
    }

    /// Test for equality with a quaternion. To return true, both the type and value must match.
    bool operator ==(const Quaternion& rhs) const
    {
        return type_ == VariantType::Quaternion ? value_.quaternion_ == rhs : false;
    }

    /// Test for equality with a color. To return true, both the type and value must match.
    bool operator ==(const Color& rhs) const
    {
        return type_ == VariantType::Color ? value_.color_ == rhs : false;
    }

    /// Test for equality with a string. To return true, both the type and value must match.
    bool operator ==(const ea::string& rhs) const
    {
        return type_ == VariantType::String ? value_.string_ == rhs : false;
    }

    /// Test for equality with a buffer. To return true, both the type and value must match.
    bool operator ==(const VariantBuffer& rhs) const;
    /// Test for equality with a %VectorBuffer. To return true, both the type and value must match.
    bool operator ==(const VectorBuffer& rhs) const;

    /// Test for equality with a void pointer. To return true, both the type and value must match, with the exception that a RefCounted pointer is also allowed.
    bool operator ==(void* rhs) const
    {
        if (type_ == VariantType::VoidPtr)
            return value_.voidPtr_ == rhs;
        else if (type_ == VariantType::Ptr)
            return value_.weakPtr_ == rhs;
        else
            return false;
    }

    /// Test for equality with a resource reference. To return true, both the type and value must match.
    bool operator ==(const ResourceRef& rhs) const
    {
        return type_ == VariantType::ResourceRef ? value_.resourceRef_ == rhs : false;
    }

    /// Test for equality with a resource reference list. To return true, both the type and value must match.
    bool operator ==(const ResourceRefList& rhs) const
    {
        return type_ == VariantType::ResourceRefList ? value_.resourceRefList_ == rhs : false;
    }

    /// Test for equality with a variant vector. To return true, both the type and value must match.
    bool operator ==(const VariantVector& rhs) const
    {
        return type_ == VariantType::VariantVector ? value_.variantVector_ == rhs : false;
    }

    /// Test for equality with a string vector. To return true, both the type and value must match.
    bool operator ==(const StringVector& rhs) const
    {
        return type_ == VariantType::StringVector ? value_.stringVector_ == rhs : false;
    }

    /// Test for equality with a variant map. To return true, both the type and value must match.
    bool operator ==(const VariantMap& rhs) const
    {
        return type_ == VariantType::VariantMap ? *value_.variantMap_ == rhs : false;
    }

    /// Test for equality with a rect. To return true, both the type and value must match.
    bool operator ==(const Rect& rhs) const
    {
        return type_ == VariantType::Rect ? value_.rect_ == rhs : false;
    }

    /// Test for equality with an integer rect. To return true, both the type and value must match.
    bool operator ==(const IntRect& rhs) const
    {
        return type_ == VariantType::IntRect ? value_.intRect_ == rhs : false;
    }

    /// Test for equality with an IntVector2. To return true, both the type and value must match.
    bool operator ==(const IntVector2& rhs) const
    {
        return type_ == VariantType::IntVector2 ? value_.intVector2_ == rhs : false;
    }

    /// Test for equality with an IntVector3. To return true, both the type and value must match.
    bool operator ==(const IntVector3& rhs) const
    {
        return type_ == VariantType::IntVector3 ? value_.intVector3_ == rhs : false;
    }

    /// Test for equality with a StringHash. To return true, both the type and value must match.
    bool operator ==(const StringHash& rhs) const { return type_ == VariantType::Int ? static_cast<unsigned>(value_.int_) == rhs.Value() : false; }

    /// Test for equality with a RefCounted pointer. To return true, both the type and value must match, with the exception that void pointer is also allowed.
    bool operator ==(RefCounted* rhs) const
    {
        if (type_ == VariantType::Ptr)
            return value_.weakPtr_ == rhs;
        else if (type_ == VariantType::VoidPtr)
            return value_.voidPtr_ == rhs;
        else
            return false;
    }

    /// Test for equality with a Matrix3. To return true, both the type and value must match.
    bool operator ==(const Matrix3& rhs) const
    {
        return type_ == VariantType::Matrix3 ? *value_.matrix3_ == rhs : false;
    }

    /// Test for equality with a Matrix3x4. To return true, both the type and value must match.
    bool operator ==(const Matrix3x4& rhs) const
    {
        return type_ == VariantType::Matrix3X4 ? *value_.matrix3x4_ == rhs : false;
    }

    /// Test for equality with a Matrix4. To return true, both the type and value must match.
    bool operator ==(const Matrix4& rhs) const
    {
        return type_ == VariantType::Matrix4 ? *value_.matrix4_ == rhs : false;
    }

    /// Test for inequality with another variant.
    bool operator !=(const Variant& rhs) const { return !(*this == rhs); }

    /// Test for inequality with an integer.
    bool operator !=(int rhs) const { return !(*this == rhs); }

    /// Test for inequality with an unsigned integer.
    bool operator !=(unsigned rhs) const { return !(*this == rhs); }

    /// Test for inequality with an 64 bit integer.
    bool operator !=(long long rhs) const { return !(*this == rhs); }

    /// Test for inequality with an unsigned 64 bit integer.
    bool operator !=(unsigned long long rhs) const { return !(*this == rhs); }

    /// Test for inequality with a bool.
    bool operator !=(bool rhs) const { return !(*this == rhs); }

    /// Test for inequality with a float.
    bool operator !=(float rhs) const { return !(*this == rhs); }

    /// Test for inequality with a double.
    bool operator !=(double rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Vector2.
    bool operator !=(const Vector2& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Vector3.
    bool operator !=(const Vector3& rhs) const { return !(*this == rhs); }

    /// Test for inequality with an Vector4.
    bool operator !=(const Vector4& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Quaternion.
    bool operator !=(const Quaternion& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a string.
    bool operator !=(const ea::string& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a buffer.
    bool operator !=(const VariantBuffer& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a %VectorBuffer.
    bool operator !=(const VectorBuffer& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a pointer.
    bool operator !=(void* rhs) const { return !(*this == rhs); }

    /// Test for inequality with a resource reference.
    bool operator !=(const ResourceRef& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a resource reference list.
    bool operator !=(const ResourceRefList& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a variant vector.
    bool operator !=(const VariantVector& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a string vector.
    bool operator !=(const StringVector& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a variant map.
    bool operator !=(const VariantMap& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a rect.
    bool operator !=(const Rect& rhs) const { return !(*this == rhs); }

    /// Test for inequality with an integer rect.
    bool operator !=(const IntRect& rhs) const { return !(*this == rhs); }

    /// Test for inequality with an IntVector2.
    bool operator !=(const IntVector2& rhs) const { return !(*this == rhs); }

    /// Test for inequality with an IntVector3.
    bool operator !=(const IntVector3& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a StringHash.
    bool operator !=(const StringHash& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a RefCounted pointer.
    bool operator !=(RefCounted* rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Matrix3.
    bool operator !=(const Matrix3& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Matrix3x4.
    bool operator !=(const Matrix3x4& rhs) const { return !(*this == rhs); }

    /// Test for inequality with a Matrix4.
    bool operator !=(const Matrix4& rhs) const { return !(*this == rhs); }

    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const ea::string& type, const ea::string& value);
    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const char* type, const char* value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const ea::string& value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const char* value);
    /// Set buffer type from a memory area.
    void SetBuffer(const void* data, unsigned size);
    /// Set custom value.
    void SetCustomVariantValue(const CustomVariantValue& value);
    /// Set custom value.
    template <class T> void SetCustom(T value)
    {
        // Try to assign value directly
        if (T* thisValue = GetCustomPtr<T>())
        {
            *thisValue = ea::move(value);
            return;
        }

        // Fall back to reallocation
        SetType(VariantType::Custom);
        value_.AsCustomValue().~CustomVariantValue();

        if constexpr (IsCustomTypeOnStack<T>())
            new (value_.storage_) CustomVariantValueImpl<T>(ea::move(value));
        else
            new (value_.storage_) CustomVariantValueImpl<ea::unique_ptr<T>>(ea::make_unique<T>(ea::move(value)));
    }

    /// Return int or zero on type mismatch. Floats and doubles are converted.
    int GetInt() const
    {
        if (type_ == VariantType::Int)
            return value_.int_;
        else if (type_ == VariantType::Float)
            return static_cast<int>(value_.float_);
        else if (type_ == VariantType::Double)
            return static_cast<int>(value_.double_);
        else
            return 0;
    }

    /// Return 64 bit int or zero on type mismatch. Floats and doubles are converted.
    long long GetInt64() const
    {
        if (type_ == VariantType::Int64)
            return value_.int64_;
        else if (type_ == VariantType::Int)
            return value_.int_;
        else if (type_ == VariantType::Float)
            return static_cast<long long>(value_.float_);
        else if (type_ == VariantType::Double)
            return static_cast<long long>(value_.double_);
        else
            return 0;
    }

    /// Return unsigned 64 bit int or zero on type mismatch. Floats and doubles are converted.
    unsigned long long GetUInt64() const
    {
        if (type_ == VariantType::Int64)
            return static_cast<unsigned long long>(value_.int64_);
        else if (type_ == VariantType::Int)
            return static_cast<unsigned long long>(value_.int_);
        else if (type_ == VariantType::Float)
            return static_cast<unsigned long long>(value_.float_);
        else if (type_ == VariantType::Double)
            return static_cast<unsigned long long>(value_.double_);
        else
            return 0;
    }

    /// Return unsigned int or zero on type mismatch. Floats and doubles are converted.
    unsigned GetUInt() const
    {
        if (type_ == VariantType::Int)
            return static_cast<unsigned>(value_.int_);
        else if (type_ == VariantType::Float)
            return static_cast<unsigned>(value_.float_);
        else if (type_ == VariantType::Double)
            return static_cast<unsigned>(value_.double_);
        else
            return 0;
    }

    /// Return StringHash or zero on type mismatch.
    StringHash GetStringHash() const { return StringHash(GetUInt()); }

    /// Return bool or false on type mismatch.
    bool GetBool() const { return type_ == VariantType::Bool ? value_.bool_ : false; }

    /// Return float or zero on type mismatch. Ints and doubles are converted.
    float GetFloat() const
    {
        if (type_ == VariantType::Float)
            return value_.float_;
        else if (type_ == VariantType::Double)
            return static_cast<float>(value_.double_);
        else if (type_ == VariantType::Int)
            return static_cast<float>(value_.int_);
        else if (type_ == VariantType::Int64)
            return static_cast<float>(value_.int64_);
        else
            return 0.0f;
    }

    /// Return double or zero on type mismatch. Ints and floats are converted.
    double GetDouble() const
    {
        if (type_ == VariantType::Double)
            return value_.double_;
        else if (type_ == VariantType::Float)
            return value_.float_;
        else if (type_ == VariantType::Int)
            return static_cast<double>(value_.int_);
        else if (type_ == VariantType::Int64)
            return static_cast<double>(value_.int64_);
        else
            return 0.0;
    }

    /// Return Vector2 or zero on type mismatch.
    const Vector2& GetVector2() const { return type_ == VariantType::Vector2 ? value_.vector2_ : Vector2::ZERO; }

    /// Return Vector3 or zero on type mismatch.
    const Vector3& GetVector3() const { return type_ == VariantType::Vector3 ? value_.vector3_ : Vector3::ZERO; }

    /// Return Vector4 or zero on type mismatch.
    const Vector4& GetVector4() const { return type_ == VariantType::Vector4 ? value_.vector4_ : Vector4::ZERO; }

    /// Return quaternion or identity on type mismatch.
    const Quaternion& GetQuaternion() const
    {
        return type_ == VariantType::Quaternion ? value_.quaternion_ : Quaternion::IDENTITY;
    }

    /// Return color or default on type mismatch. Vector4 is aliased to Color if necessary.
    const Color& GetColor() const { return (type_ == VariantType::Color || type_ == VariantType::Vector4) ? value_.color_ : Color::WHITE; }

    /// Return string or empty on type mismatch.
    const ea::string& GetString() const { return type_ == VariantType::String ? value_.string_ : EMPTY_STRING; }

    /// Return buffer or empty on type mismatch.
    const VariantBuffer& GetBuffer() const
    {
        return type_ == VariantType::Buffer ? value_.buffer_ : emptyBuffer;
    }

    /// Return %VectorBuffer containing the buffer or empty on type mismatch.
    VectorBuffer GetVectorBuffer() const;

    /// Return void pointer or null on type mismatch. RefCounted pointer will be converted.
    void* GetVoidPtr() const
    {
        if (type_ == VariantType::VoidPtr)
            return value_.voidPtr_;
        else if (type_ == VariantType::Ptr)
            return value_.weakPtr_;
        else
            return nullptr;
    }

    /// Return a resource reference or empty on type mismatch.
    const ResourceRef& GetResourceRef() const
    {
        return type_ == VariantType::ResourceRef ? value_.resourceRef_ : emptyResourceRef;
    }

    /// Return a resource reference list or empty on type mismatch.
    const ResourceRefList& GetResourceRefList() const
    {
        return type_ == VariantType::ResourceRefList ? value_.resourceRefList_ : emptyResourceRefList;
    }

    /// Return a variant vector or empty on type mismatch.
    const VariantVector& GetVariantVector() const
    {
        return type_ == VariantType::VariantVector ? value_.variantVector_ : emptyVariantVector;
    }

    /// Return a string vector or empty on type mismatch.
    const StringVector& GetStringVector() const
    {
        return type_ == VariantType::StringVector ? value_.stringVector_ : emptyStringVector;
    }

    /// Return a variant map or empty on type mismatch.
    const VariantMap& GetVariantMap() const
    {
        return type_ == VariantType::VariantMap ? *value_.variantMap_ : emptyVariantMap;
    }

    /// Return a rect or empty on type mismatch.
    const Rect& GetRect() const { return type_ == VariantType::Rect ? value_.rect_ : Rect::ZERO; }

    /// Return an integer rect or empty on type mismatch.
    const IntRect& GetIntRect() const { return type_ == VariantType::IntRect ? value_.intRect_ : IntRect::ZERO; }

    /// Return an IntVector2 or empty on type mismatch.
    const IntVector2& GetIntVector2() const
    {
        return type_ == VariantType::IntVector2 ? value_.intVector2_ : IntVector2::ZERO;
    }

    /// Return an IntVector3 or empty on type mismatch.
    const IntVector3& GetIntVector3() const
    {
        return type_ == VariantType::IntVector3 ? value_.intVector3_ : IntVector3::ZERO;
    }

    /// Return a RefCounted pointer or null on type mismatch. Will return null if holding a void pointer, as it can not be safely verified that the object is a RefCounted.
    RefCounted* GetPtr() const
    {
        return type_ == VariantType::Ptr ? value_.weakPtr_ : nullptr;
    }

    /// Return a Matrix3 or identity on type mismatch.
    const Matrix3& GetMatrix3() const
    {
        return type_ == VariantType::Matrix3 ? *value_.matrix3_ : Matrix3::IDENTITY;
    }

    /// Return a Matrix3x4 or identity on type mismatch.
    const Matrix3x4& GetMatrix3x4() const
    {
        return type_ == VariantType::Matrix3X4 ? *value_.matrix3x4_ : Matrix3x4::IDENTITY;
    }

    /// Return a Matrix4 or identity on type mismatch.
    const Matrix4& GetMatrix4() const
    {
        return type_ == VariantType::Matrix4 ? *value_.matrix4_ : Matrix4::IDENTITY;
    }

    /// Return pointer to custom variant value.
    CustomVariantValue* GetCustomVariantValuePtr()
    {
        return const_cast<CustomVariantValue*>(const_cast<const Variant*>(this)->GetCustomVariantValuePtr());
    }

    /// Return const pointer to custom variant value.
    const CustomVariantValue* GetCustomVariantValuePtr() const
    {
        if (type_ == VariantType::Custom)
            return &value_.AsCustomValue();
        else
            return nullptr;
    }

    /// Return custom variant value or default-constructed on type mismatch.
    template <class T> T GetCustom() const
    {
        if (const T* value = GetCustomPtr<T>())
            return *value;
        return T{};
    }

    /// Return true if specified custom type is stored in the variant.
    template <class T> bool IsCustomType() const
    {
        if (const CustomVariantValue* custom = GetCustomVariantValuePtr())
            return custom->IsType<T>();
        else
            return false;
    }

    /// Return value's type.
    /// @property
    VariantType GetType() const { return type_; }

    /// Return value's type name.
    /// @property
    ea::string GetTypeName() const;
    /// Convert value to string. Pointers are returned as null, and VariantBuffer or VariantMap are not supported and return empty.
    ea::string ToString() const;
    /// Return true when the variant value is considered zero according to its actual type.
    /// @property
    bool IsZero() const;

    /// Return true when the variant is empty (i.e. not initialized yet).
    /// @property
    bool IsEmpty() const { return type_ == VariantType::None; }

    /// Return the value, template version.
    template <class T> T Get() const;

    /// Return a pointer to a modifiable buffer or null on type mismatch.
    VariantBuffer* GetBufferPtr()
    {
        return type_ == VariantType::Buffer ? &value_.buffer_ : nullptr;
    }

    /// Return a pointer to a modifiable variant vector or null on type mismatch.
    VariantVector* GetVariantVectorPtr() { return type_ == VariantType::VariantVector ? &value_.variantVector_ : nullptr; }

    /// Return a pointer to a modifiable string vector or null on type mismatch.
    StringVector* GetStringVectorPtr() { return type_ == VariantType::StringVector ? &value_.stringVector_ : nullptr; }

    /// Return a pointer to a modifiable variant map or null on type mismatch.
    VariantMap* GetVariantMapPtr() { return type_ == VariantType::VariantMap ? value_.variantMap_ : nullptr; }

    /// Return a pointer to a modifiable custom variant value or null on type mismatch.
    template <class T> T* GetCustomPtr() { return const_cast<T*>(const_cast<const Variant*>(this)->GetCustomPtr<T>()); }

    /// Return a pointer to a constant custom variant value or null on type mismatch.
    template <class T> const T* GetCustomPtr() const
    {
        if (const CustomVariantValue* value = GetCustomVariantValuePtr())
        {
            if constexpr (IsCustomTypeOnStack<T>())
            {
                return value->GetValuePtr<T>();
            }
            else
            {
                if (auto valuePtrPtr = value->GetValuePtr<ea::unique_ptr<T>>())
                    return valuePtrPtr->get();
            }
        }
        return nullptr;
    }

    /// Hash function for containers.
    unsigned ToHash() const;

    /// Return type name list.
    static const char* const* GetTypeNameList();
    /// Return name for variant type.
    static ea::string GetTypeName(VariantType type);
    /// Return variant type from type name.
    static VariantType GetTypeFromName(const ea::string& typeName);
    /// Return variant type from type name.
    static VariantType GetTypeFromName(const char* typeName);

    /// Empty variant.
    static const Variant EMPTY;
    /// Empty buffer.
    static const VariantBuffer emptyBuffer;
    /// Empty resource reference.
    static const ResourceRef emptyResourceRef;
    /// Empty resource reference list.
    static const ResourceRefList emptyResourceRefList;
    /// Empty variant map.
    static const VariantMap emptyVariantMap;
    /// Empty variant vector.
    static const VariantVector emptyVariantVector;
    /// Empty string vector.
    static const StringVector emptyStringVector;

private:
    /// Set new type and allocate/deallocate memory as necessary.
    void SetType(VariantType newType);

    /// Variant type.
    VariantType type_ = VariantType::None;
    /// Variant value.
    VariantValue value_;
};

/// Make custom variant value.
template <typename T> Variant MakeCustomValue(const T& value)
{
    Variant var;
    var.SetCustom(value);
    return var;
}

/// Return variant type from type.
template <typename T> VariantType GetVariantType();

// Return variant type from concrete types
template <> inline VariantType GetVariantType<int>() { return VariantType::Int; }

template <> inline VariantType GetVariantType<unsigned>() { return VariantType::Int; }

template <> inline VariantType GetVariantType<long long>() { return VariantType::Int64; }

template <> inline VariantType GetVariantType<unsigned long long>() { return VariantType::Int64; }

template <> inline VariantType GetVariantType<bool>() { return VariantType::Bool; }

template <> inline VariantType GetVariantType<float>() { return VariantType::Float; }

template <> inline VariantType GetVariantType<double>() { return VariantType::Double; }

template <> inline VariantType GetVariantType<Vector2>() { return VariantType::Vector2; }

template <> inline VariantType GetVariantType<Vector3>() { return VariantType::Vector3; }

template <> inline VariantType GetVariantType<Vector4>() { return VariantType::Vector4; }

template <> inline VariantType GetVariantType<Quaternion>() { return VariantType::Quaternion; }

template <> inline VariantType GetVariantType<Color>() { return VariantType::Color; }

template <> inline VariantType GetVariantType<ea::string>() { return VariantType::String; }

template <> inline VariantType GetVariantType<StringHash>() { return VariantType::Int; }

template <> inline VariantType GetVariantType<VariantBuffer >() { return VariantType::Buffer; }

template <> inline VariantType GetVariantType<ResourceRef>() { return VariantType::ResourceRef; }

template <> inline VariantType GetVariantType<ResourceRefList>() { return VariantType::ResourceRefList; }

template <> inline VariantType GetVariantType<VariantVector>() { return VariantType::VariantVector; }

template <> inline VariantType GetVariantType<StringVector>() { return VariantType::StringVector; }

template <> inline VariantType GetVariantType<VariantMap>() { return VariantType::VariantMap; }

template <> inline VariantType GetVariantType<Rect>() { return VariantType::Rect; }

template <> inline VariantType GetVariantType<IntRect>() { return VariantType::IntRect; }

template <> inline VariantType GetVariantType<IntVector2>() { return VariantType::IntVector2; }

template <> inline VariantType GetVariantType<IntVector3>() { return VariantType::IntVector3; }

template <> inline VariantType GetVariantType<Matrix3>() { return VariantType::Matrix3; }

template <> inline VariantType GetVariantType<Matrix3x4>() { return VariantType::Matrix3X4; }

template <> inline VariantType GetVariantType<Matrix4>() { return VariantType::Matrix4; }

// Specializations of Variant::Get<T>
template <> URHO3D_API int Variant::Get<int>() const;

template <> URHO3D_API unsigned Variant::Get<unsigned>() const;

template <> URHO3D_API long long Variant::Get<long long>() const;

template <> URHO3D_API unsigned long long Variant::Get<unsigned long long>() const;

template <> URHO3D_API StringHash Variant::Get<StringHash>() const;

template <> URHO3D_API bool Variant::Get<bool>() const;

template <> URHO3D_API float Variant::Get<float>() const;

template <> URHO3D_API double Variant::Get<double>() const;

template <> URHO3D_API const Vector2& Variant::Get<const Vector2&>() const;

template <> URHO3D_API const Vector3& Variant::Get<const Vector3&>() const;

template <> URHO3D_API const Vector4& Variant::Get<const Vector4&>() const;

template <> URHO3D_API const Quaternion& Variant::Get<const Quaternion&>() const;

template <> URHO3D_API const Color& Variant::Get<const Color&>() const;

template <> URHO3D_API const ea::string& Variant::Get<const ea::string&>() const;

template <> URHO3D_API const Rect& Variant::Get<const Rect&>() const;

template <> URHO3D_API const IntRect& Variant::Get<const IntRect&>() const;

template <> URHO3D_API const IntVector2& Variant::Get<const IntVector2&>() const;

template <> URHO3D_API const IntVector3& Variant::Get<const IntVector3&>() const;

template <> URHO3D_API const VariantBuffer& Variant::Get<const VariantBuffer&>() const;

template <> URHO3D_API void* Variant::Get<void*>() const;

template <> URHO3D_API RefCounted* Variant::Get<RefCounted*>() const;

template <> URHO3D_API const Matrix3& Variant::Get<const Matrix3&>() const;

template <> URHO3D_API const Matrix3x4& Variant::Get<const Matrix3x4&>() const;

template <> URHO3D_API const Matrix4& Variant::Get<const Matrix4&>() const;

template <> URHO3D_API ResourceRef Variant::Get<ResourceRef>() const;

template <> URHO3D_API ResourceRefList Variant::Get<ResourceRefList>() const;

template <> URHO3D_API VariantVector Variant::Get<VariantVector>() const;

template <> URHO3D_API StringVector Variant::Get<StringVector>() const;

template <> URHO3D_API VariantMap Variant::Get<VariantMap>() const;

template <> URHO3D_API Vector2 Variant::Get<Vector2>() const;

template <> URHO3D_API Vector3 Variant::Get<Vector3>() const;

template <> URHO3D_API Vector4 Variant::Get<Vector4>() const;

template <> URHO3D_API Quaternion Variant::Get<Quaternion>() const;

template <> URHO3D_API Color Variant::Get<Color>() const;

template <> URHO3D_API ea::string Variant::Get<ea::string>() const;

template <> URHO3D_API Rect Variant::Get<Rect>() const;

template <> URHO3D_API IntRect Variant::Get<IntRect>() const;

template <> URHO3D_API IntVector2 Variant::Get<IntVector2>() const;

template <> URHO3D_API IntVector3 Variant::Get<IntVector3>() const;

template <> URHO3D_API VariantBuffer Variant::Get<VariantBuffer >() const;

template <> URHO3D_API Matrix3 Variant::Get<Matrix3>() const;

template <> URHO3D_API Matrix3x4 Variant::Get<Matrix3x4>() const;

template <> URHO3D_API Matrix4 Variant::Get<Matrix4>() const;

// Implementations
template <class T> const T* CustomVariantValue::GetValuePtr() const
{
    if (IsType<T>())
    {
        auto impl = static_cast<const CustomVariantValueImpl<T>*>(this);
        return &impl->GetValue();
    }
    return nullptr;
}

}
