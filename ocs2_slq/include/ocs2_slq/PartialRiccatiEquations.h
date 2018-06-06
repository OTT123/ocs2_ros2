/*
 * PartialRiccatiEquations.h
 *
 *  Created on: Jan 5, 2016
 *      Author: farbod
 */

#ifndef PARTIALRICCATIEQUATIONS_OCS2_H_
#define PARTIALRICCATIEQUATIONS_OCS2_H_

#include <Eigen/Dense>

#include <ocs2_core/integration/ODE_Base.h>

namespace ocs2{

/**
 * This class implements the Riccati equations for LQ problem.
 *
 * @tparam STATE_DIM: Dimension of the state space.
 * @tparam INPUT_DIM: Dimension of the control input space.
 */
template <size_t STATE_DIM, size_t INPUT_DIM>
class PartialRiccatiEquations : public ODE_Base<STATE_DIM*STATE_DIM+STATE_DIM+1>
{
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	enum { S_DIM_ = STATE_DIM*STATE_DIM+STATE_DIM+1 };
	typedef Dimensions<STATE_DIM, INPUT_DIM> DIMENSIONS;
	typedef typename DIMENSIONS::controller_t controller_t;
	typedef typename DIMENSIONS::scalar_t 		scalar_t;
	typedef typename DIMENSIONS::scalar_array_t scalar_array_t;
	typedef typename DIMENSIONS::eigen_scalar_t       eigen_scalar_t;
	typedef typename DIMENSIONS::eigen_scalar_array_t eigen_scalar_array_t;
	typedef typename DIMENSIONS::state_vector_t 	  state_vector_t;
	typedef typename DIMENSIONS::state_vector_array_t state_vector_array_t;
	typedef typename DIMENSIONS::input_vector_t 		input_vector_t;
	typedef typename DIMENSIONS::input_vector_array_t input_vector_array_t;
	typedef typename DIMENSIONS::input_state_t 	  input_state_t;
	typedef typename DIMENSIONS::input_state_array_t input_state_array_t;
	typedef typename DIMENSIONS::state_matrix_t 	  state_matrix_t;
	typedef typename DIMENSIONS::state_matrix_array_t state_matrix_array_t;
	typedef typename DIMENSIONS::input_matrix_t 		input_matrix_t;
	typedef typename DIMENSIONS::input_matrix_array_t input_matrix_array_t;
	typedef typename DIMENSIONS::state_input_matrix_t 		 state_input_matrix_t;
	typedef typename DIMENSIONS::state_input_matrix_array_t state_input_matrix_array_t;

	/**
	 * Default constructor.
	 */
	PartialRiccatiEquations() {}

	/**
	 * Default destructor.
	 */
	~PartialRiccatiEquations() {}

	/**
	 * Transcribe symmetric matrix Sm, vector Sv and scalar s into a single vector
	 *
	 * @param [in] Sm: \f$ S_m \f$
	 * @param [in] Sv: \f$ S_v \f$
	 * @param [in] s: \f$ s \f$
	 * @param [out] allSs: Single vector constructed by concatenating Sm, Sv and s.
	 */
	static void convert2Vector(const state_matrix_t& Sm, const state_vector_t& Sv, const eigen_scalar_t& s,
			Eigen::Matrix<double,S_DIM_,1>& allSs)  {

		allSs << Eigen::Map<const Eigen::VectorXd>(Sm.data(),STATE_DIM*STATE_DIM),
				Eigen::Map<const Eigen::VectorXd>(Sv.data(),STATE_DIM),
				s;
	}

    /**
    * Transcribes the stacked vector allSs into a symmetric matrix, Sm, a vector, Sv and a single scalar, s.
    * @param [in] allSs: Single vector constructed by concatenating Sm, Sv and s.
	 * @param [out] Sm: \f$ S_m \f$
	 * @param [out] Sv: \f$ S_v \f$
	 * @param [out] s: \f$ s \f$
    */
	static void convert2Matrix(const Eigen::Matrix<double,S_DIM_,1>& allSs,
			state_matrix_t& Sm, state_vector_t& Sv, eigen_scalar_t& s)  {

		Sm = Eigen::Map<const Eigen::MatrixXd>(allSs.data(),STATE_DIM,STATE_DIM);
		Sv = Eigen::Map<const Eigen::VectorXd>(allSs.data()+STATE_DIM*STATE_DIM, STATE_DIM);
		s  = allSs.template tail<1>();
	}

