# misc/gravelands — игровой слой: игра Gravelands (диаблоид)
#
# Паттерн «lib + тонкий exe» (см. ARCHITECTURE.md): core-библиотеки
# с frame-API + тонкие exe, владеющие главным циклом.
# Позже сюда добавится add_subdirectory(editor) — эдитор игры.

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
