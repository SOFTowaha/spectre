// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>

/// \cond
namespace Poisson {
template <size_t Dim>
struct FirstOrderSystem;
namespace Solutions {
template <size_t Dim>
struct Lorentzian;
template <size_t Dim>
struct Moustache;
template <size_t Dim>
struct ProductOfSinusoids;
}  // namespace Solutions
}  // namespace Poisson

template <typename System, typename InitialGuess, typename BoundaryConditions>
struct Metavariables;
/// \endcond
