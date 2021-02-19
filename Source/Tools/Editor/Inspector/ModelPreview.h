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

#include <Urho3D/Core/Object.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Toolbox/Graphics/SceneView.h>

namespace Urho3D
{

class Model;

/// Renders a model preview in attribute inspector.
class ModelPreview : public Object
{
    URHO3D_OBJECT(ModelPreview, Object);
public:
    /// Construct.
    explicit ModelPreview(Context* context);

    /// Copy effects from specified render path.
    void SetEffectSource(RenderPath* renderPath);
    /// Set preview model by passing a resource name.
    void SetModel(const ea::string& resourceName);
    /// Set preview model by passing model resource instance.
    void SetModel(Model* model);
    /// Set preview material by passing a resource name.
    void SetMaterial(const ea::string& resourceName, int index = 0);
    /// Set preview material by passing material resource instance.
    void SetMaterial(Material* material, int index = 0);
    /// Get preview material resource instance.
    Material* GetMaterial(int index);
    /// Change material preview model to next one in the list (sphere/box/torus/teapot). If custom model was set it will be reset.
    void ToggleModel();
    /// Render model preview.
    void RenderPreview();
    /// Render model preview with custom size.
    void RenderPreview(float width, float height);

protected:
    /// Initialize preview.
    void CreateObjects();

    /// Preview scene.
    SceneView view_;
    /// Node holding figure to which material is applied.
    WeakPtr<Node> node_;
    /// Flag indicating if this widget grabbed mouse for rotating material node.
    bool mouseGrabbed_ = false;
    /// Distance from camera to figure.
    float distance_ = 1.5f;
    /// Index of current figure displaying material.
    unsigned figureIndex_ = 0;
    /// A list of figures between which material view can be toggled.
    ea::vector<const char*> figures_{"Sphere", "Box", "Torus", "TeaPot"};
};

}
