# Порядок проверки слоев радара

[Английская версия](radar-layers.md) | [Справочник](../radar-layers.ru.md) | [Приемочные сценарии](radar-layer-cases.ru.md)

## Автоматические проверки

Используйте среду разработки x86 и зависимости проекта. После изменения списка исходников повторите конфигурацию. В каждой команде сборки укажите одну цель. Пример для Windows с пресетом тестов Visual Studio 2022 из репозитория:

```powershell
cmake --preset vs2022-tests
cmake --build --preset vs2022-tests-release --target client_mini
cmake --build --preset vs2022-tests-release --target client_mini-tests
cmake --build --preset vs2022-tests-release --target view-tests
cmake --build --preset vs2022-tests-release --target crosshair-tests
cmake --build --preset vs2022-tests-release --target gameui
& './build/vs2022-tests/out/bin/Release/client_mini-tests.exe'
& './build/vs2022-tests/out/bin/Release/view-tests.exe'
& './build/vs2022-tests/out/bin/Release/crosshair-tests.exe'
```

Соберите `gameui`, если изменены общие заголовки клиента. Локальные тестовые пресеты Ninja требуют `NEXTCLIENT_BUILD_TESTS=ON`, окружения MSVC x86 и явно указанной конфигурации сборки. Запишите компилятор, пресет, конфигурацию, ревизию исходников, команды и результаты.

Цель `radar-gl-tests` проверяет внешний вид маркеров, прозрачность, отсечение, видимость слоев и масштабирование. Для этих проверок нужен OpenGL 3.0 или новее. В Linux также требуется 32-битная `libEGL.so.1` с поддержкой настольного OpenGL; CTest выбирает программный рендеринг через `LIBGL_ALWAYS_SOFTWARE=1`. Ошибка инициализации графики означает неудачную проверку и не подтверждает работу рендеринга. Тесты выполняются без игры.

```powershell
cmake --build --preset vs2022-tests-release --target radar-gl-tests
ctest --test-dir build/vs2022-tests -C Release -L gl --output-on-failure
```

В Linux соберите ту же цель в настроенном тестовом каталоге x86 и выполните `ctest --test-dir <build> -C Release -L gl --output-on-failure`. Включайте GL-проверки при изменении изображения радара. Запустите `RadarLayerScheduledJobTest.DestructionJoinsWithoutPumpingFrames` через CTest, чтобы проверить завершение работы с ограничением времени в 10 секунд.

## Проверки на реальных картах

Укажите локальный BSP-файл и запустите приемочный сценарий карты:

```powershell
$env:NEXTCLIENT_RADAR_BSP = '<absolute path to scp_foundation_bw40.bsp>'
& './build/vs2022-tests/out/bin/Release/client_mini-tests.exe' '--gtest_filter=RadarBspFile.LocalMapAnalysis'
```

Если `NEXTCLIENT_RADAR_BSP` не задана или пуста, тест пропускается. В этом случае карта не считается проверенной. Храните карты вне Git. Повторите проверку для de_dust2, adv_clocktower, adv_university и scp_foundation_bw40. de_dust2 должна иметь один автоматический слой; остальные карты должны сохранять крупные многоярусные участки.

Укажите в `NEXTCLIENT_RADAR_MESH_EXPORT` путь выходного JSON-файла, чтобы сравнить экспортированное покрытие карты. Родительский каталог создается автоматически. Экспорт содержит геометрию и текстурные координаты; текстуры и освещение оценивайте в игре.

Для проверки поправок укажите в `NEXTCLIENT_RADAR_CORRECTIONS` файл в [описанном формате](../radar-layers.ru.md), без комментариев. Для `cs_assault` используйте `assets/cstrike/overviews/cs_assault_layers.txt` и `NEXTCLIENT_RADAR_EXPECTED_LAYERS=1`. Проверьте покрытие внутри и снаружи склада. Перед проверкой других карт удалите переменные поправок из окружения.

Пример для Linux x86 с настроенными нативными зависимостями:

```bash
cmake --preset linux -D NEXTCLIENT_BUILD_TESTS=ON
cmake --build --preset linux-debug --target client_mini
cmake --build --preset linux-debug --target client_mini-tests
cmake --build --preset linux-debug --target view-tests
cmake --build --preset linux-debug --target crosshair-tests
cmake --build --preset linux-debug --target gameui
export NEXTCLIENT_RADAR_BSP='<absolute Linux path to scp_foundation_bw40.bsp>'
./build/linux/out/bin/Debug/client_mini-tests
./build/linux/out/bin/Debug/view-tests
./build/linux/out/bin/Debug/crosshair-tests
```

Явно указывайте отсутствующие зависимости и пропущенные проверки. Сравнивайте производительность в одной конфигурации сборки и сохраняйте диагностический вывод с каждым результатом.

## Наблюдения в игре

Разработчик запускает установленную игру и выполняет [приемочные сценарии](radar-layer-cases.ru.md) в режимах радара 5 и 6, включая расширенный радар. Запишите ревизию и конфигурацию загруженного клиента. Установку выполните по [соответствующей процедуре](installation-and-factory.ru.md).

Для каждого сценария запишите карту и маршрут, режимы радара и наблюдения, ожидаемый этаж, снимок или видео, а также `radar_layers_dump` до, во время и после действия. Используйте статусы `pass`, `fail` или `not_run`. Отчет об ошибке должен содержать точное действие и соответствующие поля дампа. Их значения описаны в [справочнике радара](../radar-layers.ru.md).

Во время анализа карты проверьте смену карты и видеонастроек, переключение между режимами 5 и 6, выход из послойных режимов и завершение клиента. Клиент должен отвечать на действия и показывать только результаты текущей карты.

При мерцании маркеров игроков включите `hud_radar_debug 1`, воспроизведите проблему и сразу выполните `radar_markers_dump`. Сохраните вывод и запись, затем остановите сбор данных командой `hud_radar_debug 0`.

Результаты автоматических проверок и наблюдения в установленной игре учитываются отдельно. Храните заполненные отчеты, измерения и журналы в `.superpowers/reports/`.
