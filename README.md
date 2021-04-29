![rbfx-logo](https://user-images.githubusercontent.com/19151258/57008846-a292be00-6bfb-11e9-8303-d79e6dd36038.png)

(TEST COMMIT)

[![Build Status](https://github.com/rokups/rbfx/workflows/Build/badge.svg)](https://github.com/rokups/rbfx/actions)
[![Join the chat at https://gitter.im/rokups/rbfx](https://badges.gitter.im/rokups/rbfx.svg)](https://gitter.im/rokups/rbfx?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Discord Chat](https://img.shields.io/discord/560082228928053258.svg?logo=discord)](https://discord.gg/XKs73yf)

**Urho3D Rebel Fork** aka **rbfx** is an experimental fork of [Urho3D](http://urho3d.github.io/) game engine.

[GitHub Wiki](https://github.com/rokups/rbfx/wiki) contains fork-specific documentation including [build instructions](https://github.com/rokups/rbfx/wiki/first-application) and differences between original project and **rbfx**.

[Doxygen Documentation](https://rbfx.github.io/) is cloned from original project and is not updated (yet?), although it still contains relevant information regarding core functionality of the engine.

Note that **rbfx** is undergoing active developement. Please report all spotted bugs in the [issue tracker](https://github.com/rokups/rbfx/issues).

## License

Licensed under the MIT license, see [LICENSE](https://github.com/urho3d/Urho3D/blob/master/LICENSE) for details.

## Features overview

* Multiple rendering API support
  * Direct3D9
  * Direct3D11
  * OpenGL 2.0 or 3.2
  * OpenGL ES 2.0 or 3.0
  * WebGL
* HLSL or GLSL shaders + caching of HLSL bytecode
* Configurable rendering pipeline with default implementations of
  * Forward
  * Light pre-pass
  * Deferred
* Component based scene model
* Skeletal (with hardware skinning), vertex morph and node animation
* Automatic instancing on SM3 capable hardware
* Point, spot and directional lights
* Shadow mapping for all light types
  * Cascaded shadow maps for directional lights
  * Normal offset adjustment in addition to depth bias
* Particle rendering
* Geomipmapped terrain
* Static and skinned decals
* Auxiliary view rendering (reflections etc.)
* Geometry, material & animation LOD
* Software rasterized occlusion culling
* Post-processing
* HDR rendering and PBR rendering
* 2D sprites and particles that integrate into the 3D scene
* Task-based multithreading
* Hierarchical performance profiler
* Scene and object load/save in binary, XML and JSON formats
* Keyframe animation of object attributes
* Background loading of resources
* Keyboard, mouse, joystick and touch input (if available)
* Cross-platform support using SDL 2.0
* Physics using Bullet
* 2D physics using Box2D
* Scripting using C#
* Networking using SLikeNet + possibility to make HTTP requests
* Pathfinding and crowd simulation using Recast/Detour
* Inverse kinematics
* Image loading using stb_image + DDS / KTX / PVR compressed texture support + WEBP image format support
* 2D and “3D” audio playback, Ogg Vorbis support using stb_vorbis + WAV format support
* TrueType font rendering using FreeType
* Unicode string support
* Inbuilt UI based on subset of html and css, localization
* WYSIWYG scene editor editor with undo & redo capabilities and hot code reload
* Model/scene/animation/material import from formats supported by Open Asset Import Library
* Alternative model/animation import from OGRE mesh.xml and skeleton.xml files
* Supported IDEs: Visual Studio, Xcode, Eclipse, CodeBlocks, CodeLite, QtCreator, CLion
* Supported compiler toolchains: MSVC, GCC, Clang, MinGW, and their cross-compiling derivatives
* Supports both 32-bit and 64-bit build
* Build as single external library (can be linked against statically or dynamically)
* Dear ImGui integration used in tools

## Screenshots

![editor](https://user-images.githubusercontent.com/19151258/49943614-09376980-fef1-11e8-88fe-8c26fcf30a59.jpg)

## Dependencies

rbfx uses the following third-party libraries:
- Box2D 2.3.2 WIP (http://box2d.org)
- Bullet 3.06+ (http://www.bulletphysics.org)
- Civetweb 1.7 (https://github.com/civetweb/civetweb)
- FreeType 2.8 (https://www.freetype.org)
- GLEW 2.1 (http://glew.sourceforge.net)
- SLikeNet (https://github.com/SLikeSoft/SLikeNet)
- libcpuid 0.4.0+ (https://github.com/anrieff/libcpuid)
- LZ4 1.7.5 (https://github.com/lz4/lz4)
- MojoShader (https://icculus.org/mojoshader)
- Open Asset Import Library 4.1.0 (http://assimp.sourceforge.net)
- pugixml 1.10 (http://pugixml.org)
- rapidjson 1.1.0 (https://github.com/miloyip/rapidjson)
- Recast/Detour (https://github.com/recastnavigation/recastnavigation)
- SDL 2.0.10+ (https://www.libsdl.org)
- StanHull (https://codesuppository.blogspot.com/2006/03/john-ratcliffs-code-suppository-blog.html)
- stb_image 2.18 (https://nothings.org)
- stb_image_write 1.08 (https://nothings.org)
- stb_rect_pack 0.11 (https://nothings.org)
- stb_textedit 1.9 (https://nothings.org)
- stb_truetype 1.15 (https://nothings.org)
- stb_vorbis 1.13b (https://nothings.org)
- WebP (https://chromium.googlesource.com/webm/libwebp)
- ETCPACK (https://github.com/Ericsson/ETCPACK)
- Dear ImGui 1.75+ (https://github.com/ocornut/imgui/tree/docking)
- tracy (https://bitbucket.org/wolfpld/tracy/)
- capstone (http://www.capstone-engine.org/)
- nativefiledialog (https://github.com/mlabbe/nativefiledialog)
- IconFontCppHeaders (https://github.com/juliettef/IconFontCppHeaders)
- ImGuizmo 1.61 (https://github.com/CedricGuillemet/ImGuizmo)
- crunch (https://github.com/Unity-Technologies/crunch/)
- CLI11 v1.5.1 (https://github.com/CLIUtils/CLI11/)
- fmt 6.0.0 (http://fmtlib.net/)
- spdlog 1.4.2 (https://github.com/gabime/spdlog/)
- EASTL 3.17.06 (https://github.com/electronicarts/EASTL/)
- SWIG 4.0 (http://swig.org/)
- Embree 3.11 (https://github.com/embree/embree)
- RmlUi (https://github.com/mikke89/RmlUi)

rbfx optionally uses the following external third-party libraries:
- Mono (http://www.mono-project.com/download/stable/)

## Supported Platforms

| Graphics API/Platform | Windows | UWP | Linux | MacOS | iOS | Android | Web |
| --------------------- |:-------:|:---:|:-----:|:-----:|:---:|:-------:|:---:|
| D3D9                  | ✔       |     |       |       |     |         |     |
| D3D11                 | ✔       | ✔   |       |       |     |         |     |
| OpenGL 2 / 3.1        | ✔       |     | ✔     | ✔     |     |         |     |
| OpenGL ES 2 / 3       |         |     |       |       | ✔   | ✔       |     |
| WebGL                 |         |     |       |       |     |         | ✔   |
