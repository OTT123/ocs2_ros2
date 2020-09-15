/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#pragma once

#include <ocs2_core/Types.h>

#include "GaussNewtonDDP.h"
#include "riccati_equations/DiscreteTimeRiccatiEquations.h"

namespace ocs2 {

/**
 * This class is an interface class for the single-thread and multi-thread ILQR.
 */
class ILQR : public GaussNewtonDDP {
 public:
  using BASE = GaussNewtonDDP;
  /**
   * Constructor
   *
   * @param [in] rolloutPtr: The rollout class used for simulating the system dynamics.
   * @param [in] systemDynamicsPtr: The system dynamics and derivatives for the subsystems.
   * @param [in] systemConstraintsPtr: The system constraint function and its derivatives for subsystems.
   * @param [in] costFunctionPtr: The cost function (intermediate and final costs) and its derivatives for subsystems.
   * @param [in] operatingTrajectoriesPtr: The operating trajectories of system which will be used for initialization of ILQR.
   * @param [in] ddpSettings: Structure containing the settings for the DDP algorithm.
   * @param [in] heuristicsFunctionPtr: Heuristic function used in the infinite time optimal control formulation. If it is not
   * defined, we will use the final cost function defined in costFunctionPtr.
   */
  ILQR(const RolloutBase* rolloutPtr, const SystemDynamicsBase* systemDynamicsPtr, const ConstraintBase* systemConstraintsPtr,
       const CostFunctionBase* costFunctionPtr, const SystemOperatingTrajectoriesBase* operatingTrajectoriesPtr, ddp::Settings ddpSettings,
       const CostFunctionBase* heuristicsFunctionPtr = nullptr);

  /**
   * Default destructor.
   */
  ~ILQR() override = default;

 protected:
  void setupOptimizer(size_t numPartitions) override;

  scalar_t solveSequentialRiccatiEquations(const matrix_t& SmFinal, const vector_t& SvFinal, const scalar_t& sFinal) override;

  void riccatiEquationsWorker(size_t workerIndex, size_t partitionIndex, const matrix_t& SmFinal, const vector_t& SvFinal,
                              const scalar_t& sFinal) override;

  void calculateController() override;

  void calculateControllerWorker(size_t workerIndex, size_t partitionIndex, size_t timeIndex) override;

  matrix_t computeHamiltonianHessian(const ModelDataBase& modelData, const matrix_t& Sm) const override;

  void approximateIntermediateLQ(const scalar_array_t& timeTrajectory, const size_array_t& postEventIndices,
                                 const vector_array_t& stateTrajectory, const vector_array_t& inputTrajectory,
                                 std::vector<ModelDataBase>& modelDataTrajectory) override;

  /**
   * Calculates the discrete-time LQ approximation from the continuous-time LQ approximation.
   *
   * @param [in] workerIndex: Working agent index.
   * @param [in] continuousTimeModelData: Time partition index.
   * @param [out] modelData: Time index in the partition.
   */
  void discreteLQWorker(size_t workerIndex, scalar_t timeStep, const ModelDataBase& continuousTimeModelData, ModelDataBase& modelData);

  /****************
   *** Variables **
   ****************/
  matrix_array2_t projectedKmTrajectoryStock_;  // projected feedback
  vector_array2_t projectedLvTrajectoryStock_;  // projected feedforward

  std::vector<std::unique_ptr<DiscreteTimeRiccatiEquations>> riccatiEquationsPtrStock_;
};

}  // namespace ocs2
