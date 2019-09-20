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

#include <Eigen/Dense>
#include <array>
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ros/callback_queue.h>
#include <ros/ros.h>
#include <ros/transport_hints.h>

#include <ocs2_core/control/FeedforwardController.h>
#include <ocs2_core/control/LinearController.h>
#include <ocs2_core/misc/Benchmark.h>
#include <ocs2_mpc/MPC_BASE.h>
#include <ocs2_oc/oc_data/PrimalSolution.h>

// MPC messages
#include <ocs2_msgs/dummy.h>
#include <ocs2_msgs/mode_sequence.h>
#include <ocs2_msgs/mpc_flattened_controller.h>
#include <ocs2_msgs/mpc_observation.h>
#include <ocs2_msgs/mpc_target_trajectories.h>
#include <ocs2_msgs/reset.h>

#include "ocs2_comm_interfaces/CommandData.h"
#include "ocs2_comm_interfaces/SystemObservation.h"
#include "ocs2_comm_interfaces/ocs2_ros_interfaces/common/RosMsgConversions.h"
#include "ocs2_comm_interfaces/ocs2_ros_interfaces/task_listener/TaskListenerBase.h"

//#define PUBLISH_DUMMY
#define PUBLISH_THREAD

namespace ocs2 {

/**
 * This class implements MPC communication interface using ROS.
 *
 * @tparam STATE_DIM: Dimension of the state space.
 * @tparam INPUT_DIM: Dimension of the control input space.
 */
template <size_t STATE_DIM, size_t INPUT_DIM>
class MPC_ROS_Interface {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  using Ptr = std::shared_ptr<MPC_ROS_Interface<STATE_DIM, INPUT_DIM>>;

  using mpc_t = MPC_BASE<STATE_DIM, INPUT_DIM>;

  using scalar_t = typename mpc_t::scalar_t;
  using scalar_array_t = typename mpc_t::scalar_array_t;
  using scalar_array2_t = typename mpc_t::scalar_array2_t;
  using size_array_t = typename mpc_t::size_array_t;
  using state_vector_t = typename mpc_t::state_vector_t;
  using state_vector_array_t = typename mpc_t::state_vector_array_t;
  using state_vector_array2_t = typename mpc_t::state_vector_array2_t;
  using input_vector_t = typename mpc_t::input_vector_t;
  using input_vector_array_t = typename mpc_t::input_vector_array_t;
  using input_vector_array2_t = typename mpc_t::input_vector_array2_t;
  using input_state_matrix_t = typename mpc_t::input_state_matrix_t;
  using input_state_matrix_array_t = typename mpc_t::input_state_matrix_array_t;

  using cost_desired_trajectories_t = typename mpc_t::cost_desired_trajectories_t;
  using mode_sequence_template_t = typename mpc_t::mode_sequence_template_t;

  using system_observation_t = SystemObservation<STATE_DIM, INPUT_DIM>;

  using primal_solution_t = PrimalSolution<STATE_DIM, INPUT_DIM>;
  using command_data_t = CommandData<STATE_DIM, INPUT_DIM>;

  using controller_t = ControllerBase<STATE_DIM, INPUT_DIM>;
  using controller_ptr_array_t = std::vector<controller_t*>;

  using ros_msg_conversions_t = RosMsgConversions<STATE_DIM, INPUT_DIM>;
  using task_listener_ptr_array_t = typename TaskListenerBase<scalar_t>::shared_ptr_array_t;

  /**
   * Default constructor
   */
  MPC_ROS_Interface() = default;

  /**
   * Constructor.
   *
   * @param [in] mpc: The underlying MPC class to be used.
   * @param [in] robotName: The robot's name.
   * @param [in] taskListenerArray: An array of the shared_ptr to task listeners.
   */
  explicit MPC_ROS_Interface(mpc_t& mpc, const std::string& robotName = "robot",
                             const task_listener_ptr_array_t& taskListenerArray = task_listener_ptr_array_t());

  /**
   * Destructor.
   */
  virtual ~MPC_ROS_Interface();

  /**
   * Sets the class as its constructor.
   */
  void set();

  /**
   * Resets the class to its instantiation state.
   *
   * @param [in] initCostDesiredTrajectories: The initial desired cost trajectories.
   */
  virtual void reset(const cost_desired_trajectories_t& initCostDesiredTrajectories);

  /**
   * Shutdowns the ROS node.
   */
  void shutdownNode();

  /**
   * Initialize the ROS node.
   *
   * @param [in] argc: Command line number of arguments.
   * @param [in] argv: Command line vector of arguments.
   */
  void initializeNode(int argc, char* argv[]);

  /**
   * Returns a shared pointer to the node handle.
   *
   * @return shared pointer to the node handle.
   */
  std::shared_ptr<ros::NodeHandle>& nodeHandlePtr();

