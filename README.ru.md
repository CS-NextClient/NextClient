Язык: [EN](https://github.com/CS-NextClient/NextClient/blob/main/README.md) | RU

NextClient
==========

NextClient расширяет возможности Counter-Strike 1.6 для игроков и разработчиков серверных модификаций на AMX Mod X.
В NextClient также включены некоторые возможности из [csldr](https://github.com/mikkokko/csldr).

*Официальная версия NextClient поставляется без эмулятора Steam. Команда Next21 не разрабатывает такой эмулятор. Для запуска версии из GitHub нужны купленная Counter-Strike 1.6 и запущенный Steam.*

*Некоторые антивирусы ошибочно отмечают файлы NextClient как опасные. Это ложные эвристические срабатывания, вызванные перехватом функций движка во время работы. Официальные сборки создаются из исходного кода этого репозитория через GitHub Actions. Их происхождение подтверждается подписанными данными о сборке (build provenance attestation). Подробности и порядок отправки сообщений о ложных срабатываниях: [docs/reporting-av-false-positives.md](docs/reporting-av-false-positives.md).*

### Основные возможности:

 - Поддержка Windows и Linux (x86)
 - Протектор - защищает клиент от вредоносных команд с сервера
 - Смена мастер-сервера (по умолчанию установлен [tsarvar.com](https://tsarvar.com))
 - Расширенные настройки видео - корректный угол обзора (FOV) при соотношении сторон 16:9, настройка FOV для камеры и отдельно для модели оружия от первого лица
 - Расширенная лента убийств (killfeed) - поддержка сообщений [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), отображение иконок убийств: через стену, через дым, без прицела, в прыжке, с доминированием и других
 - Расширенные настройки прицела - добавлены новые виды прицела: точка, T-образный, окружность
 - Расширенный радар - генерация карты с автоматическим выбором слоя, ники игроков, здоровье союзников при передаче данных сервером, трейлы перемещения, маркеры целей и показ полной карты
 - Интеграция с Discord: отображение текущего сервера, подключения и воспроизведения демо, подключение друзей к серверу через Discord, в том числе с запуском игры
 - Две схемы интерфейса с переключением в настройках и возможностью добавлять собственные схемы
 - Поддержка HTML-интерфейсов в GameUI на базе CEF (Chromium Embedded Framework) с JS API для взаимодействия с клиентом (только Windows)
 - Отображение здоровья выше 255 HP при использовании [серверного модуля](https://github.com/CS-NextClient/NextClientServerApi)
 - Отображение количества и размера оставшихся файлов, общего размера файлов и скорости загрузки при подключении к серверу
 - Цветной чат в консоли
 - Различные улучшения из [csldr](https://github.com/mikkokko/csldr) для оружия от первого лица:
    - Настраиваемое расположение модели
    - Покачивание оружия при движении (bob) из CS:GO 1.0.0.40
    - Покачивание оружия при поворотах камеры (sway/lag)
    - Возможность отключить смещение модели при взгляде вверх или вниз
    - Клиентский осмотр оружия
    - Управление движением камеры через кости модели

### Возможности для разработчиков AMX Mod X:
 - Песочница кваров: сервер может временно менять клиентские квары из разрешенного списка, пока игрок находится на сервере
 - Настройка ленты убийств
 - Sprite API, управление спрайтами на экране
 - Расширенное сообщение для управления FOV
 - Эффекты для модели оружия от первого лица (viewmodel)
 - Замена звуков оружия
 - Инверсия мыши
 - Проверка клиента и получение аппаратного идентификатора игрока (HWID)
 - Получение платформы клиента (Windows или Linux) через серверный API NextClient
 - Раздельный прекэш ресурсов для стандартного клиента CS 1.6 и NextClient
 - Прекэш hud.txt и других стандартных ресурсов

### Новые квары (консольные переменные)
<details>
<summary>Нажмите, чтобы развернуть</summary>

| Квар | По умолчанию | Доступен в песочнице* | Описание |
| --- |---------------|-----------------------------| --- |
| viewmodel_disable_shift | 0             | Да                         | Отключить смещение модели оружия при взгляде вверх или вниз. |
| viewmodel_offset_x | 0             | Да                         |  |
| viewmodel_offset_y | 0             | Да                         |  |
| viewmodel_offset_z | 0             | Да                         |  |
| camera_movement_scale | 1.0           | Да                         | Масштаб движения камеры. |
| camera_movement_interp | 0             | Да                         | Сглаживание движения камеры при смене оружия. Рекомендуемое значение: 0.1. Значение 0 отключает сглаживание. |
| viewmodel_fov | 90            | Да                         | Мин.: 70<br/>Макс.: 100 |
| cl_crosshair_type | 0             | Да                         | Вид прицела: 0 - перекрестие, 1 - T-образный, 2 - окружность, 3 - точка. |
| cl_bob_camera | 1             | Да                         | Покачивание камеры при движении. Не действует при cl_bobstyle 2. |
| cl_bobstyle | 0             | Да                         | Вид покачивания оружия: 0 - стандартный, 1 - старый, 2 - как в CS:GO. |
| cl_bobamt_vert | 0\.13         | Да                         | Амплитуда вертикального покачивания в стиле CS:GO. |
| cl_bobamt_lat | 0\.32         | Да                         | Амплитуда бокового покачивания в стиле CS:GO. |
| cl_bob_lower_amt | 8.0           | Да                         | Смещение модели оружия внутрь при покачивании в стиле CS:GO. |
| cl_rollangle | 0             | Да                         | Угол наклона экрана при боковом движении или повороте камеры (эффект Quake). |
| cl_rollspeed | 200.0         | Да                         | Скорость наклона экрана при боковом движении или повороте камеры (эффект Quake). |
| viewmodel_lag_style | 0             | Да                         | Вид покачивания оружия при поворотах камеры: 0 - отключено, 1 - как в HL2, 2 - как в CS:S/CS:GO. |
| viewmodel_lag_scale | 1.0           | Да                         | Амплитуда покачивания оружия при поворотах камеры. |
| viewmodel_lag_speed | 8.0           | Да                         | Скорость покачивания оружия при поворотах камеры. Действует только для стиля HL2. |
| fov_horplus | 0             | Нет                          | Включить масштабирование FOV по принципу Hor+. Корректирует угол обзора при соотношениях сторон, отличных от 4:3. |
| fov_angle | 90            | Нет (используйте ncl_setfov) | Мин.: 70<br/>Макс.: 100 |
| fov_lerp | 0             | Нет (используйте ncl_setfov) | Время интерполяции FOV в секундах. |
| hud_deathnotice_max | 5             | Да                         | Максимальное число записей в ленте убийств. |
| hud_deathnotice_old | 0             | Нет                          | Включить старый вид ленты убийств. |
| http_max_active_requests | 5             | Нет                          |  |
| http_max_requests_retries | 3             | Нет                          |   |
| cl_discordrpc | 1             | Нет                          | Показывать текущую игру в Discord. |
| cl_discordrpc_server | 1             | Нет                          | Показывать название сервера в Discord. При значении 0 друзья также не смогут подключиться через Discord. |
| cl_discordrpc_join | 1             | Нет                          | Разрешить друзьям из Discord подключаться к вашему серверу. |
| hud_radar_mode | 1 | Нет | Режим радара: 0 - классический, 1 - расширенный. |
| hud_radar_scale | 1.15 | Нет | Размер радара. Мин.: 0.5<br/>Макс.: 3.0 |
| hud_radar_map_scale | 0.8 | Нет | Масштаб карты. Мин.: 0.25<br/>Макс.: 3.0 |
| hud_radar_map_shift | 0 | Нет | Смещать карту у её краёв, чтобы маркер игрока оставался внутри радара. |
| hud_radar_opacity | 100 | Нет | Непрозрачность карты и фона от 0 до 100 процентов. |
| hud_radar_round | 1 | Нет | Форма радара: 0 - квадратная, 1 - круглая. |
| hud_radar_fullmap | 1 | Нет | Показывать полную карту при открытой таблице счёта. |
| hud_radar_shift_scoreboard | 1 | Нет | Сдвигать таблицу счёта, чтобы освободить место для радара. |
| hud_radar_rotate | 1 | Нет | Вращать карту в зависимости от направления взгляда. |
| hud_radar_names | 0 | Нет | Показывать ники игроков. |
| hud_radar_health | 0 | Нет | Показывать полосы здоровья союзников при передаче сервером данных HealthInfo. |
| hud_radar_tracks | 0 | Нет | Показывать трейлы перемещения союзников. |
| hud_radar_map_mode | 5 | Нет | Источник карты: 1 - overview, 5 - сгенерированные текстурированные слои, 6 - сгенерированные плоские слои. |
| hud_radar_map_mask | 1 | Нет | Обрезать карту overview по форме радара. |
| hud_radar_map_smoothing | 0 | Нет | Сглаживать края геометрии сгенерированной карты. |
| hud_radar_bg | 0 | Нет | Фон радара: 0 - чёрный, 1 - размытое изображение игры. |
| hud_radar_health_size | 1.0 | Нет | Размер полос здоровья. Мин.: 0.3<br/>Макс.: 4.0 |
| hud_radar_name_size | 0.8 | Нет | Размер ников игроков. Мин.: 0.6<br/>Макс.: 2.0 |
| hud_radar_allies | 1 | Нет | 1 - показывать союзников, раскрытых сервером, 0 - дополнительно проверять прямую видимость. |

*Указано, может ли сервер менять значение квара через песочницу кваров.
</details>

### Консольные команды

| Команда | Назначение |
| --- | --- |
| `toggle <command> <value1> <value2> [value3...]` | Последовательно применяет указанные значения к команде; после последнего возвращается к первому. |
| `+toggle <command> [<held> <released>]` | Применяет одно значение при удержании клавиши и другое при отпускании. Без значений используются 1 и 0 соответственно. |
| `bindlist` | Выводит все назначения клавиш в консоль. |
| `lookat` | Запускает осмотр оружия; по умолчанию назначена на `V`. |
| `next_version` | Выводит версию сборки NextClient и версии модулей. |

Примеры:

```text
bind "F6" "toggle hud_radar_mode 0 1"
bind "F7" "+toggle hud_radar_names"
bindlist
```

## Установка

### Windows

1. NextClient работает только с версией движка 8684. В Steam выберите бета-ветвь "steam_legacy - Pre-25th Anniversary Build". Для установки нужны файлы официальной игры из Steam.
2. Скопируйте всю папку Counter-Strike 1.6 (Half-Life) в отдельную папку вне Steam.
3. Скопируйте все файлы NextClient в папку с этой копией игры.
4. Запустите Steam.
5. Запускайте игру через `cstrike.exe`.

### Linux

Требования для готовых архивов GitHub Actions:

- Linux на x86_64 с поддержкой запуска 32-битных x86-программ и 32-битной glibc 2.43 или новее.
- 32-битные библиотеки среды выполнения C++: в Ubuntu это пакеты `libstdc++6:i386` (`libstdc++.so.6` с `GLIBCXX_3.4.32` или новее) и `libgcc-s1:i386` (`libgcc_s.so.1`).
- Установленный и запущенный Steam, его Steam Runtime и нативные Linux-файлы игры с ветви `steam_legacy`.
- 32-битные SDL2 и OpenGL-библиотеки драйвера. Для расширенного радара нужны OpenGL 2.1 и поддержка фреймбуферов (FBO) (`GL_ARB_framebuffer_object` или OpenGL 3.0 и новее).

Установка 32-битных библиотек C++ в Ubuntu:

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libstdc++6:i386 libgcc-s1:i386
```

Порядок установки:

1. В Steam выберите бета-ветвь "steam_legacy - Pre-25th Anniversary Build" и установите нативную Linux-версию игры.
2. Скопируйте всю папку Counter-Strike 1.6 (Half-Life) в отдельную папку вне Steam..
3. Распакуйте архив в папку с копией игры и замените существующие файлы. В корне этой папки должны находиться `hw.so`, `cstrike.exe` и `nextclient.sh`.
4. Запустите Steam.
5. В терминале перейдите в папку скопированной игры, задайте права на запуск и запустите скрипт:

```bash
cd "/path/to/Half-Life-NextClient"
chmod +x nextclient.sh cstrike.exe
./nextclient.sh
```

`nextclient.sh` запускает нативный Linux-клиент через Steam Runtime. Скрипт находит среду выполнения Steam как при обычной установке, так и при установке через Snap. Аргументы передаются игре, например `./nextclient.sh -windowed`.

### Внимание: установка NextClient в папку игры в Steam приводит к блокировке VAC. Используйте отдельную копию игры.

## Смена мастер-сервера

Файл конфигурации находится по пути `platform\config\MasterServer.vdf`
```vdf
"MasterServer"
{
   "Selected"     "1"       // Номер элемента из раздела Servers. Нумерация начинается с 0.
   "CacheServers" "false"   // В случае недоступности мастер-сервера, список серверов будет взят из кэша.
                            // В кэш попадают сервера из последнего запроса, который был полностью завершён.
   "Servers"
   {
      "Steam"
      {
         "address" "hl1master.steampowered.com:27011" // Адрес мастер-сервера.
         "site"    ""       // Не используется.
         "region"  "0x03"   // Код региона, https://developer.valvesoftware.com/wiki/Master_Server_Query_Protocol#Region_codes.
      }
      "Tsarvar"
      {
         "address" "ms.tsarvar.com:27010"
         "site"    "https://tsarvar.com/"
         "region"  "0xFF"
      }
   }
}
```

## Сборка

### Windows

Требования:
 - Последняя версия MSVC 2022 или новее
 - CMake 3.21 или выше
 - Git LFS, настроенный командой `git lfs install` до клонирования (в клоне, сделанном без него, выполните `git lfs pull`)
 - Ninja (необязательно)

Запустите x86 Native Tools Command Prompt for VS

```
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
```

С генератором Visual Studio 2022:
```
cmake --preset vs2022
cmake --build --preset vs2022-release --target BUILD_ALL
```

С генератором Visual Studio 2026:
```
cmake --preset vs2026
cmake --build --preset vs2026-release --target BUILD_ALL
```

Или с Ninja:
```
cmake --preset ninja
cmake --build --preset ninja-release --target BUILD_ALL
```

`BUILD_ALL` также копирует собранные файлы и ресурсы в `NEXTCLIENT_INSTALL_DIR`, если переменная задана.
Можно передать её при конфигурации:
```
cmake --preset vs2022 -DNEXTCLIENT_INSTALL_DIR="C:/Games/CS 1.6 - NextClient"
```
Или создать `CMakeUserPresets.json` в корне проекта (файл исключен из Git через `.gitignore`), чтобы не передавать путь каждый раз:
```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "vs2022-local",
      "inherits": "vs2022",
      "cacheVariables": {
        "NEXTCLIENT_INSTALL_DIR": "C:/Games/CS 1.6 - NextClient"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "vs2022-local-release",
      "configurePreset": "vs2022-local",
      "configuration": "Release"
    },
    {
      "name": "vs2022-local-debug",
      "configurePreset": "vs2022-local",
      "configuration": "Debug"
    }
  ]
}
```
Затем:
```
cmake --preset vs2022-local
cmake --build --preset vs2022-local-release --target BUILD_ALL
```

### Linux

Требования для сборки из исходников:

- GCC 15 или новее с поддержкой 32-битной сборки (`gcc-multilib` и `g++-multilib` в Ubuntu).
- CMake 3.21 или новее, Ninja, Git LFS, `pkg-config`, Python 3, `curl`, `zip`, `unzip` и `tar`.
- 32-битные заголовки и библиотеки разработки OpenGL и X11. Список пакетов используемого окружения указан в [.github/workflows/build.yml](.github/workflows/build.yml).

Клонирование и сборка:

```bash
git lfs install
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset linux -DNEXTCLIENT_INSTALL_DIR="/path/to/Half-Life-NextClient"
cmake --build --preset linux-release --target BUILD_ALL
```

Пресет `linux` собирает 32-битные модули через Ninja Multi-Config. Цель `BUILD_ALL` копирует библиотеки, ресурсы и `nextclient.sh` в `NEXTCLIENT_INSTALL_DIR`; эта папка должна содержать отдельную копию Linux-файлов игры.

## Работа в IDE

### CLion
 - На Windows выберите набор инструментов (toolchain) Visual Studio с платформой x86; на Linux используйте GCC и пресет `linux`
 - В настройках CMake (Settings -> Build -> CMake) включить нужные пресеты, например `vs2022 - vs2022-debug` и `vs2022 - vs2022-release`
 - Собирать цель `BUILD_ALL`

### Visual Studio
 - Открыть папку проекта
 - Выбрать пресет сборки, например `vs2022-debug` или `vs2022-release`
 - В главном меню выбрать Build -> Build All

### VS Code
 - Установить расширение `ms-vscode.cmake-tools`
 - Выбрать пресет конфигурации `vs2022`, `vs2026`, `ninja` или `linux` и соответствующую конфигурацию сборки
 - Собирать цель `BUILD_ALL`
 - Для пресета `ninja` VS Code необходимо запускать из "x86 Native Tools Command Prompt for VS"

## Благодарности

- [KotyaraKryt](https://github.com/KotyaraKryt) - за перенос на Linux, интеграцию с Discord, консольные команды, повторное использование HTTP-соединений в FastDL и исправления
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - за отзывы и сообщения об ошибках
- [fl0werD](https://github.com/fl0werD) - за разработку Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - за проект [csldr](https://github.com/mikkokko/csldr), возможности которого включены в NextClient
- [Felipe](https://github.com/LAGonauta) - за проект [MetaAudio](https://github.com/LAGonauta/MetaAudio)
- [MoeMod](https://github.com/MoeMod) - за проект [Thanatos-Launcher](https://github.com/MoeMod/Thanatos-Launcher), он очень помог при реализации GameUI и VGUI2
- [tmp64](https://github.com/tmp64) - за проект [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk)
- [TsarVar](https://tsarvar.com) - за идею JS API для gameui
- [s1lent](https://github.com/s1lentq) - за советы, исправления и разработку спецификации [avatarid-spec](https://github.com/goldclient-plus/avatarid-spec)
- [lozatto](https://github.com/lozatto) - за реализацию аппаратного идентификатора Unique Machine Identifier (HWID)
- [SanyaSho](https://github.com/SanyaSho) - за проект [BarsTech_goldsrc_compatible_public](https://github.com/SanyaSho/BarsTech_goldsrc_compatible_public), который помог с рендерингом шрифтов VGUI2
- Valve - за Counter-Strike 1.6 и лояльное отношение к моддерскому сообществу

Спасибо всем, кто сообщает об ошибках, предлагает улучшения и поддерживает проект.
