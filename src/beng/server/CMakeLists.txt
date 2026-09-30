# beng-server — headless-серверное ядро движка: ServerApplication
# (тикрейт-аккумулятор + сетевой цикл), NetworkServer (неблокирующие
# сокеты, слоты клиентов, очередь отправки), ReplicationManager
# (рефлексивная репликация: зеркала клиентов → полные/дельта-снапшоты),
# WorldManager (save/load мира), IServerGame (хук-интерфейс игры).
# Зависит от beng-core (ECS + репликация-кодек) и blib-network.
# Собирается из корневого CMake при наличии blib-network.
# Headless: без blib-graphics/sound/ввода.

file (GLOB server_header_src "*.h")
file (GLOB server_impl_src "*.cpp")

# Хидеры включены в таргет явно — чтобы быть видимыми в Solution Explorer
GroupSoureces("${HeaderGroupPrefix}/server" "${server_header_src}")
GroupSoureces("${SourceGroupPrefix}/server" "${server_impl_src}")

beng_add_module(beng-server
    ${server_header_src}
    ${server_impl_src}
)

beng_configure_module(beng-server beng_export)

set_target_properties(beng-server PROPERTIES FOLDER "beng" LINKER_LANGUAGE CXX)

# Корень инклюдов: все инклюды имеют вид <beng/server/...>, корень — src/beng
target_include_directories(beng-server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# beng-core — ECS + репликация (кодек/схема/фреймер); blib-network —
# TcpListener/TcpSocket (blib-system транзитивно)
target_link_libraries(beng-server PUBLIC beng-core blib-network)
