#pragma once

#include <string>

#include <blib/config.h>
#include <blib/core/console/consoleMessageType.h>
#include <blib/system/thread/multiProducerSingleConsumerCircleQueue.h>
#include <blib/utilmacro.h>

namespace blib
{
    namespace console
    {
        // Одна строка вывода консоли: текст + уровень (для подсветки).
        // Конструктор по умолчанию обязателен: элементы буфера очереди
        // предконструируются при reset().
        struct ConsoleLine
        {
            std::string text;
            ConsoleMessageType type;

            ConsoleLine();
            ConsoleLine(std::string text_, ConsoleMessageType type_);
        };

        // Потокобезопасный кольцевой буфер строк консоли (drop-oldest).
        //
        // Построен на MultiProducerSingleConsumerCircleQueue:
        //   - add() можно звать из ЛЮБОГО потока (продюсеры сериализуются
        //     writeLock'ом очереди);
        //   - читать (popLine/isEmpty/clear) должен ровно один консюмер —
        //     как правило, UI-слой (ConsoleWindow), который дренирует
        //     очередь каждый кадр.
        // При переполнении ёмкости самое старое сообщение теряется.
        class __blib_core_api ConsoleOutput
        {
        private:
            MultiProducerSingleConsumerCircleQueue<ConsoleLine> lines;

            // Дублировать строки в stdout/stderr (CLI-режим). Включается
            // приложениями без UI-потребителя буфера (консольные утилиты,
            // тестовый раннер), чтобы вывод оставался видимым в терминале.
            // GUI-приложения (ConsoleWindow) оставляют эхо выключенным,
            // чтобы не дублировать строки и не загрязнять stdout.
            bool echoToStdout;
        public:
            // @param capacity Максимальное число необработанных строк
            explicit ConsoleOutput(size_t capacity = 2048);

            // Управление stdout-эхом. Выставляется один раз при старте
            // (обычно из main()) до начала многопоточного логирования;
            // чтение флага в add() из потоков-продюсеров не синхронизировано.
            void setStdoutEcho(bool enabled);

            void add(ConsoleMessageType type, const std::string& text);
            void add(ConsoleMessageType type, std::string&& text);

            // Дренирование (консюмер): true — строка извлечена
            bool popLine(_Out ConsoleLine& out);
            bool isEmpty() const;

            // Выбросить все необработанные строки (консюмер)
            void clear();
        };
    }
}
