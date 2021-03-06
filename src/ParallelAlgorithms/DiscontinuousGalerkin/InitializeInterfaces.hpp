// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/DataBoxTag.hpp"
#include "DataStructures/Tensor/EagerMath/Magnitude.hpp"
#include "Domain/FaceNormal.hpp"
#include "Domain/InterfaceComputeTags.hpp"
#include "Domain/Tags.hpp"
#include "ParallelAlgorithms/Initialization/MergeIntoDataBox.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

/// \cond
namespace Parallel {
template <typename Metavariables>
class ConstGlobalCache;
}  // namespace Parallel
/// \endcond

namespace dg {
namespace Initialization {
/// \ingroup InitializationGroup
/// %Tags that are to be sliced to the faces of the element
template <typename... Tags>
using slice_tags_to_face = tmpl::list<Tags...>;

/// \ingroup InitializationGroup
/// %Tags that are to be sliced to the exterior side of the faces of the element
template <typename... Tags>
using slice_tags_to_exterior = tmpl::list<Tags...>;

/// \ingroup InitializationGroup
/// Compute tags on the faces of the element
template <typename... Tags>
using face_compute_tags = tmpl::list<Tags...>;

/// \ingroup InitializationGroup
/// Compute tags on the exterior side of the faces of the element
template <typename... Tags>
using exterior_compute_tags = tmpl::list<Tags...>;
}  // namespace Initialization

namespace Actions {

namespace InitializeInterfaces_detail {
template <bool Enable, typename Metavariables>
struct InitExteriorVarsImpl {
  template <typename DbTagsList>
  static db::DataBox<DbTagsList>&& apply(
      db::DataBox<DbTagsList>&& box) noexcept {
    return std::move(box);
  }
};

template <typename Metavariables>
struct InitExteriorVarsImpl<true, Metavariables> {
  template <typename DbTagsList>
  static auto apply(db::DataBox<DbTagsList>&& box) noexcept {
    using system = typename Metavariables::system;
    static constexpr size_t dim = system::volume_dim;
    using vars_tag = typename system::variables_tag;
    using exterior_vars_tag =
        ::Tags::Interface<::Tags::BoundaryDirectionsExterior<dim>, vars_tag>;

    db::item_type<exterior_vars_tag> exterior_boundary_vars{};
    const auto& mesh = db::get<::Tags::Mesh<dim>>(box);
    for (const auto& direction :
         db::get<::Tags::Element<dim>>(box).external_boundaries()) {
      exterior_boundary_vars[direction] = db::item_type<vars_tag>{
          mesh.slice_away(direction.dimension()).number_of_grid_points()};
    }
    return ::Initialization::merge_into_databox<
        InitExteriorVarsImpl, db::AddSimpleTags<exterior_vars_tag>>(
        std::move(box), std::move(exterior_boundary_vars));
  }
};
}  // namespace InitializeInterfaces_detail

/// \ingroup InitializationGroup
/// \brief Initialize items related to the interfaces between Elements and on
/// external boundaries
///
/// The `dg::Initialization::slice_tags_to_face` and
/// `dg::Initialization::slice_tags_to_exterior` types should be used for the
/// `SliceTagsToFace` and `SliceTagsToExterior` template parameters,
/// respectively. These are used to set additional volume quantities that are to
/// be sliced to the interfaces. Compute tags on the interfaces are controlled
/// using the `FaceComputeTags` and `ExteriorComputeTags` template parameters,
/// which should be passed the `dg::Initialization::face_compute_tags` and
/// `dg::Initialization::exterior_compute_tags` types, respectively.
///
/// By default, this initializer also adds the system's `variables_tag` on
/// exterior (ghost) boundary faces. These are stored in a simple tag and
/// updated manually to impose boundary conditions. Set the
/// `AddExteriorVariables` template parameter to `false` to disable this
/// behavior.
///
/// Uses:
/// - System:
///   * `volume_dim`
///   * `variables_tag`
///   * `magnitude_tag`
///
/// DataBox changes:
/// - Adds:
///   * `Tags::Interface<Tags::BoundaryDirectionsExterior<volume_dim>,
///   variables_tag>` (as a simple tag)
///   * `face_tags<Tags::InternalDirections<Dim>>`
///   * `face_tags<Tags::BoundaryDirectionsInterior<Dim>>`
///   * `face_tags<Tags::BoundaryDirectionsExterior<Dim>>`
///
/// - For `face_tags<Directions>`:
///   * `Directions`
///   * `Tags::Interface<Directions, Tags::Directions<Dim>>`
///   * `Tags::Interface<Directions, Tags::Mesh<Dim - 1>>`
///   * `Tags::Interface<Directions, Tags::Coordinates<Dim, Frame::Inertial>>`
///   (only on exterior faces)
///   * `Tags::Interface<Directions, Tags::UnnormalizedFaceNormal<Dim>>`
///   * `Tags::Interface<Directions, Tags::Magnitude<
///   Tags::UnnormalizedFaceNormal<Dim>>>`
///   * `Tags::Interface<Directions, Tags::Normalized<
///   Tags::UnnormalizedFaceNormal<Dim>>>`
///
/// - Removes: nothing
/// - Modifies: nothing
template <
    typename System,
    typename SliceTagsToFace = Initialization::slice_tags_to_face<>,
    typename SliceTagsToExterior = Initialization::slice_tags_to_exterior<>,
    typename FaceComputeTags = Initialization::face_compute_tags<>,
    typename ExteriorComputeTags = Initialization::exterior_compute_tags<>,
    bool AddExteriorVariables = true>
struct InitializeInterfaces {
 private:
  static constexpr size_t dim = System::volume_dim;

