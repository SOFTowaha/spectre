// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/DataBoxTag.hpp"
#include "DataStructures/DataBox/Prefixes.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "Domain/CreateInitialMesh.hpp"
#include "Domain/Direction.hpp"
#include "Domain/Element.hpp"
#include "Domain/InterfaceComputeTags.hpp"
#include "Domain/Mesh.hpp"
#include "Domain/Neighbors.hpp"
#include "Domain/OrientationMap.hpp"
#include "Domain/Tags.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/FluxCommunicationTypes.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/MortarHelpers.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/NormalDotFlux.hpp"
#include "NumericalAlgorithms/DiscontinuousGalerkin/Tags.hpp"
#include "ParallelAlgorithms/Initialization/MergeIntoDataBox.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
namespace Frame {
struct Inertial;
}  // namespace Frame
namespace Parallel {
template <typename Metavariables>
class ConstGlobalCache;
}  // namespace Parallel
/// \endcond

namespace Initialization {
namespace Actions {
/// \ingroup InitializationGroup
/// \brief Initialize items related to the discontinuous Galerkin method.
///
/// Uses:
/// - DataBox:
///   * `Tags::Element<Dim>`
///   * `Tags::Mesh<Dim>`
///   * `Tags::Next<temporal_id_tag>`
///   * `Tags::Interface<Tags::InternalDirections<Dim>, Tags::Mesh<Dim - 1>>`
///
/// DataBox changes:
/// - Adds:
///   * Tags::Interface<Tags::InternalDirections<Dim>,
///                     typename flux_comm_types::normal_dot_fluxes_tag>
///   * Tags::Interface<Tags::BoundaryDirectionsInterior<Dim>,
///                     typename flux_comm_types::normal_dot_fluxes_tag>
///   * Tags::Interface<Tags::BoundaryDirectionsExterior<Dim>,
///                     typename flux_comm_types::normal_dot_fluxes_tag>
/// - Removes: nothing
/// - Modifies: nothing
template <typename Metavariables>
struct DiscontinuousGalerkin {
  static constexpr size_t dim = Metavariables::system::volume_dim;
  using flux_comm_types = dg::FluxCommunicationTypes<Metavariables>;

  template <typename Tag>
  using interface_tag = ::Tags::Interface<::Tags::InternalDirections<dim>, Tag>;

  template <typename Tag>
  using interior_boundary_tag =
      ::Tags::Interface<::Tags::BoundaryDirectionsInterior<dim>, Tag>;

  template <typename Tag>
  using external_boundary_tag =
      ::Tags::Interface<::Tags::BoundaryDirectionsExterior<dim>, Tag>;

  template <typename LocalSystem, bool IsInFluxConservativeForm =
                                      LocalSystem::is_in_flux_conservative_form>
  struct Impl {
    using simple_tags = db::AddSimpleTags<
        interface_tag<typename flux_comm_types::normal_dot_fluxes_tag>,
        interior_boundary_tag<typename flux_comm_types::normal_dot_fluxes_tag>,
        external_boundary_tag<typename flux_comm_types::normal_dot_fluxes_tag>>;
    using compute_tags = db::AddComputeTags<>;

