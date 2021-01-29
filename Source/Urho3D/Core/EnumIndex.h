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

#pragma once

#include <type_traits>

namespace Urho3D
{

#ifndef URHO3D_ENUM_INDEX
/// Helper macro that indicate that enumeration is sequentially indexed.
/// The following expressions become well-formed:
/// ++enum: Increments enum value
/// +enum: Converts enum value to underlying integer
#define URHO3D_ENUM_INDEX(Enum) \
    inline typename std::underlying_type<Enum>::type operator + (const Enum lhs) \
    { \
        return static_cast<typename std::underlying_type<Enum>::type>(lhs); \
    } \
    inline Enum operator ++ (const Enum lhs) \
    { \
        return static_cast<Enum>(+lhs + 1); \
    }
#endif

#ifndef URHO3D_ENUM_BOOL
/// Helper macro that indicate that enumeration has zero value with "false" or "invalid" semantics.
/// The following expression become well-formed:
/// !enum: Check if enum is zero
/// !!enum: Check if enum is not zero
#define URHO3D_ENUM_BOOL(Enum) \
    inline bool operator ! (const Enum lhs) { return lhs == static_cast<Enum>(0); }
#endif

}
