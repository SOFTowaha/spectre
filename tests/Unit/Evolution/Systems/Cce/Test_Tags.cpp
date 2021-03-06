// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "tests/Unit/TestingFramework.hpp"

#include <string>

#include "Evolution/Systems/Cce/Tags.hpp"

namespace {
struct SomeTag {
  using type = Scalar<SpinWeighted<ComplexDataVector, 0>>;
};
}  // namespace

SPECTRE_TEST_CASE("Unit.Evolution.Systems.Cce.Tags", "[Unit][Evolution]") {
  CHECK(db::tag_name<Cce::Tags::Dy<SomeTag>>() == "Dy(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::Du<SomeTag>>() == "Du(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::Dr<SomeTag>>() == "Dr(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::Integrand<SomeTag>>() == "Integrand(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::BoundaryValue<SomeTag>>() ==
        "BoundaryValue(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::PoleOfIntegrand<SomeTag>>() ==
        "PoleOfIntegrand(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::LinearFactor<SomeTag>>() ==
        "LinearFactor(SomeTag)");
  CHECK(db::tag_name<Cce::Tags::LinearFactorForConjugate<SomeTag>>() ==
        "LinearFactorForConjugate(SomeTag)");
}
