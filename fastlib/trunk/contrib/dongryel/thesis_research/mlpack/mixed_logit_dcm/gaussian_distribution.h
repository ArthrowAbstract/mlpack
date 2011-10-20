/** @file gaussian_distribution.h
 *
 *  The Gaussian distribution that can be used for mixed logit
 *  discrete choice model.
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef MLPACK_MIXED_LOGIT_DCM_GAUSSIAN_DISTRIBUTION_H
#define MLPACK_MIXED_LOGIT_DCM_GAUSSIAN_DISTRIBUTION_H

namespace mlpack {
namespace mixed_logit_dcm {

/** @brief The Gaussian distribution which can be used for mixed logit
 *         discrete choice model. This Gaussian distribution has the
 *         mean and the associated upper-triangular Cholesky factor.
 */
class GaussianDistribution {
  public:

    class PrivateData {
      public:

        int cholesky_factor_dimension_;

        int num_cholesky_factor_entries_;

        std::vector<int> nonzero_column_indices_;

        std::vector<int> start_indices_;

        /** @brief Stores the cached solution to the upper triangular
         *         linear system solved for computing the attribute
         *         gradient with respect to parameter.
         */
        arma::vec cached_solution_;

        arma::mat cholesky_factor_;

      public:
        PrivateData() {
          cholesky_factor_dimension_ = 0;
          num_cholesky_factor_entries_ = 0;
        }
    };

  private:

    /** @brief Sets up the Cholesky factor so that the linear system
     *         can be solved for the given parameters. This function
     *         must be called before DrawBeta is called.
     */
    static void SetupCholeskyFactor_(
      const arma::vec &parameters, PrivateData *private_data) {

      private_data->cholesky_factor_.zeros(
        private_data->cholesky_factor_dimension_,
        private_data->cholesky_factor_dimension_);
      int limit = private_data->cholesky_factor_dimension_;
      int add = private_data->cholesky_factor_dimension_ - 1;
      int row_num = 0;
      int start = 0;
      for(int i = 0; i < private_data->num_cholesky_factor_entries_; i++) {
        if(i == limit) {
          limit += add;
          add--;
          row_num++;
          start = i;
        }
        private_data->cholesky_factor_.at(row_num, row_num + i - start) =
          parameters[private_data->cholesky_factor_dimension_ + i];
      }
    }

  public:

    /** @brief This function is called whenever the parameter changes.
     */
    static void SetupDistribution(
      const arma::vec &parameters, PrivateData *private_data) {

      // Setup the Cholesky factor.
      SetupCholeskyFactor_(parameters, private_data);
    }

    /** @brief This function is called before each beta sample is used
     *         to accumulate the simulated probabilities and the
     *         gradient/Hessians.
     */
    static void SamplingAccumulatePrecompute(
      const arma::vec &parameters, const arma::vec &beta_vector,
      PrivateData *private_data) {

      // Solve. The right hand side is basically beta_vector shifted
      // by the means.
      arma::vec mean_vector(
        const_cast<arma::vec &>(parameters).memptr(),
        beta_vector.n_elem, false);
      arma::vec right_hand_side = beta_vector - mean_vector;
      private_data->cached_solution_ =
        arma::solve(
          private_data->cholesky_factor_, right_hand_side);
    }

    /** @brief Returns the (row, col)-th entry of
     *         $\frac{\partial}{\partial \theta} \beta^{\nu}(\theta)$
     */
    static double AttributeGradientWithRespectToParameter(
      const PrivateData &private_data,
      const arma::vec &parameters, const arma::vec &beta_vector,
      int row_index, int col_index) {

      int num_attributes = beta_vector.n_elem;

      // Upper half of $K \times K$ block is the identity matrix.
      if(row_index < num_attributes) {
        if(row_index == col_index) {
          return 1.0;
        }
        else {
          return 0.0;
        }
      }

      // Lower half.
      else {

        // Locate the non-zero column for this row. If the col_index
        // matches this index, then return a non-zero value.
        int nonzero_column_index =
          private_data.nonzero_column_indices_[row_index];
        if(nonzero_column_index == col_index) {
          return
            private_data.cached_solution_[
              row_index - private_data.start_indices_[row_index] +
              nonzero_column_index];
        }
        else {
          return 0.0;
        }
      }
    }

    /** @brief Draws a new $\beta$ from the Gaussian distribution.
     */
    static void DrawBeta(
      const PrivateData &private_data,
      const arma::vec &parameters, arma::vec *beta_out) {

      arma::vec random_gaussian_vector;
      random_gaussian_vector.set_size(private_data.cholesky_factor_.n_cols);
      for(unsigned int i = 0; i < private_data.cholesky_factor_.n_cols; i++) {
        random_gaussian_vector[i] = core::math::RandGaussian(1.0);
      }

      // Multiply by the Cholesky factor and shift it by the mean.
      arma::vec mean(
        const_cast<arma::vec &>(parameters).memptr(),
        private_data.cholesky_factor_.n_cols, false);
      (*beta_out) =
        private_data.cholesky_factor_ * random_gaussian_vector + mean;
    }

    static void Init(
      const std::vector<int> & attribute_dimensions_in,
      int *num_parameters_out,
      PrivateData *private_data_out) {

      int num_attributes_in = attribute_dimensions_in[0];
      *num_parameters_out = num_attributes_in * (num_attributes_in + 3) / 2;
      private_data_out->cholesky_factor_dimension_ = num_attributes_in;
      private_data_out->num_cholesky_factor_entries_ =
        (private_data_out->cholesky_factor_dimension_) *
        (private_data_out->cholesky_factor_dimension_ + 1) / 2;
      private_data_out->nonzero_column_indices_.resize(*num_parameters_out);
      std::fill(
        private_data_out->nonzero_column_indices_.begin(),
        private_data_out->nonzero_column_indices_.end(), 0);
      private_data_out->start_indices_.resize(*num_parameters_out);
      std::fill(
        private_data_out->start_indices_.begin(),
        private_data_out->start_indices_.end(), 0);

      // Fill out the non-zero column indices for the gradient of the
      // attribute with respect to parameter.
      int limit = private_data_out->cholesky_factor_dimension_;
      int add = private_data_out->cholesky_factor_dimension_ - 1;
      int row_num = 0;
      int start = 0;
      for(int i = 0; i < private_data_out->num_cholesky_factor_entries_; i++) {
        if(i == limit) {
          limit += add;
          add--;
          row_num++;
          start = i;
        }
        private_data_out->nonzero_column_indices_[
          private_data_out->cholesky_factor_dimension_ + i] = row_num;
        private_data_out->start_indices_[
          private_data_out->cholesky_factor_dimension_ + i] =
            private_data_out->cholesky_factor_dimension_ + start;
      }
    }
};
}
}

#endif
