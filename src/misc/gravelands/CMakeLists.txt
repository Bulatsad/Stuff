# misc/gravelands — игровой слой: игра Gravelands (диаблоид)
#
# Паттерн «lib + тонкий exe» (см. ARCHITECTURE.md): core-библиотеки
# с frame-API + тонкие exe, владеющие главным циклом.
# Эдитора игры здесь НЕТ: эдитор — единый beng-editor.exe на все игры;
# Gravelands подключится к нему как плагин (см. ARCHITECTURE.md, «Эдитор»).

add_subdirectory(common)
add_subdirectory(world)
add_subdirectory(plugin)
add_subdirectory(server)
add_subdirectory(client)
