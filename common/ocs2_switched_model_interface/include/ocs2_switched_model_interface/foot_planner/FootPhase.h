//
// Created by rgrandia on 24.04.20.
//

#pragma once

#include "ocs2_switched_model_interface/core/SwitchedModel.h"
#include "ocs2_switched_model_interface/foot_planner/SwingSpline3d.h"
#include "ocs2_switched_model_interface/terrain/ConvexTerrain.h"
#include "ocs2_switched_model_interface/terrain/TerrainModel.h"
#include "ocs2_switched_model_interface/terrain/TerrainPlane.h"

namespace switched_model {

/**
 * Linear constraint A_p * p_world + A_v * v_world + b = 0
 */
struct FootNormalConstraintMatrix {
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  Eigen::Matrix<scalar_t, 1, 3> positionMatrix = Eigen::Matrix<scalar_t, 1, 3>::Zero();
  Eigen::Matrix<scalar_t, 1, 3> velocityMatrix = Eigen::Matrix<scalar_t, 1, 3>::Zero();
  scalar_t constant = 0.0;
};

/**
 * Linear inequality constraint A_p * p_world + b >=  0
 */
struct FootTangentialConstraintMatrix {
  Eigen::Matrix<scalar_t, -1, 3> A;
  vector_t b;
};

FootTangentialConstraintMatrix tangentialConstraintsFromConvexTerrain(const ConvexTerrain& stanceTerrain, scalar_t margin);

/**
 * Base class for a planned foot phase : Stance or Swing.
 */
class FootPhase {
 public:
  FootPhase() = default;
  virtual ~FootPhase() = default;
  FootPhase(const FootPhase&) = delete;
  FootPhase& operator=(const FootPhase&) = delete;

  /** Returns the contact flag for this phase. Stance phase: True, Swing phase: false */
  virtual bool contactFlag() const = 0;

  /** Returns the unit vector pointing in the normal direction */
  virtual vector3_t normalDirectionInWorldFrame(scalar_t time) const = 0;

  /** Nominal foothold location (upcoming for swinglegs) */
  virtual vector3_t nominalFootholdLocation() const = 0;

  /** Foot reference position in world frame */
  virtual vector3_t getPositionInWorld(scalar_t time) const = 0;

  /** Foot reference velocity in world frame */
  virtual vector3_t getVelocityInWorld(scalar_t time) const = 0;

  /** Foot reference acceleration in world frame */
  virtual vector3_t getAccelerationInWorld(scalar_t time) const = 0;

  /** Returns the velocity equality constraint formulated in the normal direction */
  virtual FootNormalConstraintMatrix getFootNormalConstraintInWorldFrame(scalar_t time) const = 0;

  /** Returns the position inequality constraints formulated in the tangential direction */
  virtual const FootTangentialConstraintMatrix* getFootTangentialConstraintInWorldFrame() const { return nullptr; };

  virtual scalar_t getMinimumFootClearance(scalar_t time) const { return 0.0; };
};

/**
 * Encodes a planned stance phase on a terrain plane.
 * The normal constraint makes the foot converge to the terrain plane when positionGain > 0.0
 */
class StancePhase final : public FootPhase {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  explicit StancePhase(const ConvexTerrain& stanceTerrain, scalar_t positionGain = 0.0, scalar_t terrainMargin = 0.0);
  ~StancePhase() override = default;

  bool contactFlag() const override { return true; };
  vector3_t normalDirectionInWorldFrame(scalar_t time) const override;
  vector3_t nominalFootholdLocation() const override;
  vector3_t getPositionInWorld(scalar_t time) const override;
  vector3_t getVelocityInWorld(scalar_t time) const override;
  vector3_t getAccelerationInWorld(scalar_t time) const override;
  FootNormalConstraintMatrix getFootNormalConstraintInWorldFrame(scalar_t time) const override;
  const FootTangentialConstraintMatrix* getFootTangentialConstraintInWorldFrame() const override;

 private:
  const vector3_t nominalFootholdLocation_;
  const vector3_t surfaceNormalInWorldFrame_;
  const FootNormalConstraintMatrix footNormalConstraint_;
  const FootTangentialConstraintMatrix footTangentialConstraint_;
};

/**
 * Encodes a swing trajectory between two terrain planes.
 * A cubic spline is designed in both liftoff and target plane. The constraint then smoothly interpolates between the two splines.
 */
class SwingPhase final : public FootPhase {
 public:
  struct SwingEvent {
    scalar_t time;
    scalar_t velocity;
    const TerrainPlane* terrainPlane;
  };

  struct SwingProfile {
    /// Desired swing height in Z direction of the world
    scalar_t swingHeight = 0.1;
    /// Shapes the swing profile, the XY velocity at the apex is set to be: apexVelocityFactor * swingdistance / dt
    scalar_t apexVelocityFactor = 3.0;
    /// Desired SDF clearance at the middle of the swing phase.
    scalar_t sdfMidswingMargin = 0.0;
    /// Desired SDF clearance at liftoff and touchdown. Slight negative margin allows a bit of ground penetration
    scalar_t sdfStartEndMargin = -0.02;
  };

  /**
   * Construct a swing phase:
   *    Creates a 3D swing reference motion
   *    Creates a 1D clearance profile for SDF based obstacle avoidance.
   * @param liftOff : Information about the liftoff event.
   * @param touchDown : Information about the touchdown event.
   * @param SwingProfile : Settings to shape the swing profile
   * @param terrainModel : (optional) Pointer to the terrain model. Terrain model should be kept alive externall as long as the swingphase
   * object exists. Will extract SDF and obstacle information from the terrain.
   * @param positionGain : Position feedback in the normal direction making the swing leg follow the reference profile in that direction.
   */
  SwingPhase(SwingEvent liftOff, SwingEvent touchDown, const SwingProfile& swingProfile, const TerrainModel* terrainModel = nullptr,
             scalar_t positionGain = 0.0);
  ~SwingPhase() override = default;

  bool contactFlag() const override { return false; };
  vector3_t normalDirectionInWorldFrame(scalar_t time) const override;
  vector3_t nominalFootholdLocation() const override;
  vector3_t getPositionInWorld(scalar_t time) const override;
  vector3_t getVelocityInWorld(scalar_t time) const override;
  vector3_t getAccelerationInWorld(scalar_t time) const override;
  FootNormalConstraintMatrix getFootNormalConstraintInWorldFrame(scalar_t time) const override;
  scalar_t getMinimumFootClearance(scalar_t time) const override;

 private:
  void setFullSwing(const SwingProfile& swingProfile, const TerrainModel* terrainModel);
  void setHalveSwing(const SwingProfile& swingProfile, const TerrainModel* terrainModel);

  scalar_t getScaling(scalar_t time) const;

  SwingEvent liftOff_;
  SwingEvent touchDown_;
  scalar_t positionGain_;

  std::unique_ptr<SwingSpline3d> motion_;
  std::unique_ptr<QuinticSwing> terrainClearanceMotion_;
};

}  // namespace switched_model
