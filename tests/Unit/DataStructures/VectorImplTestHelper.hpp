// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <array>
#include <cstddef>
#include <memory>  // IWYU pragma: keep
#include <tuple>
#include <utility>

#include "DataStructures/VectorImpl.hpp"
#include "Utilities/Algorithm.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/Requires.hpp"
#include "Utilities/Tuple.hpp"
#include "tests/Unit/TestHelpers.hpp"
#include "tests/Unit/TestingFramework.hpp"
#include "tests/Utilities/MakeWithRandomValues.hpp"

namespace TestHelpers {
namespace VectorImpl {

/// \ingroup TestingFrameworkGroup
/// \brief test construction and assignment of a `VectorType` with a `ValueType`
template <typename VectorType, typename ValueType>
void vector_test_construct_and_assign(
    tt::get_fundamental_type_t<ValueType> low =
        tt::get_fundamental_type_t<ValueType>{-100.0},
    tt::get_fundamental_type_t<ValueType> high =
        tt::get_fundamental_type_t<ValueType>{100.0}) noexcept {
  MAKE_GENERATOR(gen);
  UniformCustomDistribution<tt::get_fundamental_type_t<ValueType>> dist{low,
                                                                        high};
  UniformCustomDistribution<size_t> sdist{2, 20};

  const size_t size = sdist(gen);

  const VectorType size_constructed{size};
  CHECK(size_constructed.size() == size);
  const auto generated_value1 = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));

  const VectorType value_size_constructed{size, generated_value1};
  CHECK(value_size_constructed.size() == size);
  alg::for_each(value_size_constructed, [
    generated_value1, &value_size_constructed
  ](typename VectorType::value_type element) noexcept {
    CAPTURE_PRECISE(value_size_constructed);
    CHECK(element == generated_value1);
  });

  // random generation must use `make_with_random_values`, because stored value
  // in vector type might be a non-fundamental type.
  const auto generated_value2 = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));
  const auto generated_value3 = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));

  VectorType initializer_list_constructed{
      {static_cast<typename VectorType::value_type>(generated_value2),
       static_cast<typename VectorType::value_type>(generated_value3)}};
  CHECK(initializer_list_constructed.size() == 2);
  CHECK(initializer_list_constructed.is_owning());
  CHECK(gsl::at(initializer_list_constructed, 0) == generated_value2);
  CHECK(gsl::at(initializer_list_constructed, 1) == generated_value3);

  typename VectorType::value_type raw_ptr[2] = {generated_value2,
                                                generated_value3};
  const VectorType pointer_size_constructed{
      static_cast<typename VectorType::value_type*>(raw_ptr), 2};
  CHECK(initializer_list_constructed == pointer_size_constructed);
  CHECK_FALSE(initializer_list_constructed != pointer_size_constructed);

  test_copy_semantics(initializer_list_constructed);
  auto initializer_list_constructed_copy = initializer_list_constructed;
  CHECK(initializer_list_constructed_copy.is_owning());
  CHECK(initializer_list_constructed_copy == pointer_size_constructed);
  test_move_semantics(std::move(initializer_list_constructed),
                      initializer_list_constructed_copy);

  VectorType move_assignment_initialized;
  move_assignment_initialized = std::move(initializer_list_constructed_copy);
  CHECK(move_assignment_initialized.is_owning());

  const VectorType move_constructed{std::move(move_assignment_initialized)};
  CHECK(move_constructed.is_owning());
  CHECK(move_constructed == pointer_size_constructed);

  // clang-tidy has performance complaints, and we're checking functionality
  const VectorType copy_constructed{move_constructed};  // NOLINT
  CHECK(copy_constructed.is_owning());
  CHECK(copy_constructed == pointer_size_constructed);
}

