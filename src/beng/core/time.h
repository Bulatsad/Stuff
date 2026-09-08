#pragma once

#include <beng/config.h>
#include <blib/blibint.h>

namespace beng
{
    /**
     * Time - утилита для работы со временем в игровом цикле.
     * 
     * Назначение:
     * - Предоставляет deltaTime (время между кадрами)
     * - Отслеживает общее игровое время
     * - Используется Scene для передачи в системы
     * 
     * Использование:
     *   Time time;
     *   
     *   while (running) {
     *       time.tick();  // обновить таймер
     *       float dt = time.getDeltaTime();
     *       scene.update(dt);
     *   }
     * 
     * Ограничения:
     * - Не thread-safe
     * - Использует blib::core::Time внутри (если есть) или std::chrono
     */
    class __beng_api Time
    {
    public:
        /**
         * Конструктор — инициализирует таймер.
         */
        Time();

        /**
         * Обновить таймер (вызывать в начале каждого кадра).
         * 
         * Вычисляет deltaTime с предыдущего вызова tick().
         */
        void tick();

        /**
         * Получить время с предыдущего кадра (в секундах).
         * 
         * @return deltaTime в секундах
         */
        float getDeltaTime() const { return deltaTime; }

        /**
         * Получить общее игровое время (в секундах).
         * 
         * @return Время с момента создания Time
         */
        float getTotalTime() const { return totalTime; }

        /**
         * Получить текущий FPS (кадров в секунду).
         * 
         * @return Примерный FPS (1.0 / deltaTime)
         */
        float getFPS() const;

    private:
        float deltaTime;   // Время с предыдущего кадра (секунды)
        float totalTime;   // Общее время (секунды)
        buint64 lastTick;  // Предыдущий тик (в миллисекундах или тиках процессора)
    };

} // namespace beng
