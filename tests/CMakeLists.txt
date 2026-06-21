add_executable(xtodo_model_tests
    todo_model_tests.cpp
    ../src/TodoModel.cpp
)

target_include_directories(xtodo_model_tests PRIVATE ../src)
target_compile_features(xtodo_model_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(xtodo_model_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(xtodo_model_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME xtodo_model_tests COMMAND xtodo_model_tests)