/// \ingroup TestingFrameworkGroup
/// \brief test the serialization of a `VectorType` constructed with a
/// `ValueType`
template <typename VectorType, typename ValueType>
void vector_test_serialize(tt::get_fundamental_type_t<ValueType> low =
                               tt::get_fundamental_type_t<ValueType>{-100.0},
                           tt::get_fundamental_type_t<ValueType> high =
                               tt::get_fundamental_type_t<ValueType>{
                                   100.0}) noexcept {
  MAKE_GENERATOR(gen);
  UniformCustomDistribution<tt::get_fundamental_type_t<ValueType>> dist{low,
                                                                        high};
  UniformCustomDistribution<size_t> sdist{2, 20};

  const size_t size = sdist(gen);
  VectorType vector_test{size}, vector_control{size};
  VectorType vector_ref;
  const auto start_value = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));
  const auto value_difference = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));
  // generate_series is used to generate a pair of equivalent, but independently
  // constructed, data sets to fill the vectors with.
  ValueType current_value = start_value;
  const auto generate_series = [&current_value, value_difference ]() noexcept {
    return current_value += value_difference;
  };
  std::generate(vector_test.begin(), vector_test.end(), generate_series);
  current_value = start_value;
  std::generate(vector_control.begin(), vector_control.end(), generate_series);
  // checks the vectors have been constructed as expected
  CHECK(vector_control == vector_test);
  CHECK(vector_test.is_owning());
  CHECK(vector_control.is_owning());
  const VectorType serialized_vector_test =
      serialize_and_deserialize(vector_test);
  // check that the vector is unaltered by serialize_and_deserialize
  CHECK(vector_control == vector_test);
  CHECK(serialized_vector_test == vector_control);
  CHECK(serialized_vector_test.is_owning());
  CHECK(serialized_vector_test.data() != vector_test.data());
  CHECK(vector_test.is_owning());
  // checks serialization for reference
  vector_ref.set_data_ref(&vector_test);
  CHECK(vector_test.is_owning());
  CHECK_FALSE(vector_ref.is_owning());
  CHECK(vector_ref == vector_test);
  const VectorType serialized_vector_ref =
      serialize_and_deserialize(vector_ref);
  CHECK(vector_test.is_owning());
  CHECK(vector_test == vector_control);
  CHECK(vector_ref == vector_test);
  CHECK(serialized_vector_ref == vector_test);
  CHECK(serialized_vector_ref.is_owning());
  CHECK(serialized_vector_ref.data() != vector_ref.data());
  CHECK_FALSE(vector_ref.is_owning());
}

