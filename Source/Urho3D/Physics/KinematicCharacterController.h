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

#pragma once

#include "../Scene/Component.h"

class btPairCachingGhostObject;
class btKinematicCharacterController;

namespace Urho3D
{
class PhysicsWorld;
class DebugRenderer;

class URHO3D_API KinematicCharacterController : public Component
{
    URHO3D_OBJECT(KinematicCharacterController, Component);

public:
    /// Construct.
    KinematicCharacterController(Context* context);
    /// Destruct.
    ~KinematicCharacterController() override;

    /// Register object factory and attributes.
    static void RegisterObject(Context* context);
    void OnSetAttribute(const AttributeInfo& attr, const Variant& src) override;

    /// Perform post-load after deserialization. Acquire the components from the scene nodes.
    void ApplyAttributes() override;

    /// Return character position in world space.
    Vector3 GetPosition() const;
    /// Return character rotation in world space.
    Quaternion GetRotation() const;
    /// Set character position and rotation in world space as an atomic operation.
    void SetTransform(const Vector3& position, const Quaternion& rotation);
    /// Get character position and rotation in world space.
    void GetTransform(Vector3& position, Quaternion& rotation) const;

    /// Set collision layer.
    void SetCollisionLayer(unsigned layer);
    /// Return collision layer.
    unsigned GetCollisionLayer() const { return colLayer_; }
    /// Set collision mask.
    void SetCollisionMask(unsigned mask);
    /// Return collision mask.
    unsigned GetCollisionMask() const { return colMask_; }
    /// Set collision group and mask.
    void SetCollisionLayerAndMask(unsigned layer, unsigned mask);

    /// Set gravity.
    void SetGravity(const Vector3 &gravity);
    /// Get gravity.
    const Vector3& GetGravity() const { return gravity_; }
    /// Set linear velocity damping factor.
    void SetLinearDamping(float linearDamping);
    /// Return linear velocity damping factor.
    float GetLinearDamping() const { return linearDamping_; }
    /// Set angular velocity damping factor.
    void SetAngularDamping(float angularDamping);
    /// Return linear velocity damping factor.
    float GetAngularDamping() const { return angularDamping_; }
    /// Set step height.
    void SetStepHeight(float stepHeight);
    /// Return step height.
    float GetStepHeight() const { return stepHeight_; }
    /// Set max jump height.
    void SetMaxJumpHeight(float maxJumpHeight);
    /// Return max jump height.
    float GetMaxJumpHeight() const { return maxJumpHeight_; }
    /// Set fall speed (terminal velocity).
    void SetFallSpeed(float fallSpeed);
    /// Return fall speed (terminal velocity).
    float GetFallSpeed() const { return fallSpeed_; }
    /// Set jump speed.
    void SetJumpSpeed(float jumpSpeed);
    /// Return jump speed.
    float GetJumpSpeed() const { return jumpSpeed_; }
    /// Set max slope angle in degrees.
    void SetMaxSlope(float maxSlope);
    /// Return max slope angle in degrees.
    float GetMaxSlope() const { return maxSlope_; }
    /// Set walk direction. This is neither a direction nor a velocity, but the amount to increment the position each simulation iteration, regardless of dt.
    void SetWalkDirection(const Vector3& walkDir);
    /// Check if character in on the ground.
    bool OnGround() const;
    /// Jump.
    void Jump(const Vector3 &jump = Vector3::ZERO);
    /// ApplyImpulse is same as Jump
    void ApplyImpulse(const Vector3 &impulse);
    /// Check if character can jump.
    bool CanJump() const;
    /// Set angular velocity.
    void SetAngularVelocity(const Vector3 &velocity);
    /// Return angular velocity.
    const Vector3 GetAngularVelocity() const;
    /// Set linear velocity.
    void SetLinearVelocity(const Vector3 &velocity);
    /// Return linear velocity.
    const Vector3 GetLinearVelocity() const;
    /// Teleport character into new position.
    void Warp(const Vector3 &position);
    /// Draw debug geometry.
    virtual void DrawDebugGeometry();

protected:
    void ReleaseKinematic();
    void ApplySettings(bool reapply=false);
    void OnNodeSet(Node* node) override;
    void OnSceneSet(Scene* scene) override;
    void AddKinematicToWorld();
    void RemoveKinematicFromWorld();
    /// Handle physics post-step event.
    virtual void HandlePhysicsPostStep(StringHash eventType, VariantMap& eventData);

protected:
    unsigned colLayer_{ 1 };
    unsigned colMask_{ 0xffff };

    float stepHeight_{ 0.4f };
    float maxJumpHeight_{ 2.0f };
    float jumpSpeed_{ 9.0f };
    float fallSpeed_{ 55.0f };
    float maxSlope_{ 45.0f };
    float linearDamping_{ 0.2f };
    float angularDamping_{ 0.2f };
    Vector3 gravity_{ Vector3(0.0f, -14.0f, 0.0f) };

    WeakPtr<PhysicsWorld> physicsWorld_;
    ea::unique_ptr<btPairCachingGhostObject> pairCachingGhostObject_;
    ea::unique_ptr<btKinematicCharacterController> kinematicController_;

    Vector3 colShapeOffset_{ Vector3::ZERO };
    bool reapplyAttributes_{ false };
};

}
