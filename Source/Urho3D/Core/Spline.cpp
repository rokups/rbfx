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

#include "../Core/Spline.h"
#include "../IO/Log.h"

namespace Urho3D
{

const char* interpolationModeNames[] =
{
    "Bezier",
    "Catmull-Rom",
    "Linear",
    "Catmull-Rom Full",
    nullptr
};

Spline::Spline() :
    interpolationMode_(InterpolationMode::BezierCurve)
{
}

Spline::Spline(InterpolationMode mode) :
    interpolationMode_(mode)
{
}

Spline::Spline(const ea::vector<Variant>& knots, InterpolationMode mode) :
    interpolationMode_(mode),
    knots_(knots)
{
}

Variant Spline::GetPoint(float f) const
{
    if (knots_.size() < 2)
        return knots_.size() == 1 ? knots_[0] : Variant::EMPTY;

    if (f > 1.f)
        f = 1.f;
    else if (f < 0.f)
        f = 0.f;

    switch (interpolationMode_)
    {
    case InterpolationMode::BezierCurve:
        return BezierInterpolation(knots_, f);
    case InterpolationMode::CatmullRomCurve:
        return CatmullRomInterpolation(knots_, f);
    case InterpolationMode::LinearCurve:
        return LinearInterpolation(knots_, f);
    case InterpolationMode::CatmullRomFullCurve:
        {
            /// \todo Do not allocate a new vector each time
            ea::vector<Variant> fullKnots;
            if (knots_.size() > 1)
            {
                // Non-cyclic case: duplicate start and end
                if (knots_.front() != knots_.back())
                {
                    fullKnots.push_back(knots_.front());
                    fullKnots.push_back(knots_);
                    fullKnots.push_back(knots_.back());
                }
                // Cyclic case: smooth the tangents
                else
                {
                    fullKnots.push_back(knots_[knots_.size() - 2]);
                    fullKnots.push_back(knots_);
                    fullKnots.push_back(knots_[1]);
                }
            }
            return CatmullRomInterpolation(fullKnots, f);
        }

    default:
        URHO3D_LOGERROR("Unsupported interpolation mode");
        return Variant::EMPTY;
    }
}

void Spline::SetKnot(const Variant& knot, unsigned index)
{
    if (index < knots_.size())
    {
        if (knots_.size() > 0 && knots_[0].GetType() == knot.GetType())
            knots_[index] = knot;
        else if (knots_.empty())
            knots_.push_back(knot);
        else
            URHO3D_LOGERRORF("Attempted to set a Spline's Knot value of type %s where elements are already using %s",
                knot.GetTypeName().c_str(), knots_[0].GetTypeName().c_str());
    }
}

void Spline::AddKnot(const Variant& knot)
{
    if (knots_.size() > 0 && knots_[0].GetType() == knot.GetType())
        knots_.push_back(knot);
    else if (knots_.empty())
        knots_.push_back(knot);
    else
        URHO3D_LOGERRORF("Attempted to add Knot to Spline of type %s where elements are already using %s", knot.GetTypeName().c_str(),
            knots_[0].GetTypeName().c_str());
}

void Spline::AddKnot(const Variant& knot, unsigned index)
{
    if (index > knots_.size())
        index = knots_.size();

    if (knots_.size() > 0 && knots_[0].GetType() == knot.GetType())
        knots_.insert_at(index, knot);
    else if (knots_.empty())
        knots_.push_back(knot);
    else
        URHO3D_LOGERRORF("Attempted to add Knot to Spline of type %s where elements are already using %s", knot.GetTypeName().c_str(),
            knots_[0].GetTypeName().c_str());
}

Variant Spline::BezierInterpolation(const ea::vector<Variant>& knots, float t) const
{
    if (knots.size() == 2)
    {
        switch (knots[0].GetType())
        {
        case VariantType::Float:
        case VariantType::Vector2:
        case VariantType::Vector3:
        case VariantType::Vector4:
        case VariantType::Color:
        case VariantType::Double:
            return LinearInterpolation(knots[0], knots[1], t);
        default:
            return Variant::EMPTY;
        }
    }
    else
    {
        /// \todo Do not allocate a new vector each time
        ea::vector<Variant> interpolatedKnots;
        for (unsigned i = 1; i < knots.size(); i++)
        {
            switch (knots[0].GetType())
            {
            case VariantType::Float:
            case VariantType::Vector2:
            case VariantType::Vector3:
            case VariantType::Vector4:
            case VariantType::Color:
            case VariantType::Double:
                interpolatedKnots.push_back(LinearInterpolation(knots[i - 1], knots[i], t));
                break;
            default:
                return Variant::EMPTY;
            }
        }
        return BezierInterpolation(interpolatedKnots, t);
    }
}

template <typename T> Variant CalculateCatmullRom(const T& p0, const T& p1, const T& p2, const T& p3, float t, float t2, float t3)
{
    return Variant(0.5f * ((2.0f * p1) + (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
}

Variant Spline::CatmullRomInterpolation(const ea::vector<Variant>& knots, float t) const
{
    if (knots.size() < 4)
        return Variant::EMPTY;
    else
    {
        if (t >= 1.f)
            return knots[knots.size() - 2];

        auto originIndex = static_cast<int>(t * (knots.size() - 3));
        t = fmodf(t * (knots.size() - 3), 1.f);
        float t2 = t * t;
        float t3 = t2 * t;

        switch (knots[originIndex].GetType())
        {
        case VariantType::Float:
            return CalculateCatmullRom(knots[originIndex].GetFloat(), knots[originIndex + 1].GetFloat(),
                knots[originIndex + 2].GetFloat(), knots[originIndex + 3].GetFloat(), t, t2, t3);
        case VariantType::Vector2:
            return CalculateCatmullRom(knots[originIndex].GetVector2(), knots[originIndex + 1].GetVector2(),
                knots[originIndex + 2].GetVector2(), knots[originIndex + 3].GetVector2(), t, t2, t3);
        case VariantType::Vector3:
            return CalculateCatmullRom(knots[originIndex].GetVector3(), knots[originIndex + 1].GetVector3(),
                knots[originIndex + 2].GetVector3(), knots[originIndex + 3].GetVector3(), t, t2, t3);
        case VariantType::Vector4:
            return CalculateCatmullRom(knots[originIndex].GetVector4(), knots[originIndex + 1].GetVector4(),
                knots[originIndex + 2].GetVector4(), knots[originIndex + 3].GetVector4(), t, t2, t3);
        case VariantType::Color:
            return CalculateCatmullRom(knots[originIndex].GetColor(), knots[originIndex + 1].GetColor(),
                knots[originIndex + 2].GetColor(), knots[originIndex + 3].GetColor(), t, t2, t3);
        case VariantType::Double:
            return CalculateCatmullRom(knots[originIndex].GetDouble(), knots[originIndex + 1].GetDouble(),
                knots[originIndex + 2].GetDouble(), knots[originIndex + 3].GetDouble(), t, t2, t3);
        default:
            return Variant::EMPTY;
        }
    }
}

Variant Spline::LinearInterpolation(const ea::vector<Variant>& knots, float t) const
{
    if (knots.size() < 2)
        return Variant::EMPTY;
    else
    {
        if (t >= 1.f)
            return knots.back();

        int originIndex = Clamp((int)(t * (knots.size() - 1)), 0, (int)(knots.size() - 2));
        t = fmodf(t * (knots.size() - 1), 1.f);
        return LinearInterpolation(knots[originIndex], knots[originIndex + 1], t);
    }
}

Variant Spline::LinearInterpolation(const Variant& lhs, const Variant& rhs, float t) const
{
    switch (lhs.GetType())
    {
    case VariantType::Float:
        return Lerp(lhs.GetFloat(), rhs.GetFloat(), t);
    case VariantType::Vector2:
        return lhs.GetVector2().Lerp(rhs.GetVector2(), t);
    case VariantType::Vector3:
        return lhs.GetVector3().Lerp(rhs.GetVector3(), t);
    case VariantType::Vector4:
        return lhs.GetVector4().Lerp(rhs.GetVector4(), t);
    case VariantType::Color:
        return lhs.GetColor().Lerp(rhs.GetColor(), t);
    case VariantType::Double:
        return Lerp(lhs.GetDouble(), rhs.GetDouble(), t);
    default:
        return Variant::EMPTY;
    }
}

}