/// \ingroup TestingFrameworkGroup
/// \brief test the construction and move of a reference `VectorType`
/// constructed with a `ValueType`
template <typename VectorType, typename ValueType>
void vector_test_ref(tt::get_fundamental_type_t<ValueType> low =
                         tt::get_fundamental_type_t<ValueType>{-100.0},
                     tt::get_fundamental_type_t<ValueType> high =
                         tt::get_fundamental_type_t<ValueType>{
                             100.0}) noexcept {
  MAKE_GENERATOR(gen);
  UniformCustomDistribution<tt::get_fundamental_type_t<ValueType>> dist{low,
                                                                        high};
  UniformCustomDistribution<size_t> sdist{2, 20};

  const size_t size = sdist(gen);
  VectorType original_vector = make_with_random_values<VectorType>(
      make_not_null(&gen), make_not_null(&dist), VectorType{size});

  {
    INFO("Check construction, copy, move, and ownership of reference vectors")
    VectorType ref_vector;
    ref_vector.set_data_ref(&original_vector);
    CHECK_FALSE(ref_vector.is_owning());
    CHECK(original_vector.is_owning());
    CHECK(ref_vector.data() == original_vector.data());

    const VectorType data_check{original_vector};
    CHECK(ref_vector.size() == size);
    CHECK(ref_vector == data_check);
    test_copy_semantics(ref_vector);

    VectorType ref_vector_copy;
    ref_vector_copy.set_data_ref(&ref_vector);
    test_move_semantics(std::move(ref_vector), ref_vector_copy);
    VectorType move_assignment_initialized;
    move_assignment_initialized = std::move(ref_vector_copy);
    CHECK(not move_assignment_initialized.is_owning());
    const VectorType move_constructed{std::move(move_assignment_initialized)};
    CHECK(not move_constructed.is_owning());
  }
  {
    INFO("Check move acts appropriately on both source and target refs")
    VectorType ref_original_vector;
    ref_original_vector.set_data_ref(&original_vector);
    VectorType generated_vector = make_with_random_values<VectorType>(
        make_not_null(&gen), make_not_null(&dist), VectorType{size});
    const VectorType generated_vector_copy = generated_vector;
    ref_original_vector = std::move(generated_vector);
    // clang-tidy : Intentionally testing use after move
    CHECK(original_vector != generated_vector);  // NOLINT
    CHECK(original_vector == generated_vector_copy);
// Intentionally testing self-move
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif  // defined(__clang__)
    ref_original_vector = std::move(ref_original_vector);
#ifdef __clang__
#pragma GCC diagnostic pop
#endif  // defined(__clang__)
    CHECK(original_vector == generated_vector_copy);
    // clang-tidy: false positive, used after it was moved
    const VectorType data_check_vector = ref_original_vector;  // NOLINT
    CHECK(data_check_vector == generated_vector_copy);
  }
  {
    INFO("Check math affects both data vectors which share a ref")
    const auto generated_value1 = make_with_random_values<ValueType>(
        make_not_null(&gen), make_not_null(&dist));
    const auto generated_value2 = make_with_random_values<ValueType>(
        make_not_null(&gen), make_not_null(&dist));
    const auto sum_generated_values = generated_value1 + generated_value2;
    VectorType sharing_vector{size, generated_value1};
    VectorType owning_vector{size, generated_value2};
    sharing_vector.set_data_ref(&owning_vector);
    sharing_vector = sharing_vector + generated_value1;
    CHECK_ITERABLE_APPROX(owning_vector,
                          (VectorType{size, sum_generated_values}));
    CHECK_ITERABLE_APPROX(sharing_vector,
                          (VectorType{size, sum_generated_values}));
  }
}

enum RefSizeErrorTestKind { Copy, ExpressionAssign, Move };

/// \ingroup TestingFrameworkGroup
/// \brief Test that assigning to a non-owning `VectorType` of the wrong size
/// appropriately generates an error.
///
/// \details a calling function should be an `ASSERTION_TEST()` and check for
/// the string "Must copy into same size".
/// Three types of tests are provided and one must be provided as the first
/// function argument:
/// - `RefSizeErrorTestKind::Copy`: Checks that copy-assigning to a non-owning
/// `VectorType` from a `VectorType` with the wrong size generates an error.
/// - `RefSizeErrorTestKind::ExpressionAssign`: Checks that assigning to a
/// non-owning `VectorType` from an expression with alias `ResultType` of
/// `VectorType` with the wrong size generates an error
/// - `RefSizeErrorTestKind::Move`: Checks that move-assigning to a non-owning
/// `VectorType` from a `VectorType` with the wrong size generates an error.
template <typename VectorType,
          typename ValueType = typename VectorType::ElementType>
void vector_ref_test_size_error(
    RefSizeErrorTestKind test_kind,
    tt::get_fundamental_type_t<ValueType> low =
        tt::get_fundamental_type_t<ValueType>{-100.0},
    tt::get_fundamental_type_t<ValueType> high =
        tt::get_fundamental_type_t<ValueType>{100.0}) noexcept {
  MAKE_GENERATOR(gen);
  UniformCustomDistribution<tt::get_fundamental_type_t<ValueType>> dist{low,
                                                                        high};
  UniformCustomDistribution<size_t> sdist{2, 20};

  const size_t size = sdist(gen);
  VectorType generated_vector = make_with_random_values<VectorType>(
      make_not_null(&gen), make_not_null(&dist), VectorType{size});
  VectorType ref_generated_vector;
  ref_generated_vector.set_data_ref(&generated_vector);
  const VectorType larger_generated_vector =
      make_with_random_values<VectorType>(
          make_not_null(&gen), make_not_null(&dist), VectorType{size + 1});
  // each of the following options should error, the reference should have
  // received the wrong size
  if (test_kind == RefSizeErrorTestKind::Copy) {
    ref_generated_vector = larger_generated_vector;
  }
  if (test_kind == RefSizeErrorTestKind::ExpressionAssign) {
    ref_generated_vector = (larger_generated_vector + larger_generated_vector);
  }
  if (test_kind == RefSizeErrorTestKind::Move) {
    ref_generated_vector = std::move(larger_generated_vector);
  }
}

