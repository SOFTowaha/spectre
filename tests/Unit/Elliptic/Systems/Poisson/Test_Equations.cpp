// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "tests/Unit/TestingFramework.hpp"

#include <array>
#include <cstddef>
#include <string>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/DataBoxTag.hpp"
#include "DataStructures/DataBox/Prefixes.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Elliptic/Systems/Poisson/Equations.hpp"
#include "Elliptic/Systems/Poisson/Tags.hpp"
#include "NumericalAlgorithms/LinearOperators/PartialDerivatives.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeString.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/TMPL.hpp"
#include "tests/Unit/Pypp/CheckWithRandomValues.hpp"
#include "tests/Unit/Pypp/SetupLocalPythonEnvironment.hpp"

namespace {

template <size_t Dim>
void test_equations(const DataVector& used_for_size) {
  pypp::check_with_random_values<1>(&Poisson::euclidean_fluxes<Dim>,
                                    "Equations", {"euclidean_fluxes"},
                                    {{{0., 1.}}}, used_for_size);
  pypp::check_with_random_values<1>(&Poisson::noneuclidean_fluxes<Dim>,
                                    "Equations", {"noneuclidean_fluxes"},
                                    {{{0., 1.}}}, used_for_size);
  pypp::check_with_random_values<1>(
      &Poisson::auxiliary_fluxes<Dim>, "Equations",
      {MakeString{} << "auxiliary_fluxes_" << Dim << "d"}, {{{0., 1.}}},
      used_for_size);
}

template <size_t Dim>
void test_computers(const DataVector& used_for_size) {
  using field_tag = Poisson::Tags::Field;
  using auxiliary_field_tag =
      ::Tags::deriv<field_tag, tmpl::size_t<Dim>, Frame::Inertial>;
  using field_flux_tag =
      ::Tags::Flux<field_tag, tmpl::size_t<Dim>, Frame::Inertial>;
  using auxiliary_flux_tag =
      ::Tags::Flux<auxiliary_field_tag, tmpl::size_t<Dim>, Frame::Inertial>;
  using field_source_tag = ::Tags::Source<field_tag>;

  const size_t num_points = used_for_size.size();
  {
    INFO("EuclideanFluxes" << Dim << "D");
    auto box =
        db::create<db::AddSimpleTags<field_tag, auxiliary_field_tag,
                                     field_flux_tag, auxiliary_flux_tag>>(
            Scalar<DataVector>{num_points, 2.},
            tnsr::i<DataVector, Dim>{num_points, 3.},
            tnsr::I<DataVector, Dim>{num_points, 0.},
            tnsr::Ij<DataVector, Dim>{num_points, 0.});

    const Poisson::EuclideanFluxes<Dim> fluxes_computer{};
    using argument_tags = typename Poisson::EuclideanFluxes<Dim>::argument_tags;

    db::mutate_apply<tmpl::list<field_flux_tag>, argument_tags>(
        fluxes_computer, make_not_null(&box), get<auxiliary_field_tag>(box));
    auto expected_field_flux = tnsr::I<DataVector, Dim>{num_points, 0.};
    Poisson::euclidean_fluxes(make_not_null(&expected_field_flux),
                              get<auxiliary_field_tag>(box));
    CHECK(get<field_flux_tag>(box) == expected_field_flux);

    db::mutate_apply<tmpl::list<auxiliary_flux_tag>, argument_tags>(
        fluxes_computer, make_not_null(&box), get<field_tag>(box));
    auto expected_auxiliary_flux = tnsr::Ij<DataVector, Dim>{num_points, 0.};
    Poisson::auxiliary_fluxes(make_not_null(&expected_auxiliary_flux),
                              get<field_tag>(box));
    CHECK(get<auxiliary_flux_tag>(box) == expected_auxiliary_flux);
  }
  {
    INFO("Sources" << Dim << "D");
    auto box = db::create<db::AddSimpleTags<field_tag, field_source_tag>>(
        Scalar<DataVector>{num_points, 2.}, Scalar<DataVector>{num_points, 0.});

    const Poisson::Sources sources_computer{};
    using argument_tags = typename Poisson::Sources::argument_tags;

    db::mutate_apply<tmpl::list<field_source_tag>, argument_tags>(
        sources_computer, make_not_null(&box), get<field_tag>(box));
    auto expected_field_source = Scalar<DataVector>{num_points, 0.};
    CHECK(get<field_source_tag>(box) == expected_field_source);
  }
}

}  // namespace

SPECTRE_TEST_CASE("Unit.Elliptic.Systems.Poisson", "[Unit][Elliptic]") {
  pypp::SetupLocalPythonEnvironment local_python_env{
      "Elliptic/Systems/Poisson"};

  GENERATE_UNINITIALIZED_DATAVECTOR;
  CHECK_FOR_DATAVECTORS(test_equations, (1, 2, 3));
  CHECK_FOR_DATAVECTORS(test_computers, (1, 2, 3));
}