    template <typename TagsList>
    static auto initialize(db::DataBox<TagsList>&& box) noexcept {
      const auto& internal_directions =
          db::get<::Tags::InternalDirections<dim>>(box);

      const auto& boundary_directions =
          db::get<::Tags::BoundaryDirectionsInterior<dim>>(box);

      typename interface_tag<typename flux_comm_types::normal_dot_fluxes_tag>::
          type normal_dot_fluxes_interface{};
      for (const auto& direction : internal_directions) {
        const auto& interface_num_points =
            db::get<interface_tag<::Tags::Mesh<dim - 1>>>(box)
                .at(direction)
                .number_of_grid_points();
        normal_dot_fluxes_interface[direction].initialize(interface_num_points,
                                                          0.);
      }

      typename interior_boundary_tag<
          typename flux_comm_types::normal_dot_fluxes_tag>::type
          normal_dot_fluxes_boundary_exterior{},
          normal_dot_fluxes_boundary_interior{};
      for (const auto& direction : boundary_directions) {
        const auto& boundary_num_points =
            db::get<interior_boundary_tag<::Tags::Mesh<dim - 1>>>(box)
                .at(direction)
                .number_of_grid_points();
        normal_dot_fluxes_boundary_exterior[direction].initialize(
            boundary_num_points, 0.);
        normal_dot_fluxes_boundary_interior[direction].initialize(
            boundary_num_points, 0.);
      }

      return ::Initialization::merge_into_databox<DiscontinuousGalerkin,
                                                  simple_tags, compute_tags>(
          std::move(box), std::move(normal_dot_fluxes_interface),
          std::move(normal_dot_fluxes_boundary_interior),
          std::move(normal_dot_fluxes_boundary_exterior));
    }
  };

  template <typename LocalSystem>
  struct Impl<LocalSystem, true> {
    using simple_tags = db::AddSimpleTags<>;

    template <typename Tag>
    using interface_compute_tag =
        ::Tags::InterfaceCompute<::Tags::InternalDirections<dim>, Tag>;

    template <typename Tag>
    using boundary_interior_compute_tag =
        ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsInterior<dim>, Tag>;

    template <typename Tag>
    using boundary_exterior_compute_tag =
        ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsExterior<dim>, Tag>;

    using char_speed_tag = typename LocalSystem::char_speeds_tag;

    using compute_tags = db::AddComputeTags<
        ::Tags::Slice<::Tags::InternalDirections<dim>,
                      db::add_tag_prefix<::Tags::Flux,
                                         typename LocalSystem::variables_tag,
                                         tmpl::size_t<dim>, Frame::Inertial>>,
        interface_compute_tag<::Tags::NormalDotFluxCompute<
            typename LocalSystem::variables_tag, dim, Frame::Inertial>>,
        interface_compute_tag<char_speed_tag>,
        ::Tags::Slice<::Tags::BoundaryDirectionsInterior<dim>,
                      db::add_tag_prefix<::Tags::Flux,
                                         typename LocalSystem::variables_tag,
                                         tmpl::size_t<dim>, Frame::Inertial>>,
        boundary_interior_compute_tag<::Tags::NormalDotFluxCompute<
            typename LocalSystem::variables_tag, dim, Frame::Inertial>>,
        boundary_interior_compute_tag<char_speed_tag>,
        ::Tags::Slice<::Tags::BoundaryDirectionsExterior<dim>,
                      db::add_tag_prefix<::Tags::Flux,
                                         typename LocalSystem::variables_tag,
                                         tmpl::size_t<dim>, Frame::Inertial>>,
        boundary_exterior_compute_tag<::Tags::NormalDotFluxCompute<
            typename LocalSystem::variables_tag, dim, Frame::Inertial>>,
        boundary_exterior_compute_tag<char_speed_tag>>;

    template <typename TagsList>
    static auto initialize(db::DataBox<TagsList>&& box) noexcept {
      return ::Initialization::merge_into_databox<DiscontinuousGalerkin,
                                                  simple_tags, compute_tags>(
          std::move(box));
    }
  };

  using initialization_tags =
      tmpl::list<::Tags::InitialExtents<Metavariables::volume_dim>>;

  template <typename DbTagsList, typename... InboxTags, typename ArrayIndex,
            typename ActionList, typename ParallelComponent>
  static auto apply(db::DataBox<DbTagsList>& box,
                    const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
                    const Parallel::ConstGlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, ActionList /*meta*/,
                    const ParallelComponent* const /*meta*/) noexcept {
    return std::make_tuple(
        Impl<typename Metavariables::system>::initialize(std::move(box)));
  }
};
}  // namespace Actions
}  // namespace Initialization
