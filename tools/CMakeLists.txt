if(NOT SUN AND BUILD_SHELL)
  add_subdirectory(shell)
endif()

if(BUILD_SHELL)
  add_executable(plan_serializer utils/plan_serializer.cpp)
  target_link_libraries(plan_serializer duckdb_static
                        ${DUCKDB_EXTRA_LINK_FLAGS})
  link_extension_libraries(plan_serializer "")
endif()

add_executable(parquet_scan_debugger utils/parquet_scan_debugger.cpp)
target_include_directories(parquet_scan_debugger PRIVATE ${CMAKE_SOURCE_DIR}/extension/parquet/include)
target_include_directories(parquet_scan_debugger PRIVATE ${CMAKE_SOURCE_DIR}/third_party/parquet)
target_include_directories(parquet_scan_debugger PRIVATE ${CMAKE_SOURCE_DIR}/third_party/thrift)
target_link_libraries(parquet_scan_debugger duckdb_static ${DUCKDB_EXTRA_LINK_FLAGS})
link_extension_libraries(parquet_scan_debugger "parquet" PRIVATE)

add_executable(sql_runner utils/sql_runner.cpp)
target_link_libraries(sql_runner duckdb ${DUCKDB_EXTRA_LINK_FLAGS})

# add_executable(benchmark_vector_operations utils/benchmark_vector_operations.cpp)
# target_link_libraries(benchmark_vector_operations duckdb_static ${DUCKDB_EXTRA_LINK_FLAGS})
# link_extension_libraries(benchmark_vector_operations "")

# add_executable(test_zoned_sel utils/test_zoned_sel.cpp)
# target_link_libraries(test_zoned_sel duckdb_static ${DUCKDB_EXTRA_LINK_FLAGS})
# link_extension_libraries(test_zoned_sel "")