  template <typename TagToSlice, typename Directions>
  struct make_slice_tag {
    using type = ::Tags::Slice<Directions, TagToSlice>;
  };

  template <typename ComputeTag, typename Directions>
  struct make_compute_tag {
    using type = ::Tags::InterfaceCompute<Directions, ComputeTag>;
  };

  template <typename Directions>
  using face_tags = tmpl::flatten<tmpl::list<
      Directions, ::Tags::InterfaceCompute<Directions, ::Tags::Direction<dim>>,
      ::Tags::InterfaceCompute<Directions, ::Tags::InterfaceMesh<dim>>,
      tmpl::transform<SliceTagsToFace,
                      make_slice_tag<tmpl::_1, tmpl::pin<Directions>>>,
      ::Tags::InterfaceCompute<Directions,
                               ::Tags::UnnormalizedFaceNormalCompute<dim>>,
      ::Tags::InterfaceCompute<Directions,
                               typename System::template magnitude_tag<
                                   ::Tags::UnnormalizedFaceNormal<dim>>>,
      ::Tags::InterfaceCompute<
          Directions,
          ::Tags::NormalizedCompute<::Tags::UnnormalizedFaceNormal<dim>>>,
      tmpl::transform<FaceComputeTags,
                      make_compute_tag<tmpl::_1, tmpl::pin<Directions>>>>>;

  using exterior_face_tags = tmpl::flatten<tmpl::list<
      ::Tags::BoundaryDirectionsExterior<dim>,
      ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsExterior<dim>,
                               ::Tags::Direction<dim>>,
      ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsExterior<dim>,
                               ::Tags::InterfaceMesh<dim>>,
      tmpl::transform<
          SliceTagsToExterior,
          make_slice_tag<tmpl::_1,
                         tmpl::pin<::Tags::BoundaryDirectionsExterior<dim>>>>,
      ::Tags::InterfaceCompute<
          ::Tags::BoundaryDirectionsExterior<dim>,
          ::Tags::BoundaryCoordinates<dim, Frame::Inertial>>,
      ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsExterior<dim>,
                               ::Tags::UnnormalizedFaceNormalCompute<dim>>,
      ::Tags::InterfaceCompute<::Tags::BoundaryDirectionsExterior<dim>,
                               typename System::template magnitude_tag<
                                   ::Tags::UnnormalizedFaceNormal<dim>>>,
      ::Tags::InterfaceCompute<
          ::Tags::BoundaryDirectionsExterior<dim>,
          ::Tags::NormalizedCompute<::Tags::UnnormalizedFaceNormal<dim>>>,
      tmpl::transform<
          ExteriorComputeTags,
          make_compute_tag<
              tmpl::_1, tmpl::pin<::Tags::BoundaryDirectionsExterior<dim>>>>>>;

 public:
  template <typename DbTagsList, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static auto apply(db::DataBox<DbTagsList>& box,
                    const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
                    const Parallel::ConstGlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, ActionList /*meta*/,
                    const ParallelComponent* const /*meta*/) noexcept {
    using compute_tags =
        tmpl::append<face_tags<::Tags::InternalDirections<dim>>,
                     face_tags<::Tags::BoundaryDirectionsInterior<dim>>,
                     exterior_face_tags>;
    return std::make_tuple(
        ::Initialization::merge_into_databox<InitializeInterfaces,
                                             db::AddSimpleTags<>, compute_tags>(
            InitializeInterfaces_detail::InitExteriorVarsImpl<
                AddExteriorVariables, Metavariables>::apply(std::move(box))));
  }
};
}  // namespace Actions
}  // namespace dg
