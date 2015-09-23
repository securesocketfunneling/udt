cmake_minimum_required(VERSION 2.8)

set(project_NAME "timer_benchmark")
project(${project_NAME})

set(TIMER_FILES
      high_precision_timer_scheduler.hpp
      detail/high_precision_timer_scheduler.ipp
      main.cpp)

include_directories(
  ${Boost_INCLUDE_DIRS})

add_target("timer_benchmark"
  TYPE
    executable ${EXEC_FLAG} INSTALL
  LINK
    ${Boost_LIBRARIES}
    ${PLATFORM_SPECIFIC_LIB_DEP}
    PREFIX_SKIP     .*/src
    HEADER_FILTER   "\\.h(h|m|pp|xx|\\+\\+)?" 
  FILES
    ${TIMER_FILES}
)

target_link_libraries(timer_benchmark ${Boost_LIBRARIES} ${PLATFORM_SPECIFIC_LIB_DEP})
