# gravelands-common — общие определения игры Gravelands:
# константы конфигурации, позже — общие компоненты ECS, пакеты
# протокола и формулы. Линкуется и клиентом, и сервером.
#
# Пока модуль header-only — собирается как INTERFACE-библиотека
# (не создаёт пустой .lib). Когда появятся первые .cpp — переключить
# на STATIC, не меняя имени таргета.

add_library(gravelands-common INTERFACE)

set_target_properties(gravelands-common PROPERTIES FOLDER "gravelands/common" LINKER_LANGUAGE CXX)

# Инклюд-корень game-слоя: src/misc — все инклюды игры имеют вид <gravelands/...>
target_include_directories(gravelands-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# blib-core даёт корень <blib/...> и конфигурационные дефайны (PUBLIC).
# beng-core добавится сюда, когда появятся компоненты (наследуют beng::IComponent).
target_link_libraries(gravelands-common INTERFACE blib-core)
