#pragma once

namespace blib
{
    namespace console
    {
        // Уровень сообщения консоли. Используется UI-слоем для подсветки
        // (info/warning/error/command разными цветами) и, в перспективе,
        // для фильтрации вывода.
        enum class ConsoleMessageType
        {
            Info,    // обычное информационное сообщение
            Warning, // предупреждение — что-то пошло не так, но работаем
            Error,   // ошибка
            Command  // эхо введённой пользователем команды
        };

        // Человекочитаемое имя уровня (для логов в файл/прочих потребителей)
        const char* messageTypeToString(ConsoleMessageType type);
    }
}
