#pragma once

#include <cilantro/icp_base.hpp>
#include <cilantro/non_rigid_registration_utilities.hpp>

namespace cilantro {
    template <typename ScalarT, class CorrespondenceSearchEngineT>
    class SparseCombinedMetricNonRigidICP3 : public IterativeClosestPointBase<SparseCombinedMetricNonRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformationSet<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>> {
        friend class IterativeClosestPointBase<SparseCombinedMetricNonRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformationSet<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>>;
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        SparseCombinedMetricNonRigidICP3(const ConstVectorSetMatrixMap<ScalarT,3> &dst_p,
                                         const ConstVectorSetMatrixMap<ScalarT,3> &dst_n,
                                         const ConstVectorSetMatrixMap<ScalarT,3> &src_p,
                                         size_t num_ctrl_nodes,
                                         const std::vector<NeighborSet<ScalarT>> &src_to_ctrl_neighborhoods,
                                         const std::vector<NeighborSet<ScalarT>> &ctrl_regularization_neighborhoods,
                                         CorrespondenceSearchEngineT &corr_engine)
                : IterativeClosestPointBase<SparseCombinedMetricNonRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformationSet<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>>(corr_engine),
                  dst_points_(dst_p), dst_normals_(dst_n), src_points_(src_p),
                  num_ctrl_nodes_(num_ctrl_nodes),
                  src_to_ctrl_neighborhoods_(src_to_ctrl_neighborhoods),
                  ctrl_regularization_neighborhoods_(ctrl_regularization_neighborhoods),
                  point_to_point_weight_(0.1), point_to_plane_weight_(1.0), stiffness_weight_(1.0), huber_boundary_((ScalarT)1e-6),
                  max_gauss_newton_iterations_(10), gauss_newton_convergence_tol_((ScalarT)1e-5),
                  max_conjugate_gradient_iterations_(1000), conjugate_gradient_convergence_tol_((ScalarT)1e-5),
                  src_points_trans_(src_points_.rows(), src_points_.cols()),
                  transform_dense_(src_points_.cols()), transform_iter_(num_ctrl_nodes_)
        {
            this->transform_init_.resize(num_ctrl_nodes_);
            this->transform_init_.setIdentity();
            this->correspondences_.reserve(std::max(dst_points_.cols(), src_points_.cols()));
        }

        inline ScalarT getPointToPointMetricWeight() const { return point_to_point_weight_; }

        inline SparseCombinedMetricNonRigidICP3& setPointToPointMetricWeight(ScalarT weight) {
            point_to_point_weight_ = weight;
            return *this;
        }

        inline ScalarT getPointToPlaneMetricWeight() const { return point_to_plane_weight_; }

        inline SparseCombinedMetricNonRigidICP3& setPointToPlaneMetricWeight(ScalarT weight) {
            point_to_plane_weight_ = weight;
            return *this;
        }

        inline ScalarT getStiffnessRegularizationWeight() const { return stiffness_weight_; }

        inline SparseCombinedMetricNonRigidICP3& setStiffnessRegularizationWeight(ScalarT weight) {
            stiffness_weight_ = weight;
            return *this;
        }

        inline size_t getMaxNumberOfGaussNewtonIterations() const { return max_gauss_newton_iterations_; }

        inline SparseCombinedMetricNonRigidICP3& setMaxNumberOfGaussNewtonIterations(size_t max_iter) {
            max_gauss_newton_iterations_ = max_iter;
            return *this;
        }

        inline ScalarT getGaussNewtonConvergenceTolerance() const { return gauss_newton_convergence_tol_; }

        inline SparseCombinedMetricNonRigidICP3& setGaussNewtonConvergenceTolerance(ScalarT conv_tol) {
            gauss_newton_convergence_tol_ = conv_tol;
            return *this;
        }

        inline size_t getMaxNumberOfConjugateGradientIterations() const { return max_conjugate_gradient_iterations_; }

        inline SparseCombinedMetricNonRigidICP3& setMaxNumberOfConjugateGradientIterations(size_t max_iter) {
            max_conjugate_gradient_iterations_ = max_iter;
            return *this;
        }

        inline ScalarT getConjugateGradientConvergenceTolerance() const { return conjugate_gradient_convergence_tol_; }

        inline SparseCombinedMetricNonRigidICP3& setConjugateGradientConvergenceTolerance(ScalarT conv_tol) {
            conjugate_gradient_convergence_tol_ = conv_tol;
            return *this;
        }

        inline ScalarT getHuberLossBoundary() const { return huber_boundary_; }

