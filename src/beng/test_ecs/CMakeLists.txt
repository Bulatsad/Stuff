# test_ecs - ECS Test Application
cmake_minimum_required(VERSION 3.29)

# Создать executable
add_executable(test_ecs main.cpp)

# Линковка с beng-core (автоматически подтянет blib)
target_link_libraries(test_ecs PRIVATE beng-core)

# Include directories (beng-core уже предоставляет публичные includes)
# target_include_directories уже не нужен — beng-core PUBLIC includes покрывают всё

# Compiler flags уже установлены глобально
