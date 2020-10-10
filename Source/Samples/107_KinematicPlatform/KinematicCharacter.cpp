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

#include <Urho3D/Core/Context.h>
#include <Urho3D/Graphics/AnimationController.h>
#include <Urho3D/Physics/PhysicsEvents.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Physics/KinematicCharacterController.h>
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Math/Ray.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/DebugNew.h>

#include "KinematicCharacter.h"

KinematicCharacter::KinematicCharacter(Context* context) :
    LogicComponent(context),
    onGround_(false),
    okToJump_(true),
    inAirTimer_(0.0f),
    curMoveDir_(Vector3::ZERO),
    isJumping_(false),
    jumpStarted_(false)
{
    // Only the physics update event is needed: unsubscribe from the rest for optimization
    SetUpdateEventMask(USE_FIXEDUPDATE | USE_FIXEDPOSTUPDATE);
}

void KinematicCharacter::RegisterObject(Context* context)
{
    context->RegisterFactory<KinematicCharacter>();

    // These macros register the class attributes to the Context for automatic load / save handling.
    // We specify the Default attribute mode which means it will be used both for saving into file, and network replication
    URHO3D_ATTRIBUTE("Controls Yaw", float, controls_.yaw_, 0.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Controls Pitch", float, controls_.pitch_, 0.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("On Ground", bool, onGround_, false, AM_DEFAULT);
    URHO3D_ATTRIBUTE("OK To Jump", bool, okToJump_, true, AM_DEFAULT);
    URHO3D_ATTRIBUTE("In Air Timer", float, inAirTimer_, 0.0f, AM_DEFAULT);
}

void KinematicCharacter::DelayedStart()
{
    collisionShape_ = node_->GetComponent<CollisionShape>(true);
    animController_ = node_->GetComponent<AnimationController>(true);
    kinematicController_ = node_->GetComponent<KinematicCharacterController>(true);
}
void KinematicCharacter::Start()
{
    // Component has been inserted into its scene node. Subscribe to events now
    SubscribeToEvent(GetNode(), E_NODECOLLISION, URHO3D_HANDLER(KinematicCharacter, HandleNodeCollision));
}

void KinematicCharacter::FixedUpdate(float timeStep)
{
    // Update the in air timer. Reset if grounded
    if (!onGround_)
        inAirTimer_ += timeStep;
    else
        inAirTimer_ = 0.0f;
    // When character has been in air less than 1/10 second, it's still interpreted as being on ground
    bool softGrounded = inAirTimer_ < INAIR_THRESHOLD_TIME;

    // Update movement & animation
    const Quaternion& rot = node_->GetRotation();
    Vector3 moveDir = Vector3::ZERO;
    onGround_ = kinematicController_->OnGround();

    if (controls_.IsDown(CTRL_FORWARD))
        moveDir += Vector3::FORWARD;
    if (controls_.IsDown(CTRL_BACK))
        moveDir += Vector3::BACK;
    if (controls_.IsDown(CTRL_LEFT))
        moveDir += Vector3::LEFT;
    if (controls_.IsDown(CTRL_RIGHT))
        moveDir += Vector3::RIGHT;

    // Normalize move vector so that diagonal strafing is not faster
    if (moveDir.LengthSquared() > 0.0f)
        moveDir.Normalize();

    // rotate movedir
    Vector3 velocity = rot * moveDir;
    if (onGround_)
    {
        curMoveDir_ = velocity;
    }
    else
    {   // In-air direction control is limited
        curMoveDir_ = curMoveDir_.Lerp(velocity, 0.03f);
    }

    kinematicController_->SetWalkDirection(curMoveDir_ * (softGrounded ? MOVE_FORCE : INAIR_MOVE_FORCE));

    if (softGrounded)
    {
        if (isJumping_)
        {
            isJumping_ = false;
        }
        isJumping_ = false;
        // Jump. Must release jump control between jumps
        if (controls_.IsDown(CTRL_JUMP))
        {
            isJumping_ = true;
            if (okToJump_)
            {
                okToJump_ = false;
                jumpStarted_ = true;
                kinematicController_->Jump();

                animController_->StopLayer(0);
                animController_->PlayExclusive("Models/Mutant/Mutant_Jump1.ani", 0, false, 0.2f);
                animController_->SetTime("Models/Mutant/Mutant_Jump1.ani", 0);
            }
        }
        else
        {
            okToJump_ = true;
        }
    }

    if (!onGround_ || jumpStarted_)
    {
        if (jumpStarted_)
        {
            if (animController_->IsAtEnd("Models/Mutant/Mutant_Jump1.ani"))
            {
                animController_->PlayExclusive("Models/Mutant/Mutant_Jump1.ani", 0, true, 0.3f);
                animController_->SetTime("Models/Mutant/Mutant_Jump1.ani", 0);
                jumpStarted_ = false;
            }
        }
        else
        {
            const float maxDistance = 50.0f;
            const float segmentDistance = 10.01f;
            PhysicsRaycastResult result;
            GetScene()->GetComponent<PhysicsWorld>()->RaycastSingleSegmented(result, Ray(node_->GetPosition(), Vector3::DOWN),
                                                                             maxDistance, segmentDistance, 0xffff);
            if (result.body_ && result.distance_ > 0.7f )
            {
                animController_->PlayExclusive("Models/Mutant/Mutant_Jump1.ani", 0, true, 0.2f);
            }
        }
    }
    else
    {
        // Play walk animation if moving on ground, otherwise fade it out
        if ((softGrounded) && !moveDir.Equals(Vector3::ZERO))
        {
            animController_->PlayExclusive("Models/Mutant/Mutant_Run.ani", 0, true, 0.2f);
        }
        else
        {
            animController_->PlayExclusive("Models/Mutant/Mutant_Idle0.ani", 0, true, 0.2f);
        }
    }
}

void KinematicCharacter::FixedPostUpdate(float timeStep)
{
    if (movingData_[0] == movingData_[1])
    {
        Matrix3x4 delta = movingData_[0].transform_ * movingData_[1].transform_.Inverse();

        // add delta
        Vector3 kPos;
        Quaternion kRot;
        kinematicController_->GetTransform(kPos, kRot);
        Matrix3x4 matKC(kPos, kRot, Vector3::ONE);

        // update
        matKC = delta * matKC;
        kinematicController_->SetTransform(matKC.Translation(), matKC.Rotation());

        // update yaw control (directly rotates char)
        controls_.yaw_ += delta.Rotation().YawAngle();
    }

    // update node position
    node_->SetWorldPosition(kinematicController_->GetPosition());

    // shift and clear
    movingData_[1] = movingData_[0];
    movingData_[0].node_ = 0;
}

bool KinematicCharacter::IsNodeMovingPlatform(Node *node) const
{
    if (node == 0)
    {
        return false;
    }

    const Variant& var = node->GetVar(StringHash("IsMovingPlatform"));
    return (var != Variant::EMPTY && var.GetBool());
}

void KinematicCharacter::NodeOnMovingPlatform(Node *node)
{
    if (!IsNodeMovingPlatform(node))
    {
        return;
    }

    movingData_[0].node_ = node;
    movingData_[0].transform_ = node->GetWorldTransform();
}

void KinematicCharacter::HandleNodeCollision(StringHash eventType, VariantMap& eventData)
{
    // Check collision contacts and see if character is standing on ground (look for a contact that has near vertical normal)
    using namespace NodeCollision;

    // possible moving platform trigger volume
    if (((RigidBody*)eventData[P_OTHERBODY].GetVoidPtr())->IsTrigger())
    {
        NodeOnMovingPlatform((Node*)eventData[P_OTHERNODE].GetVoidPtr());
    }
}