        inline SparseCombinedMetricNonRigidICP3& setHuberLossBoundary(ScalarT huber_boundary) {
            huber_boundary_ = huber_boundary;
            return *this;
        }

        inline const RigidTransformationSet<ScalarT,3>& getPointTransformations() const {
            return transform_dense_;
        }

    private:
        // Input data holders
        ConstVectorSetMatrixMap<ScalarT,3> dst_points_;
        ConstVectorSetMatrixMap<ScalarT,3> dst_normals_;
        ConstVectorSetMatrixMap<ScalarT,3> src_points_;
        size_t num_ctrl_nodes_;
        const std::vector<NeighborSet<ScalarT>>& src_to_ctrl_neighborhoods_;
        const std::vector<NeighborSet<ScalarT>>& ctrl_regularization_neighborhoods_;

        // Parameters
        ScalarT point_to_point_weight_;
        ScalarT point_to_plane_weight_;
        ScalarT stiffness_weight_;
        ScalarT huber_boundary_;
        size_t max_gauss_newton_iterations_;
        ScalarT gauss_newton_convergence_tol_;
        size_t max_conjugate_gradient_iterations_;
        ScalarT conjugate_gradient_convergence_tol_;

        // Temporaries and results
        VectorSet<ScalarT,3> src_points_trans_;
        RigidTransformationSet<ScalarT,3> transform_dense_;
        RigidTransformationSet<ScalarT,3> transform_iter_;

        // ICP interface
        inline void initializeComputation() {
            resampleTransformations(this->transform_, src_to_ctrl_neighborhoods_, transform_dense_);
        }

        // ICP interface
        inline void updateCorrespondences() {
            this->correspondence_search_engine_.findCorrespondences(transform_dense_, this->correspondences_);
        }

        // ICP interface
        void updateEstimate() {
#pragma omp parallel for
            for (size_t i = 0; i < src_points_.cols(); i++) {
                src_points_trans_.col(i) = transform_dense_[i]*src_points_.col(i);
            }

            estimateSparseWarpFieldCombinedMetric3<ScalarT,typename CorrespondenceSearchEngineT::CorrespondenceScalar>(dst_points_, dst_normals_, src_points_trans_, this->correspondences_, num_ctrl_nodes_, src_to_ctrl_neighborhoods_, ctrl_regularization_neighborhoods_, transform_iter_, point_to_point_weight_, point_to_plane_weight_, stiffness_weight_, huber_boundary_, max_gauss_newton_iterations_, gauss_newton_convergence_tol_, max_conjugate_gradient_iterations_, conjugate_gradient_convergence_tol_);
            this->transform_.preApply(transform_iter_);
            resampleTransformations(this->transform_, src_to_ctrl_neighborhoods_, transform_dense_);

            ScalarT max_delta_norm_sq = (ScalarT)0.0;
#pragma omp parallel for reduction (max: max_delta_norm_sq)
            for (size_t i = 0; i < transform_iter_.size(); i++) {
                ScalarT last_delta_norm_sq = (transform_iter_[i].linear() - Eigen::Matrix<ScalarT,3,3>::Identity()).squaredNorm() + transform_iter_[i].translation().squaredNorm();
                if (last_delta_norm_sq > max_delta_norm_sq) max_delta_norm_sq = last_delta_norm_sq;
            }
            this->last_delta_norm_ = std::sqrt(max_delta_norm_sq);
        }

        // ICP interface
        VectorSet<ScalarT,1> computeResiduals() {
            if (dst_points_.cols() == 0) {
                return VectorSet<ScalarT,1>::Constant(1, src_points_.cols(), std::numeric_limits<ScalarT>::quiet_NaN());
            }
            VectorSet<ScalarT,1> res(1, src_points_.cols());
            KDTree<ScalarT,3,KDTreeDistanceAdaptors::L2> dst_tree(dst_points_);
            Neighbor<ScalarT> nn;
            Vector<ScalarT,3> src_p_trans;
#pragma omp parallel for shared (res) private (nn, src_p_trans)
            for (size_t i = 0; i < src_points_.cols(); i++) {
                src_p_trans = transform_dense_[i]*src_points_.col(i);
                dst_tree.nearestNeighborSearch(src_p_trans, nn);
                ScalarT point_to_plane_dist = dst_normals_.col(nn.index).dot(dst_points_.col(nn.index) - src_p_trans);
                res[i] = point_to_point_weight_*(dst_points_.col(nn.index) - src_p_trans).squaredNorm() + point_to_plane_weight_*point_to_plane_dist*point_to_plane_dist;
            }
            return res;
        }
    };
}
