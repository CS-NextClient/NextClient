# Подготовка тестового контента для рендеринга Studio

[Английская версия](studio-content.md) | [Проверочные сценарии](../testing/studio-rendering.ru.md) | [Диагностика](../renderer-diagnostics.ru.md)

Инструменты создают карту `studio_test` и варианты моделей для ручной проверки рендеринга разработчиком. Подготовка выполняется из корня репозитория. Полученные файлы устанавливаются в указанный каталог игры. Результаты проверки в игре сохраняются отдельно от результатов проверки исходников и сборки.

## Требования

Нужны Python с Pillow, корректная модель GIGN, `halflife.wad` и пользовательские модели карты. По умолчанию используются `YoRHa_2B`, `svinorez_t_robb` и `next21_adventures/treasure.mdl`, доступные по путям поиска игры. Параметры `--perf-a` и `--perf-b` задают имена моделей в каталоге `--perf-models`. Для каждого имени нужен файл `<name>/<name>.mdl`. Параметр `--remap-model` задает игровой путь модели с перекраской, а `--remap-model-file` задает путь ее файла на диске для расчета высоты размещения. Значение `--perf-count` по умолчанию равно 48.

Разместите `hlcsg.exe`, `hlbsp.exe`, `hlvis.exe` и `hlrad.exe` в `<game>/tools/`. Скрипт сборки читает созданные `.map` из `<game>/cstrike/maps/src/` и копирует готовый BSP в `<game>/cstrike/maps/`.

## Генерация и компиляция

Замените `<game>` корнем установки и выполните:

```text
python tools/studio_tests/mdl_flags.py "<game>/cstrike/models/player/gign/gign.mdl" "<game>/cstrike/models/studio_tests"
python tools/studio_tests/make_labels.py "<game>/cstrike/studio_test.wad" plain flatshade chrome chromeflat fullbright masked additive glow_rm4 add_rm5 color_rm1 shell_red shell_blue remap
python tools/studio_tests/make_maps.py "<game>/cstrike/maps/src" --flag-models "<game>/cstrike/models/studio_tests" --perf-models "<game>/cstrike_downloads/models/player" --wad "<game>/valve/halflife.wad" --label-wad "<game>/cstrike/studio_test.wad" --remap-model-file "<game>/cstrike_downloads/models/next21_adventures/treasure.mdl"
powershell -File tools/studio_tests/build_maps.ps1 -GameDir "<game>/cstrike"
```

`mdl_flags.py` копирует исходную модель и изменяет флаги ее текстур, создавая варианты `plain`, `flatshade`, `chrome`, `chrome_flat`, `fullbright`, `masked` и `additive`. Если текстуры хранятся отдельно, изменяется связанный файл текстур. Скрипт также создает копию `shell` без флагов для проверки оболочек и освещения. Привязывайте образцы материалов к варианту `plain`. Для сравнения модели без авторских материалов используйте `shell`.

Файл WAD должен содержать текстуру для каждой подписи на карте. Скрипт сборки встраивает текстуры, выполняет полный VIS, запускает RAD с `-fast` и устанавливает `studio_test.bsp`. Полный VIS и коридор ограничивают видимость между двумя комнатами.

## Состав карты

| Область | Содержимое |
|---|---|
| Южная комната и точка появления | Варианты флагов текстур, режимы отрисовки 4/5/1, красная и синяя светящиеся оболочки (glow shell), подбираемый АК и модель с перекраской текстуры. |
| Темная ниша | Две модели без материалов для проверки света сцены и источников света сущностей. |
| Северная комната | Сетка из 12 столбцов с чередованием двух нагрузочных моделей; по умолчанию 48 сущностей. |
| S-образный коридор | Ограничивает видимость между комнатами. Используйте фиксированную точку обзора в северной комнате и проверьте состав видимых моделей по диагностическому дампу. |

Если указан `--remap-model-file`, высота размещения модели с перекраской вычисляется по ограничивающему объему из описания анимации модели. В созданной карте ее `colormap` равен нулю. Для проверки других цветов задайте `colormap` сущности через серверный плагин выражением `top | (bottom << 8)`.

После подготовки используйте [сценарии Studio](../testing/studio-rendering.ru.md) и [процедуры захвата](../renderer-diagnostics.ru.md) в установленной игре.
