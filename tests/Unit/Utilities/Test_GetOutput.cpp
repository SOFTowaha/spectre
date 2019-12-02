// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "tests/Unit/TestingFramework.hpp"

#include <string>
#include <type_traits>

#include "Utilities/GetOutput.hpp"
#include "tests/Unit/TestHelpers.hpp"

SPECTRE_TEST_CASE("Unit.Utilities.GetOutput", "[Utilities][Unit]") {
  CHECK(get_output(3) == "3");
  CHECK(get_output(std::string("abc")) == "abc");
  CHECK(get_output(std::add_const_t<NonCopyable>{}) == "NC");
}
