#pragma once

#include <beng/config.h>

#include <blib/core/math/vector.h>
#include <blib/utilmacro.h>

namespace beng
{
    namespace client
    {
        /**
         * ClientPrediction — чистая логика движкового client-side
         * prediction игрока (без сети/графики — покрывается группой
         * тестов client_prediction). Оболочка (ClientApplication)
         * владеет полным циклом, см. CLIENT.md:
         * - после poll (renderMirror записал интерполированную позицию)
         *   оболочка зовёт reconcile(...) с НОВЕЙШИМ серверным сэмплом
         *   позиции игрока (ReplicationClientState::getLatestFieldSample);
         * - затем игра интегрирует команду ввода
         *   (IClientGame::applyPlayerCommand), оболочка фиксирует
         *   результат (setPredictedPosition) и пишет его в зеркало —
         *   интерполированная позиция игрока перекрывается.
         *
         * Контракт:
         * - reconcile — до интеграции ввода; сэмпла нет — предсказание
         *   продолжается от текущего состояния (сервер молчит — не мешаем);
         * - снап — только при расхождении больше snapDistance: штатный
         *   лаг сервера (v·латентность) доверяем предсказанию —
         *   постоянная коррекция дала бы rubber-band на остановке;
         * - reset — при разрыве сессии (следующее подключение стартует
         *   от первого снапшота).
         */
        class ClientPrediction
        {
        private:
            // Предсказанная позиция игрока (оболочка пишет её в зеркало)
            blib::math::Vector<float, 3> predictedPosition;

            // Активно ли предсказание (первый серверный сэмпл пришёл)
            bool active;

        public:
            ClientPrediction()
                : predictedPosition(0.0f, 0.0f, 0.0f)
                , active(false)
            {
            }

            /**
             * Сброс состояния (разрыв сессии / остановка клиента).
             */
            void reset()
            {
                this->active = false;
                this->predictedPosition = blib::math::Vector<float, 3>(0.0f, 0.0f, 0.0f);
            }

            /**
             * Реконсиляция с новейшим серверным сэмплом позиции игрока
             * (вызывается ПЕРЕД интеграцией ввода):
             * - не активно и сэмпл есть — старт от серверной позиции
             *   (скачка при старте сессии нет);
             * - активно и расхождение > snapDistance — снап на сервер
             *   (реальная рассинхронизация);
             * - сэмпла нет — состояние не меняется.
             *
             * @param serverSampleValid Сэмпл есть (снапшот пришёл)
             * @param serverPosition Новейшая серверная позиция игрока
             * @param snapDistance Порог снапа (мир. ед.; 0 — любой
             *        рассинхрон снапает)
             * @return true — предсказанная позиция изменилась (старт/снап;
             *         оболочка логирует как диагностику)
             */
            bool reconcile(bool serverSampleValid,
                _In const blib::math::Vector<float, 3>& serverPosition, float snapDistance)
            {
                if (!serverSampleValid)
                {
                    return false; // сервер молчит — предсказание как есть
                }

                if (!this->active)
                {
                    this->predictedPosition = serverPosition;
                    this->active = true;
                    return true;
                }

                const blib::math::Vector<float, 3> error =
                    this->predictedPosition - serverPosition;
                if (blib::math::length(error) > snapDistance)
                {
                    this->predictedPosition = serverPosition;
                    return true;
                }
                return false;
            }

            /**
             * Активно ли предсказание (первый серверный сэмпл пришёл).
             */
            bool isActive() const { return this->active; }

            /**
             * Текущая предсказанная позиция (оболочка пишет её в зеркало
             * ПОСЛЕ renderMirror — интерполированное значение игрока
             * перекрывается).
             */
            const blib::math::Vector<float, 3>& getPredictedPosition() const
            {
                return this->predictedPosition;
            }

            /**
             * Зафиксировать позицию после интеграции ввода игрой.
             */
            void setPredictedPosition(_In const blib::math::Vector<float, 3>& position)
            {
                this->predictedPosition = position;
            }
        };

    } // namespace client
} // namespace beng
