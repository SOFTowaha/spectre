# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_EvolutionConservative")

set(LIBRARY_SOURCES
  Test_ConservativeDuDt.cpp
  )

add_test_library(
  ${LIBRARY}
  "Evolution/Conservative/"
  "${LIBRARY_SOURCES}"
  "DataStructures;ErrorHandling"
  )
