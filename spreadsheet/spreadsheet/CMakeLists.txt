cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project(spreadsheet CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /JMC")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-implicit-fallthrough")
endif()

# Для лучшего автодополнения (VSCode/Qt Creator)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Пути к ANTLR
set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr-4.13.2-complete.jar)
include(${CMAKE_CURRENT_SOURCE_DIR}/FindANTLR.cmake)

# Рантайм ANTLR (статическая линковка)
add_definitions(-DANTLR4CPP_STATIC -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
set(WITH_STATIC_CRT OFF CACHE BOOL "Visual C++ static CRT for ANTLR" FORCE)
add_subdirectory(antlr4_runtime)

# Явно задаём каталог для генерации — удобно для IDE
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Генерация C++ файлов из грамматики Formula.g4
# Ваша FindANTLR.cmake поддерживает OUTPUT_DIRECTORY и экспортирует:
#   ANTLR_FormulaParser_OUTPUT_DIR и ANTLR_FormulaParser_CXX_OUTPUTS
antlr_target(FormulaParser
        Formula.g4
        LEXER PARSER LISTENER
        OUTPUT_DIRECTORY ${GEN_DIR}
)

# Проектные исходники из корня (не затрагиваем сгенерированные)
file(GLOB sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(spreadsheet
        ${sources}
        ${ANTLR_FormulaParser_CXX_OUTPUTS}  # добавляем сгенерированные .cpp/.h
)

# Помечаем как сгенерированные, чтобы IDE не ругалась до первой сборки
set_source_files_properties(${ANTLR_FormulaParser_CXX_OUTPUTS} PROPERTIES GENERATED TRUE)

# Инклюды для индексатора IDE и компиляции
target_include_directories(spreadsheet PRIVATE
        ${ANTLR4_INCLUDE_DIRS}
        ${ANTLR_FormulaParser_OUTPUT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/antlr4_runtime/runtime/src
)

# Линкуем с рантаймом
target_link_libraries(spreadsheet PRIVATE antlr4_static)

# Подавляем ворнинги рантайма в MSVC (как было у вас)
if(MSVC)
    target_compile_options(antlr4_static PRIVATE /W0)
endif()

# Красиво группируем сгенерированные файлы в дереве проекта IDE
source_group(TREE ${ANTLR_FormulaParser_OUTPUT_DIR}
        PREFIX "Generated/ANTLR"
        FILES ${ANTLR_FormulaParser_CXX_OUTPUTS})

# Инсталляция и стартовый проект для VS
install(TARGETS spreadsheet DESTINATION bin EXPORT spreadsheet)
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT spreadsheet)