# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_NumericalDiscontinuousGalerkin")

set(LIBRARY_SOURCES
  Test_LiftFlux.cpp
  Test_MortarHelpers.cpp
  Test_NormalDotFlux.cpp
  Test_SimpleBoundaryData.cpp
  Test_Tags.cpp
  )

add_subdirectory(Actions)
add_subdirectory(NumericalFluxes)

add_test_library(
  ${LIBRARY}
  "NumericalAlgorithms/DiscontinuousGalerkin/"
  "${LIBRARY_SOURCES}"
  "CoordinateMaps;DataStructures;Domain;DiscontinuousGalerkin;ErrorHandling;Utilities"
  )

add_dependencies(
  ${LIBRARY}
  module_ConstGlobalCache
  )
