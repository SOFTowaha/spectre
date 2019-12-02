// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/DiscontinuousGalerkin/Limiters/Minmod.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "Domain/Element.hpp"  // IWYU pragma: keep
#include "Domain/Side.hpp"
#include "Evolution/DiscontinuousGalerkin/Limiters/MinmodHelpers.hpp"
#include "Evolution/DiscontinuousGalerkin/Limiters/MinmodTci.hpp"
#include "NumericalAlgorithms/LinearOperators/Linearize.hpp"
#include "NumericalAlgorithms/LinearOperators/MeanValue.hpp"
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeArray.hpp"

namespace Limiters {
namespace Minmod_detail {

template <size_t VolumeDim>
bool minmod_limited_slopes(
    const gsl::not_null<double*> u_mean,
    const gsl::not_null<std::array<double, VolumeDim>*> u_limited_slopes,
    const gsl::not_null<DataVector*> u_lin_buffer,
    const gsl::not_null<std::array<DataVector, VolumeDim>*> boundary_buffer,
    const Limiters::MinmodType minmod_type, const double tvbm_constant,
    const DataVector& u, const Element<VolumeDim>& element,
    const Mesh<VolumeDim>& mesh,
    const std::array<double, VolumeDim>& element_size,
    const DirectionMap<VolumeDim, double>& effective_neighbor_means,
    const DirectionMap<VolumeDim, double>& effective_neighbor_sizes,
    const std::array<std::pair<gsl::span<std::pair<size_t, size_t>>,
                               gsl::span<std::pair<size_t, size_t>>>,
                     VolumeDim>& volume_and_slice_indices) noexcept {
  const double tvbm_scale = [&tvbm_constant, &element_size ]() noexcept {
    const double max_h =
        *std::max_element(element_size.begin(), element_size.end());
    return tvbm_constant * square(max_h);
  }
  ();

  // Results from SpECTRE paper (https://arxiv.org/abs/1609.00098) used a
  // max_slope_factor a factor of 2.0 too small, so that LambdaPi1 behaved
  // like MUSCL, and MUSCL was even more dissipative.
  const double max_slope_factor =
      (minmod_type == Limiters::MinmodType::Muscl) ? 1.0 : 2.0;

  *u_mean = mean_value(u, mesh);

  const auto difference_to_neighbor = [
    &u_mean, &element, &element_size, &effective_neighbor_means, &
    effective_neighbor_sizes
  ](const size_t dim, const Side& side) noexcept {
    return effective_difference_to_neighbor(
        *u_mean, element, element_size, effective_neighbor_means,
        effective_neighbor_sizes, dim, side);
  };

  // The LambdaPiN limiter calls a simple troubled-cell indicator to avoid
  // limiting solutions that appear smooth:
  if (minmod_type == Limiters::MinmodType::LambdaPiN) {
    const bool u_needs_limiting = troubled_cell_indicator(
        boundary_buffer, tvbm_constant, u, element, mesh, element_size,
        effective_neighbor_means, effective_neighbor_sizes,
        volume_and_slice_indices);

    if (not u_needs_limiting) {
      // Skip the limiting step for this tensor component
#ifdef SPECTRE_DEBUG
      *u_mean = std::numeric_limits<double>::signaling_NaN();
      *u_limited_slopes =
          make_array<VolumeDim>(std::numeric_limits<double>::signaling_NaN());
#endif  // ifdef SPECTRE_DEBUG
      return false;
    }
  }  // end if LambdaPiN

  // If the LambdaPiN check did not skip the limiting, then proceed as normal
  // to determine whether the slopes need to be reduced.
  //
  // Note that we expect the Muscl and LambdaPi1 limiters to linearize the
  // solution whether or not the slope needed reduction. To permit this
  // linearization, we always return (by reference) the slopes when these
  // limiters are in use. In contrast, for LambdaPiN, we only return the slopes
  // when they do in fact need to be reduced.
  bool slopes_need_reducing = false;

  linearize(u_lin_buffer, u, mesh);
  for (size_t d = 0; d < VolumeDim; ++d) {
    const double u_lower =
        mean_value_on_boundary(&(gsl::at(*boundary_buffer, d)),
                               gsl::at(volume_and_slice_indices, d).first,
                               *u_lin_buffer, mesh, d, Side::Lower);
    const double u_upper =
        mean_value_on_boundary(&(gsl::at(*boundary_buffer, d)),
                               gsl::at(volume_and_slice_indices, d).second,
                               *u_lin_buffer, mesh, d, Side::Upper);

    // Divide by element's width (2.0 in logical coordinates) to get a slope
    const double local_slope = 0.5 * (u_upper - u_lower);
    const double upper_slope = 0.5 * difference_to_neighbor(d, Side::Upper);
    const double lower_slope = 0.5 * difference_to_neighbor(d, Side::Lower);

    const MinmodResult result =
        minmod_tvbm(local_slope, max_slope_factor * upper_slope,
                    max_slope_factor * lower_slope, tvbm_scale);
    gsl::at(*u_limited_slopes, d) = result.value;
    if (result.activated) {
      slopes_need_reducing = true;
    }
  }

#ifdef SPECTRE_DEBUG
  // Guard against incorrect use of returned (by reference) slopes in a
  // LambdaPiN limiter, by setting these to NaN when they should not be used.
  if (minmod_type == Limiters::MinmodType::LambdaPiN and
      not slopes_need_reducing) {
    *u_mean = std::numeric_limits<double>::signaling_NaN();
    *u_limited_slopes =
        make_array<VolumeDim>(std::numeric_limits<double>::signaling_NaN());
  }
#endif  // ifdef SPECTRE_DEBUG

  return slopes_need_reducing;
}

// Explicit instantiations
#define DIM(data) BOOST_PP_TUPLE_ELEM(0, data)

#define INSTANTIATE(_, data)                                            \
  template bool minmod_limited_slopes<DIM(data)>(                       \
      const gsl::not_null<double*>,                                     \
      const gsl::not_null<std::array<double, DIM(data)>*>,              \
      const gsl::not_null<DataVector*>,                                 \
      const gsl::not_null<std::array<DataVector, DIM(data)>*>,          \
      const Limiters::MinmodType, const double, const DataVector&,      \
      const Element<DIM(data)>&, const Mesh<DIM(data)>&,                \
      const std::array<double, DIM(data)>&,                             \
      const DirectionMap<DIM(data), double>&,                           \
      const DirectionMap<DIM(data), double>&,                           \
      const std::array<std::pair<gsl::span<std::pair<size_t, size_t>>,  \
                                 gsl::span<std::pair<size_t, size_t>>>, \
                       DIM(data)>&) noexcept;

GENERATE_INSTANTIATIONS(INSTANTIATE, (1, 2, 3))

#undef DIM
#undef INSTANTIATE

}  // namespace Minmod_detail
}  // namespace Limiters
