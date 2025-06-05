cmake_minimum_required(VERSION 3.20)
project(hello)

include_directories(include)

aux_source_directory(src COACHECKER_SRC)
list(REMOVE_ITEM COACHECKER_SRC "src/coachecker.c" "src/exp1.c" "src/log_analyzer.c" "src/acoac_instgen.c")# "src/bigflop.c")

link_directories(lib)

add_executable(coachecker src/coachecker.c ${COACHECKER_SRC})

add_executable(instgen src/acoac_instgen.c src/acoac_writer.c src/acoac_utils.c src/hashmap.c src/hashset.c src/hashbasedtable.c src/acoac_rule.c src/acoac_inst.c)

add_executable(exp1 src/exp1.c ${COACHECKER_SRC})

add_executable(log_analyzer src/log_analyzer.c src/acoac_utils.c src/hashmap.c)

target_link_libraries(coachecker PRIVATE ccl m)

target_link_libraries(instgen PRIVATE ccl)

target_link_libraries(exp1 PRIVATE ccl m)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)