%typemap(cscode) Urho3D::CollisionShape %{
  public $typemap(cstype, Urho3D::CollisionGeometryData *) GeometryData {
    get { return GetGeometryData(); }
  }
  public $typemap(cstype, Urho3D::PhysicsWorld *) PhysicsWorld {
    get { return GetPhysicsWorld(); }
  }
  public $typemap(cstype, Urho3D::ShapeType) ShapeType {
    get { return GetShapeType(); }
    set { SetShapeType(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) Size {
    get { return GetSize(); }
    set { SetSize(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) Position {
    get { return GetPosition(); }
    set { SetPosition(value); }
  }
  public $typemap(cstype, const Urho3D::Quaternion &) Rotation {
    get { return GetRotation(); }
    set { SetRotation(value); }
  }
  public $typemap(cstype, float) Margin {
    get { return GetMargin(); }
    set { SetMargin(value); }
  }
  public $typemap(cstype, Urho3D::Model *) Model {
    get { return GetModel(); }
    set { SetModel(value); }
  }
  public $typemap(cstype, unsigned int) LodLevel {
    get { return GetLodLevel(); }
    set { SetLodLevel(value); }
  }
  public $typemap(cstype, Urho3D::BoundingBox) WorldBoundingBox {
    get { return GetWorldBoundingBox(); }
  }
  public $typemap(cstype, Urho3D::ResourceRef) ModelAttr {
    get { return GetModelAttr(); }
  }
%}
%csmethodmodifiers Urho3D::CollisionShape::GetGeometryData "private";
%csmethodmodifiers Urho3D::CollisionShape::GetPhysicsWorld "private";
%csmethodmodifiers Urho3D::CollisionShape::GetShapeType "private";
%csmethodmodifiers Urho3D::CollisionShape::SetShapeType "private";
%csmethodmodifiers Urho3D::CollisionShape::GetSize "private";
%csmethodmodifiers Urho3D::CollisionShape::SetSize "private";
%csmethodmodifiers Urho3D::CollisionShape::GetPosition "private";
%csmethodmodifiers Urho3D::CollisionShape::SetPosition "private";
%csmethodmodifiers Urho3D::CollisionShape::GetRotation "private";
%csmethodmodifiers Urho3D::CollisionShape::SetRotation "private";
%csmethodmodifiers Urho3D::CollisionShape::GetMargin "private";
%csmethodmodifiers Urho3D::CollisionShape::SetMargin "private";
%csmethodmodifiers Urho3D::CollisionShape::GetModel "private";
%csmethodmodifiers Urho3D::CollisionShape::SetModel "private";
%csmethodmodifiers Urho3D::CollisionShape::GetLodLevel "private";
%csmethodmodifiers Urho3D::CollisionShape::SetLodLevel "private";
%csmethodmodifiers Urho3D::CollisionShape::GetWorldBoundingBox "private";
%csmethodmodifiers Urho3D::CollisionShape::GetModelAttr "private";
%typemap(cscode) Urho3D::Constraint %{
  public $typemap(cstype, Urho3D::PhysicsWorld *) PhysicsWorld {
    get { return GetPhysicsWorld(); }
  }
  public $typemap(cstype, Urho3D::ConstraintType) ConstraintType {
    get { return GetConstraintType(); }
    set { SetConstraintType(value); }
  }
  public $typemap(cstype, Urho3D::RigidBody *) OwnBody {
    get { return GetOwnBody(); }
  }
  public $typemap(cstype, Urho3D::RigidBody *) OtherBody {
    get { return GetOtherBody(); }
    set { SetOtherBody(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) Position {
    get { return GetPosition(); }
    set { SetPosition(value); }
  }
  public $typemap(cstype, const Urho3D::Quaternion &) Rotation {
    get { return GetRotation(); }
    set { SetRotation(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) OtherPosition {
    get { return GetOtherPosition(); }
    set { SetOtherPosition(value); }
  }
  public $typemap(cstype, const Urho3D::Quaternion &) OtherRotation {
    get { return GetOtherRotation(); }
    set { SetOtherRotation(value); }
  }
  public $typemap(cstype, Urho3D::Vector3) WorldPosition {
    get { return GetWorldPosition(); }
  }
  public $typemap(cstype, const Urho3D::Vector2 &) HighLimit {
    get { return GetHighLimit(); }
    set { SetHighLimit(value); }
  }
  public $typemap(cstype, const Urho3D::Vector2 &) LowLimit {
    get { return GetLowLimit(); }
    set { SetLowLimit(value); }
  }
  public $typemap(cstype, float) ERP {
    get { return GetERP(); }
    set { SetERP(value); }
  }
  public $typemap(cstype, float) CFM {
    get { return GetCFM(); }
    set { SetCFM(value); }
  }
  public $typemap(cstype, bool) DisableCollision {
    get { return GetDisableCollision(); }
    set { SetDisableCollision(value); }
  }
%}
%csmethodmodifiers Urho3D::Constraint::GetPhysicsWorld "private";
%csmethodmodifiers Urho3D::Constraint::GetConstraintType "private";
%csmethodmodifiers Urho3D::Constraint::SetConstraintType "private";
%csmethodmodifiers Urho3D::Constraint::GetOwnBody "private";
%csmethodmodifiers Urho3D::Constraint::GetOtherBody "private";
%csmethodmodifiers Urho3D::Constraint::SetOtherBody "private";
%csmethodmodifiers Urho3D::Constraint::GetPosition "private";
%csmethodmodifiers Urho3D::Constraint::SetPosition "private";
%csmethodmodifiers Urho3D::Constraint::GetRotation "private";
%csmethodmodifiers Urho3D::Constraint::SetRotation "private";
%csmethodmodifiers Urho3D::Constraint::GetOtherPosition "private";
%csmethodmodifiers Urho3D::Constraint::SetOtherPosition "private";
%csmethodmodifiers Urho3D::Constraint::GetOtherRotation "private";
%csmethodmodifiers Urho3D::Constraint::SetOtherRotation "private";
%csmethodmodifiers Urho3D::Constraint::GetWorldPosition "private";
%csmethodmodifiers Urho3D::Constraint::GetHighLimit "private";
%csmethodmodifiers Urho3D::Constraint::SetHighLimit "private";
%csmethodmodifiers Urho3D::Constraint::GetLowLimit "private";
%csmethodmodifiers Urho3D::Constraint::SetLowLimit "private";
%csmethodmodifiers Urho3D::Constraint::GetERP "private";
%csmethodmodifiers Urho3D::Constraint::SetERP "private";
%csmethodmodifiers Urho3D::Constraint::GetCFM "private";
%csmethodmodifiers Urho3D::Constraint::SetCFM "private";
%csmethodmodifiers Urho3D::Constraint::GetDisableCollision "private";
%csmethodmodifiers Urho3D::Constraint::SetDisableCollision "private";
%typemap(cscode) Urho3D::KinematicCharacterController %{
  public $typemap(cstype, const Urho3D::Vector3 &) Position {
    get { return GetPosition(); }
  }
  public $typemap(cstype, const Urho3D::Quaternion &) Rotation {
    get { return GetRotation(); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) Gravity {
    get { return GetGravity(); }
    set { SetGravity(value); }
  }
  public $typemap(cstype, float) LinearDamping {
    get { return GetLinearDamping(); }
    set { SetLinearDamping(value); }
  }
  public $typemap(cstype, float) AngularDamping {
    get { return GetAngularDamping(); }
    set { SetAngularDamping(value); }
  }
  public $typemap(cstype, float) StepHeight {
    get { return GetStepHeight(); }
    set { SetStepHeight(value); }
  }
  public $typemap(cstype, float) MaxJumpHeight {
    get { return GetMaxJumpHeight(); }
    set { SetMaxJumpHeight(value); }
  }
  public $typemap(cstype, float) FallSpeed {
    get { return GetFallSpeed(); }
    set { SetFallSpeed(value); }
  }
  public $typemap(cstype, float) JumpSpeed {
    get { return GetJumpSpeed(); }
    set { SetJumpSpeed(value); }
  }
  public $typemap(cstype, float) MaxSlope {
    get { return GetMaxSlope(); }
    set { SetMaxSlope(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) AngularVelocity {
    get { return GetAngularVelocity(); }
    set { SetAngularVelocity(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) LinearVelocity {
    get { return GetLinearVelocity(); }
    set { SetLinearVelocity(value); }
  }
  public $typemap(cstype, unsigned int) CollisionLayer {
    get { return GetCollisionLayer(); }
    set { SetCollisionLayer(value); }
  }
  public $typemap(cstype, unsigned int) CollisionMask {
    get { return GetCollisionMask(); }
    set { SetCollisionMask(value); }
  }
%}
%csmethodmodifiers Urho3D::KinematicCharacterController::GetPosition "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetRotation "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetGravity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetGravity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetLinearDamping "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetLinearDamping "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetAngularDamping "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetAngularDamping "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetStepHeight "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetStepHeight "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetMaxJumpHeight "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetMaxJumpHeight "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetFallSpeed "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetFallSpeed "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetJumpSpeed "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetJumpSpeed "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetMaxSlope "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetMaxSlope "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetAngularVelocity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetAngularVelocity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetLinearVelocity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetLinearVelocity "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetCollisionLayer "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetCollisionLayer "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetCollisionMask "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetCollisionMask "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::GetCollisionEventMode "private";
%csmethodmodifiers Urho3D::KinematicCharacterController::SetCollisionEventMode "private";
%typemap(cscode) Urho3D::PhysicsWorld %{
  public $typemap(cstype, Urho3D::Vector3) Gravity {
    get { return GetGravity(); }
  }
  public $typemap(cstype, int) MaxSubSteps {
    get { return GetMaxSubSteps(); }
    set { SetMaxSubSteps(value); }
  }
  public $typemap(cstype, int) NumIterations {
    get { return GetNumIterations(); }
    set { SetNumIterations(value); }
  }
  public $typemap(cstype, bool) Interpolation {
    get { return GetInterpolation(); }
    set { SetInterpolation(value); }
  }
  public $typemap(cstype, bool) InternalEdge {
    get { return GetInternalEdge(); }
    set { SetInternalEdge(value); }
  }
  public $typemap(cstype, bool) SplitImpulse {
    get { return GetSplitImpulse(); }
    set { SetSplitImpulse(value); }
  }
  public $typemap(cstype, int) Fps {
    get { return GetFps(); }
    set { SetFps(value); }
  }
  public $typemap(cstype, float) MaxNetworkAngularVelocity {
    get { return GetMaxNetworkAngularVelocity(); }
    set { SetMaxNetworkAngularVelocity(value); }
  }
  public $typemap(cstype, btDiscreteDynamicsWorld *) World {
    get { return GetWorld(); }
  }
  public $typemap(cstype, eastl::unordered_map<eastl::pair<Urho3D::Model *, unsigned int>, Urho3D::SharedPtr<Urho3D::CollisionGeometryData>> &) TriMeshCache {
    get { return GetTriMeshCache(); }
  }
  public $typemap(cstype, eastl::unordered_map<eastl::pair<Urho3D::Model *, unsigned int>, Urho3D::SharedPtr<Urho3D::CollisionGeometryData>> &) ConvexCache {
    get { return GetConvexCache(); }
  }
  public $typemap(cstype, eastl::unordered_map<eastl::pair<Urho3D::Model *, unsigned int>, Urho3D::SharedPtr<Urho3D::CollisionGeometryData>> &) GImpactTrimeshCache {
    get { return GetGImpactTrimeshCache(); }
  }
%}
%csmethodmodifiers Urho3D::PhysicsWorld::GetGravity "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetMaxSubSteps "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetMaxSubSteps "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetNumIterations "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetNumIterations "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetInterpolation "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetInterpolation "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetInternalEdge "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetInternalEdge "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetSplitImpulse "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetSplitImpulse "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetFps "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetFps "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetMaxNetworkAngularVelocity "private";
%csmethodmodifiers Urho3D::PhysicsWorld::SetMaxNetworkAngularVelocity "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetWorld "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetTriMeshCache "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetConvexCache "private";
%csmethodmodifiers Urho3D::PhysicsWorld::GetGImpactTrimeshCache "private";
%typemap(cscode) Urho3D::RaycastVehicle %{
  public $typemap(cstype, int) NumWheels {
    get { return GetNumWheels(); }
  }
  public $typemap(cstype, float) MaxSideSlipSpeed {
    get { return GetMaxSideSlipSpeed(); }
    set { SetMaxSideSlipSpeed(value); }
  }
  public $typemap(cstype, float) InAirRPM {
    get { return GetInAirRPM(); }
    set { SetInAirRPM(value); }
  }
  public $typemap(cstype, Urho3D::IntVector3) CoordinateSystem {
    get { return GetCoordinateSystem(); }
  }
  public $typemap(cstype, eastl::vector<Urho3D::Variant>) WheelDataAttr {
    get { return GetWheelDataAttr(); }
  }
%}
%csmethodmodifiers Urho3D::RaycastVehicle::GetNumWheels "private";
%csmethodmodifiers Urho3D::RaycastVehicle::GetMaxSideSlipSpeed "private";
%csmethodmodifiers Urho3D::RaycastVehicle::SetMaxSideSlipSpeed "private";
%csmethodmodifiers Urho3D::RaycastVehicle::GetInAirRPM "private";
%csmethodmodifiers Urho3D::RaycastVehicle::SetInAirRPM "private";
%csmethodmodifiers Urho3D::RaycastVehicle::GetCoordinateSystem "private";
%csmethodmodifiers Urho3D::RaycastVehicle::GetWheelDataAttr "private";
%typemap(cscode) Urho3D::RigidBody %{
  public $typemap(cstype, Urho3D::PhysicsWorld *) PhysicsWorld {
    get { return GetPhysicsWorld(); }
  }
  public $typemap(cstype, btRigidBody *) Body {
    get { return GetBody(); }
  }
  public $typemap(cstype, btCompoundShape *) CompoundShape {
    get { return GetCompoundShape(); }
  }
  public $typemap(cstype, float) Mass {
    get { return GetMass(); }
    set { SetMass(value); }
  }
  public $typemap(cstype, Urho3D::Vector3) Position {
    get { return GetPosition(); }
  }
  public $typemap(cstype, Urho3D::Quaternion) Rotation {
    get { return GetRotation(); }
  }
  public $typemap(cstype, Urho3D::Vector3) LinearVelocity {
    get { return GetLinearVelocity(); }
  }
  public $typemap(cstype, Urho3D::Vector3) LinearFactor {
    get { return GetLinearFactor(); }
  }
  public $typemap(cstype, float) LinearRestThreshold {
    get { return GetLinearRestThreshold(); }
    set { SetLinearRestThreshold(value); }
  }
  public $typemap(cstype, float) LinearDamping {
    get { return GetLinearDamping(); }
    set { SetLinearDamping(value); }
  }
  public $typemap(cstype, Urho3D::Vector3) AngularVelocity {
    get { return GetAngularVelocity(); }
  }
  public $typemap(cstype, Urho3D::Vector3) AngularFactor {
    get { return GetAngularFactor(); }
  }
  public $typemap(cstype, float) AngularRestThreshold {
    get { return GetAngularRestThreshold(); }
    set { SetAngularRestThreshold(value); }
  }
  public $typemap(cstype, float) AngularDamping {
    get { return GetAngularDamping(); }
    set { SetAngularDamping(value); }
  }
  public $typemap(cstype, float) Friction {
    get { return GetFriction(); }
    set { SetFriction(value); }
  }
  public $typemap(cstype, Urho3D::Vector3) AnisotropicFriction {
    get { return GetAnisotropicFriction(); }
  }
  public $typemap(cstype, float) RollingFriction {
    get { return GetRollingFriction(); }
    set { SetRollingFriction(value); }
  }
  public $typemap(cstype, float) Restitution {
    get { return GetRestitution(); }
    set { SetRestitution(value); }
  }
  public $typemap(cstype, float) ContactProcessingThreshold {
    get { return GetContactProcessingThreshold(); }
    set { SetContactProcessingThreshold(value); }
  }
  public $typemap(cstype, float) CcdRadius {
    get { return GetCcdRadius(); }
    set { SetCcdRadius(value); }
  }
  public $typemap(cstype, float) CcdMotionThreshold {
    get { return GetCcdMotionThreshold(); }
    set { SetCcdMotionThreshold(value); }
  }
  public $typemap(cstype, bool) UseGravity {
    get { return GetUseGravity(); }
    set { SetUseGravity(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) GravityOverride {
    get { return GetGravityOverride(); }
    set { SetGravityOverride(value); }
  }
  public $typemap(cstype, const Urho3D::Vector3 &) CenterOfMass {
    get { return GetCenterOfMass(); }
  }
  public $typemap(cstype, unsigned int) CollisionLayer {
    get { return GetCollisionLayer(); }
    set { SetCollisionLayer(value); }
  }
  public $typemap(cstype, unsigned int) CollisionMask {
    get { return GetCollisionMask(); }
    set { SetCollisionMask(value); }
  }
  public $typemap(cstype, Urho3D::CollisionEventMode) CollisionEventMode {
    get { return GetCollisionEventMode(); }
    set { SetCollisionEventMode(value); }
  }
  public $typemap(cstype, const eastl::vector<unsigned char> &) NetAngularVelocityAttr {
    get { return GetNetAngularVelocityAttr(); }
    set { SetNetAngularVelocityAttr(value); }
  }
%}
%csmethodmodifiers Urho3D::RigidBody::GetPhysicsWorld "private";
%csmethodmodifiers Urho3D::RigidBody::GetBody "private";
%csmethodmodifiers Urho3D::RigidBody::GetCompoundShape "private";
%csmethodmodifiers Urho3D::RigidBody::GetMass "private";
%csmethodmodifiers Urho3D::RigidBody::SetMass "private";
%csmethodmodifiers Urho3D::RigidBody::GetPosition "private";
%csmethodmodifiers Urho3D::RigidBody::GetRotation "private";
%csmethodmodifiers Urho3D::RigidBody::GetLinearVelocity "private";
%csmethodmodifiers Urho3D::RigidBody::GetLinearFactor "private";
%csmethodmodifiers Urho3D::RigidBody::GetLinearRestThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::SetLinearRestThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::GetLinearDamping "private";
%csmethodmodifiers Urho3D::RigidBody::SetLinearDamping "private";
%csmethodmodifiers Urho3D::RigidBody::GetAngularVelocity "private";
%csmethodmodifiers Urho3D::RigidBody::GetAngularFactor "private";
%csmethodmodifiers Urho3D::RigidBody::GetAngularRestThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::SetAngularRestThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::GetAngularDamping "private";
%csmethodmodifiers Urho3D::RigidBody::SetAngularDamping "private";
%csmethodmodifiers Urho3D::RigidBody::GetFriction "private";
%csmethodmodifiers Urho3D::RigidBody::SetFriction "private";
%csmethodmodifiers Urho3D::RigidBody::GetAnisotropicFriction "private";
%csmethodmodifiers Urho3D::RigidBody::GetRollingFriction "private";
%csmethodmodifiers Urho3D::RigidBody::SetRollingFriction "private";
%csmethodmodifiers Urho3D::RigidBody::GetRestitution "private";
%csmethodmodifiers Urho3D::RigidBody::SetRestitution "private";
%csmethodmodifiers Urho3D::RigidBody::GetContactProcessingThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::SetContactProcessingThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::GetCcdRadius "private";
%csmethodmodifiers Urho3D::RigidBody::SetCcdRadius "private";
%csmethodmodifiers Urho3D::RigidBody::GetCcdMotionThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::SetCcdMotionThreshold "private";
%csmethodmodifiers Urho3D::RigidBody::GetUseGravity "private";
%csmethodmodifiers Urho3D::RigidBody::SetUseGravity "private";
%csmethodmodifiers Urho3D::RigidBody::GetGravityOverride "private";
%csmethodmodifiers Urho3D::RigidBody::SetGravityOverride "private";
%csmethodmodifiers Urho3D::RigidBody::GetCenterOfMass "private";
%csmethodmodifiers Urho3D::RigidBody::GetCollisionLayer "private";
%csmethodmodifiers Urho3D::RigidBody::SetCollisionLayer "private";
%csmethodmodifiers Urho3D::RigidBody::GetCollisionMask "private";
%csmethodmodifiers Urho3D::RigidBody::SetCollisionMask "private";
%csmethodmodifiers Urho3D::RigidBody::GetCollisionEventMode "private";
%csmethodmodifiers Urho3D::RigidBody::SetCollisionEventMode "private";
%csmethodmodifiers Urho3D::RigidBody::GetNetAngularVelocityAttr "private";
%csmethodmodifiers Urho3D::RigidBody::SetNetAngularVelocityAttr "private";
