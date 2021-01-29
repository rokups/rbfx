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

#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include "Pipeline/Asset.h"
#include "Pipeline/Importers/TextureImporter.h"

namespace Urho3D
{

const char* TextureImporter::mipModeNames[] = {
    "None",
    "Generate",
    "UseSourceOrGenerate",
    "UseSource",
    nullptr
};

const char* TextureImporter::mipFilterNames[] = {
    "Box",
    "Tent",
    "Lanczos4",
    "Mitchell",
    "Kaiser",
    nullptr
};

const char* TextureImporter::compressorNames[] = {
    "CRN",
    "CRNF",
    "RYG",
    "ATI",
    nullptr
};

const char* TextureImporter::dxtQualityNames[] = {
    "Superfast",
    "Fast",
    "Normal",
    "Better",
    "Uber",
    nullptr
};

const char* TextureImporter::pixelFormatNames[] = {
    "None",
    "DXT1",
    "DXT2",
    "DXT3",
    "DXT4",
    "DXT5",
    "3DC",
    "DXN",
    "DXT5A",
    "DXT5_CCxY",
    "DXT5_xGxR",
    "DXT5_xGBR",
    "DXT5_AGBR",
    "DXT1A",
    "ETC1",
    "ETC2",
    "ETC2A",
    "R8G8B8",
    "L8",
    "A8",
    "A8L8",
    "A8R8G8B8",
    nullptr
};

TextureImporter::TextureImporter(Context* context)
    : AssetImporter(context)
{
    flags_ = AssetImporterFlag::IsOptional | AssetImporterFlag::IsRemapped;
}

void TextureImporter::RegisterObject(Context* context)
{
    context->RegisterFactory<TextureImporter>();
    URHO3D_COPY_BASE_ATTRIBUTES(AssetImporter);
    URHO3D_ATTRIBUTE("Y-flip", bool, yFlip_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Un-flip", bool, unflip_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE_EX("Quality", int, quality_, ApplyQualityLimits, 255u, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Bitrate", int, bitrate_, 0, AttributeMode::Default);
    URHO3D_ENUM_ATTRIBUTE("Mip Mode", mipMode_, mipModeNames, MipMode::Generate, AttributeMode::Default);
    URHO3D_ENUM_ATTRIBUTE("Mip Filter", mipFilter_, mipFilterNames, MipFilter::Kaiser, AttributeMode::Default);
    URHO3D_ATTRIBUTE_EX("Max Mips", int, maxMips_, ApplyMipsLimits, 16u, AttributeMode::Default);
    URHO3D_ATTRIBUTE_EX("Min Mip Size", int, minMipSize_, ApplyMipsLimits, 1u, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Gamma", float, gamma_, 2.2f, AttributeMode::Default);
    URHO3D_ATTRIBUTE_EX("Blur", float, blur_, ApplyBlurLimit, 0.9f, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Wrap", bool, wrap_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Renormalize", bool, renormalize_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE_EX("Alpha Threshold", int, alphaThreshold_, ApplyAlphaThresholdLimits, 128u, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Uniform Metircs", bool, uniformMetrics_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Adaptive Blocks", bool, adaptiveBlocks_, true, AttributeMode::Default);
    URHO3D_ENUM_ATTRIBUTE("Compressor", compressor_, compressorNames, Compressor::CRN, AttributeMode::Default);
    URHO3D_ENUM_ATTRIBUTE("DXT Quality", dxtQuality_, dxtQualityNames, DxtQuality::Uber, AttributeMode::Default);
    URHO3D_ATTRIBUTE("No Endpoint Caching", bool, noEndpointCaching_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Greyscale Sampling", bool, grayscaleSampling_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Force Primary Encoding", bool, forcePrimaryEncoding_, false, AttributeMode::Default);
    URHO3D_ATTRIBUTE("Use Transparent Indices For Black", bool, useTransparentIndicesForBlack_, false, AttributeMode::Default);
    URHO3D_ENUM_ATTRIBUTE("Pixel Format", pixelFormat_, pixelFormatNames, PixelFormat::None, AttributeMode::Default);
}

bool TextureImporter::Accepts(const ea::string& path) const
{
    return path.ends_with(".png");
}

bool TextureImporter::Execute(Urho3D::Asset* input, const ea::string& outputPath)
{
    if (!BaseClassName::Execute(input, outputPath))
        return false;

    ea::string outputDirectory = outputPath + GetPath(input->GetName());
    ea::string outputFile = outputDirectory + GetFileName(input->GetName()) + ".dds";
    int pixelFormatValue = GetAttribute("Pixel Format").GetInt();

    if (pixelFormatValue == (int)PixelFormat::None)
        return false;
    else
        context_->GetSubsystem<FileSystem>()->CreateDirsRecursive(outputDirectory);

    ea::string output;
    StringVector arguments{
        "-fileformat", "dds", "-noprogress", "-nostats", "-quality", ea::to_string(GetAttribute("Quality").GetInt()),
        "-gamma", Format("{:.2f}", GetAttribute("Gamma").GetFloat()),
        "-blurriness", Format("{:.2f}", GetAttribute("Blur").GetFloat()),
        "-alphaThreshold", ea::to_string(GetAttribute("Alpha Threshold").GetInt()),
    };

    if (GetAttribute("Y-flip").GetBool())
        arguments.push_back("-yflip");
    if (GetAttribute("Un-flip").GetBool())
        arguments.push_back("-unflip");
    if (int bitrate = GetAttribute("Bitrate").GetInt())
    {
        arguments.push_back("-bitrate");
        arguments.push_back(ea::to_string(bitrate));
    }

    int mipMode = GetAttribute("Mip Mode").GetInt();
    arguments.push_back("-mipMode");
    arguments.push_back(mipModeNames[mipMode]);

    arguments.push_back("-mipFilter");
    arguments.push_back(ea::string(mipFilterNames[GetAttribute("Mip Filter").GetInt()]).to_lower());

    if (GetAttribute("Wrap").GetBool())
        arguments.push_back("-wrap");
    if (GetAttribute("Renormalize").GetBool())
        arguments.push_back("-renormalize");
    if (mipMode == (int)MipMode::Generate || mipMode == (int)MipMode::UseSourceOrGenerate)
    {
        arguments.push_back("-maxmips");
        arguments.push_back(ea::to_string(GetAttribute("Max Mips").GetInt()));
        arguments.push_back("-minmipsize");
        arguments.push_back(ea::to_string(GetAttribute("Min Mip Size").GetInt()));
    }

    if (GetAttribute("Uniform Metircs").GetBool())
        arguments.push_back("-uniformMetrics");
    if (!GetAttribute("Adaptive Blocks").GetBool())
        arguments.push_back("-noAdaptiveBlocks");

    arguments.push_back("-compressor");
    arguments.push_back(compressorNames[GetAttribute("Compressor").GetInt()]);

    arguments.push_back("-dxtQuality");
    arguments.push_back(ea::string(dxtQualityNames[GetAttribute("DXT Quality").GetInt()]).to_lower());

    if (GetAttribute("No Endpoint Caching").GetBool())
        arguments.push_back("-noendpointcaching");
    if (GetAttribute("Greyscale Sampling").GetBool())
        arguments.push_back("-grayscalsampling");
    if (GetAttribute("Force Primary Encoding").GetBool())
        arguments.push_back("-forceprimaryencoding");
    if (GetAttribute("Use Transparent Indices For Black").GetBool())
        arguments.push_back("-usetransparentindicesforblack");

    ea::string pixelFormat = pixelFormatNames[pixelFormatValue];
    arguments.push_back(Format("-{}", pixelFormat));

    arguments.push_back("-out");
    arguments.push_back(outputFile);
    arguments.push_back("-file");
    arguments.push_back(input->GetResourcePath());

    int result = context_->GetSubsystem<FileSystem>()->SystemRun(context_->GetSubsystem<FileSystem>()->GetProgramDir() + "/crunch", arguments, output);
    if (result != 0)
    {
        logger_.Error("Error {}-compressing 'res://{}' to '{}' failed.", pixelFormat, input->GetName(), outputFile);
        if (!output.empty())
            URHO3D_LOGERROR(output);
        return false;
    }

    AddByproduct(outputFile);
    return true;
}

void TextureImporter::ApplyBlurLimit()
{
    if (blur_ < 0.01f)
        SetAttribute("Blur", 0.01f);
    else if (blur_ > 8.f)
        SetAttribute("Blur", 8.f);
}

void TextureImporter::ApplyMipsLimits()
{
    auto maxMips = Clamp<uint8_t>(maxMips_, 1, 16);
    auto minMips = Clamp<uint8_t>(minMipSize_, 1, 16);

    if (maxMips < minMips)
        minMips = maxMips;

    if (maxMips != maxMips_)
        SetAttribute("Max Mips", maxMips);

    if (minMips != minMipSize_)
        SetAttribute("Min Mip Size", minMips);
}

void TextureImporter::ApplyQualityLimits()
{
    int quality = Clamp<int>(quality_, 0, 255);
    if (quality_ != quality)
        SetAttribute("Quality", quality);
}

void TextureImporter::ApplyAlphaThresholdLimits()
{
    int quality = Clamp<int>(quality_, 0, 255);
    if (quality_ != quality)
        SetAttribute("Alpha Threshold", quality);
}

}