  /**
   * Spins ROS.
   */
  void spin();

  /**
   * This is the main routine which launches all the nodes required for MPC to run which includes:
   * (1) The MPC policy publisher (either feedback or feedforward policy).
   * (2) The observation subscriber which gets the current measured state to invoke the MPC run routine.
   * (3) The desired trajectories subscriber which gets the goal information from user.
   * (4) The desired mode sequence which gets the predefined mode switches for time-triggered hybrid systems.
   *
   * @param [in] argc: Command line number of arguments.
   * @param [in] argv: Command line vector of arguments.
   */
  void launchNodes(int argc, char* argv[]);

  /**
   * This method will be called either after the very fist call of the class or after a call to reset().
   * Users can use this function for any sort of initialization that they may need in the first call.
   *
   * @param [in] initObservation: The observation after the very fist call of the class or after call to reset().
   */
  virtual void initCall(const system_observation_t& initObservation) {}

  /**
   * Provides the initial mode sequence for time-triggered hybrid systems.
   *
   * @param [in] initObservation: The observation after the very fist call of the class or after call to reset().
   */
  virtual void initModeSequence(const system_observation_t& initObservation) {}

 protected:
  /**
   * Signal handler
   *
   * @param sig: signal
   */
  static void sigintHandler(int sig);

  /**
   * Callback to reset MPC.
   *
   * @param req: Service request.
   * @param res: Service response.
   */
  bool resetMpcCallback(ocs2_msgs::reset::Request& req, ocs2_msgs::reset::Response& res);

  /**
   * Dummy publisher for network debugging.
   */
  void publishDummy();

  /**
   * Creates MPC Policy message.
   *
   * @param [in] controllerIsUpdated: Whether the policy is updated.
   * @param [in] primalSolution: The policy data of the MPC.
   * @param [in] commandDataPtr: The command data of the MPC.
   * @return MPC policy message.
   */
  static ocs2_msgs::mpc_flattened_controller createMpcPolicyMsg(bool controllerIsUpdated, const primal_solution_t& primalSolution,
                                                                const command_data_t& commandData);

  /**
   * Handles ROS publishing thread.
   */
  void publisherWorkerThread();

  /**
   * @brief fillMpcOutputBuffers updates the *Buffer variables from the MPC object.
   * This method is automatically called by advanceMpc()
   * @param [in] mpcInitObservation: The observation used to run the MPC.
   */
  void fillMpcOutputBuffers(system_observation_t mpcInitObservation);

  /**
   * The callback method which receives the current observation, invokes the MPC algorithm,
   * and finally publishes the optimized policy.
   *
   * @param [in] msg: The observation message.
   */
  void mpcObservationCallback(const ocs2_msgs::mpc_observation::ConstPtr& msg);

  /**
   * The callback method which receives the user-defined target trajectories message.
   *
   * @param [in] msg: The target trajectories message.
   */
  void mpcTargetTrajectoriesCallback(const ocs2_msgs::mpc_target_trajectories::ConstPtr& msg);

  /**
   * The callback method which receives the user-defined mode sequence message.
   *
   * @param [in] msg: The mode sequence message.
   */
  void mpcModeSequenceCallback(const ocs2_msgs::mode_sequence::ConstPtr& msg);

 protected:
  /*
   * Variables
   */
  mpc_t& mpc_;

  std::string robotName_;

  task_listener_ptr_array_t taskListenerArray_;

  std::shared_ptr<ros::NodeHandle> nodeHandlerPtr_;

  // Publishers and subscribers
  ::ros::Subscriber mpcObservationSubscriber_;
  ::ros::Subscriber mpcTargetTrajectoriesSubscriber_;
  ::ros::Subscriber mpcModeSequenceSubscriber_;
  ::ros::Publisher mpcPolicyPublisher_;
  ::ros::Publisher dummyPublisher_;
  ::ros::ServiceServer mpcResetServiceServer_;

  std::unique_ptr<primal_solution_t> currentPrimalSolution_;
  std::unique_ptr<primal_solution_t> primalSolutionBuffer_;
  std::unique_ptr<command_data_t> currentCommand_;
  std::unique_ptr<command_data_t> commandBuffer_;

  mutable std::mutex policyBufferMutex_;  // for policy variables WITH suffix (*Buffer_)

  // multi-threading for publishers
  std::atomic_bool terminateThread_;
  std::atomic_bool readyToPublish_;
  std::thread publisherWorker_;
  std::mutex publisherMutex_;
  std::condition_variable msgReady_;

  benchmark::RepeatedTimer mpcTimer_;

  // MPC reset
  bool initialCall_;
  std::mutex resetMutex_;
  std::atomic_bool resetRequestedEver_;
};

}  // namespace ocs2

#include "implementation/MPC_ROS_Interface.h"