	/**
	 * Sets coefficients of the model.
	 *
	 * @param [in] timeStart: The start time of the subsystem.
	 * @param [in] timeFinal: The final time of the subsystem.
	 * @param [in] Am: The trajectory of \f$ A_m \f$ .
	 * @param [in] Bm: The trajectory of \f$ B_m \f$ .
	 * @param [in] q: The trajectory of \f$ q \f$ .
	 * @param [in] Qv: The trajectory of \f$ Q_v \f$ .
	 * @param [in] Qm: The trajectory of \f$ Q_m \f$ .
	 * @param [in] Rv: The trajectory of \f$ R_v \f$ .
	 * @param [in] Rm: The trajectory of \f$ R_m \f$ .
	 * @param [in] Pm: The trajectory of \f$ P_m \f$ .
	 */
	void setData(const scalar_t& timeStart, const scalar_t& timeFinal,
			const state_matrix_t& Am, const state_input_matrix_t& Bm,
			const eigen_scalar_t& q, const state_vector_t& Qv, const state_matrix_t& Qm,
			const input_vector_t& Rv, const input_matrix_t& Rm,
			const input_state_t& Pm)  {

		timeStart_ = timeStart;
		timeFinal_ = timeFinal;
		Am_ = Am;
		Bm_ = Bm;
		q_ = q;
		Qv_ = Qv;
		Qm_ = Qm;
		Rv_ = Rv;
		Rm_ = Rm;
		Pm_ = Pm;
	}

	/**
	 * Computes derivatives.
	 *
	 * @param [in] t: Time.
	 * @param [in] state: Single vector constructed by concatenating Sm, Sv and s.
	 * @param [out] derivative: d(state)/dt.
	 */
	void computeFlowMap(const scalar_t& t,
			const Eigen::Matrix<double,S_DIM_,1>& state,
			Eigen::Matrix<double,S_DIM_,1>& derivative) {

		state_matrix_t Sm;
		state_vector_t Sv;
		eigen_scalar_t s;
		convert2Matrix(state, Sm, Sv, s);

		state_matrix_t dSmdt, dSmdz;
		state_vector_t dSvdt, dSvdz;
		eigen_scalar_t dsdt, dsdz;

		// Riccati equations for the original system
		dSmdt = Qm_ + Am_.transpose()*Sm + Sm.transpose()*Am_ - (Pm_+Bm_.transpose()*Sm).transpose()*Rm_.inverse()*(Pm_+Bm_.transpose()*Sm);
		dSmdt = (dSmdt+dSmdt.transpose()).eval()*0.5;
		dSvdt = Qv_ + Am_.transpose()*Sv - (Pm_+Bm_.transpose()*Sm).transpose()*Rm_.inverse()*(Rv_+Bm_.transpose()*Sv);
		dsdt  = q_ - 0.5*(Rv_+Bm_.transpose()*Sv).transpose()*Rm_.inverse()*(Rv_+Bm_.transpose()*Sv);
		// Riccati equations for the equivalent system
		dSmdz = (timeFinal_-timeStart_)*dSmdt;
		dSvdz = (timeFinal_-timeStart_)*dSvdt;
		dsdz  = (timeFinal_-timeStart_)*dsdt;

//		if (makePSD(dSmdz))
//			std::cout << "time: " << t << std::endl;

		convert2Vector(dSmdz, dSvdz, dsdz, derivative);
	}

protected:
	/**
	 * Makes the matrix PSD.
	 * @tparam Derived type.
	 * @param [out] squareMatrix: The matrix to become PSD.
	 * @return boolean
	 */
	template <typename Derived>
	bool makePSD(Eigen::MatrixBase<Derived>& squareMatrix) {

		if (squareMatrix.rows() != squareMatrix.cols())  throw std::runtime_error("Not a square matrix: makePSD() method is for square matrix.");

		Eigen::SelfAdjointEigenSolver<Derived> eig(squareMatrix);
		Eigen::VectorXd lambda = eig.eigenvalues();

		bool hasNegativeEigenValue = false;
		for (size_t j=0; j<lambda.size() ; j++)
			if (lambda(j) < 0.0) {
				hasNegativeEigenValue = true;
				lambda(j) = 0.0;
			}

		if (hasNegativeEigenValue)
			squareMatrix = eig.eigenvectors() * lambda.asDiagonal() * eig.eigenvectors().inverse();
		//	else
		//		squareMatrix = 0.5*(squareMatrix+squareMatrix.transpose()).eval();

		return hasNegativeEigenValue;
	}

private:
	scalar_t timeStart_;
	scalar_t timeFinal_;

	state_matrix_t Am_;
	state_input_matrix_t Bm_;

	eigen_scalar_t q_;
	state_vector_t Qv_;
	state_matrix_t Qm_;
	input_vector_t Rv_;
	input_matrix_t Rm_;
	input_state_t Pm_;

};

} // namespace ocs2

#endif /* PARTIALRICCATIEQUATIONS_H_ */
