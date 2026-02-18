Многопользовательский игровой сервер, написанный с нуля на C++17. Реализует управление игровыми сессиями, обработку игровой логики в реальном времени и HTTP API для взаимодействия с клиентом. Использует Boost (Asio/Beast/JSON/Log) для асинхронного I/O и сетевого слоя, PostgreSQL (libpqxx) для хранения статистики и таблицы лидеров, а также покрыт unit-тестами (Catch2) для ключевых компонентов. Контейнеризуется через Docker, зависимости управляются через Conan.

## Сборка и запуск (Windows, MSVS)

Требуется Conan 1.x (проверено на 1.66.0).

В директории с кодом выполните следующие команды:

```bat

1. mkdir build
2. cd build
3. conan install .. --build=missing -s build\_type=Release -s compiler.runtime=MT
4. cmake ..
5. cmake --build . --config Release

```



## Возможные сложности со сборкой

в случае проблем с libpqxx при сборке в директории /.conan/data/libpqxx/7.7.4/*/*/source/src/cmake замените config.cmake на [config.cmake](./fix/config.cmake)