/// \ingroup TestingFrameworkGroup
/// \brief tests a small sample of math functions after a move of a
/// `VectorType` initialized with `ValueType`
template <typename VectorType, typename ValueType>
void vector_test_math_after_move(
    tt::get_fundamental_type_t<ValueType> low =
        tt::get_fundamental_type_t<ValueType>{-100.0},
    tt::get_fundamental_type_t<ValueType> high =
        tt::get_fundamental_type_t<ValueType>{100.0}) noexcept {
  MAKE_GENERATOR(gen);
  UniformCustomDistribution<tt::get_fundamental_type_t<ValueType>> dist{low,
                                                                        high};
  UniformCustomDistribution<size_t> sdist{2, 20};

  const size_t size = sdist(gen);
  const auto generated_value1 = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));
  const auto generated_value2 = make_with_random_values<ValueType>(
      make_not_null(&gen), make_not_null(&dist));
  const auto sum_generated_values = generated_value1 + generated_value2;
  const auto difference_generated_values = generated_value1 - generated_value2;

  const VectorType vector_math_lhs{size, generated_value1},
      vector_math_rhs{size, generated_value2};
  {
    INFO("Check move assignment and use after move");
    VectorType from_vector = make_with_random_values<VectorType>(
        make_not_null(&gen), make_not_null(&dist), VectorType{size});
    VectorType to_vector{};
    to_vector = std::move(from_vector);
    to_vector = vector_math_lhs + vector_math_rhs;
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, sum_generated_values}));
    // clang-tidy: use after move (intentional here)
    CHECK(from_vector.size() == 0);  // NOLINT
    CHECK(from_vector.is_owning());
    from_vector = vector_math_lhs - vector_math_rhs;
    CHECK_ITERABLE_APPROX(from_vector,
                          (VectorType{size, difference_generated_values}));
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, sum_generated_values}));
  }
  {
    INFO("Check move assignment and value of target")
    auto from_value = make_with_random_values<ValueType>(make_not_null(&gen),
                                                         make_not_null(&dist));
    VectorType from_vector{size, from_value};
    VectorType to_vector{};
    to_vector = std::move(from_vector);
    from_vector = vector_math_lhs + vector_math_rhs;
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, from_value}));
    CHECK_ITERABLE_APPROX(from_vector,
                          (VectorType{size, sum_generated_values}));
  }
  {
    INFO("Check move constructor and use after move")
    VectorType from_vector = make_with_random_values<VectorType>(
        make_not_null(&gen), make_not_null(&dist), VectorType{size});
    VectorType to_vector{std::move(from_vector)};
    to_vector = vector_math_lhs + vector_math_rhs;
    CHECK(to_vector.size() == size);
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, sum_generated_values}));
    // clang-tidy: use after move (intentional here)
    CHECK(from_vector.size() == 0);  // NOLINT
    CHECK(from_vector.is_owning());
    from_vector = vector_math_lhs - vector_math_rhs;
    CHECK_ITERABLE_APPROX(from_vector,
                          (VectorType{size, difference_generated_values}));
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, sum_generated_values}));
  }

  {
    INFO("Check move constructor and value of target")
    auto from_value = make_with_random_values<ValueType>(make_not_null(&gen),
                                                         make_not_null(&dist));
    VectorType from_vector{size, from_value};
    const VectorType to_vector{std::move(from_vector)};
    from_vector = vector_math_lhs + vector_math_rhs;
    CHECK_ITERABLE_APPROX(to_vector, (VectorType{size, from_value}));
    CHECK_ITERABLE_APPROX(from_vector,
                          (VectorType{size, sum_generated_values}));
  }
}
}  // namespace VectorImpl
}  // namespace TestHelpers
