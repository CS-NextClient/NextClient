Language: [EN](https://github.com/CS-NextClient/NextClient/blob/main/README.md) | RU

NextClient
==========

NextClient это модификация для Counter-Strike 1.6, нацеленная на введение новой функциональности как для игроков, так и для разработчиков серверных модификаций на amxmodx.
В NextClient интегрированы некоторые фичи из [csldr](https://github.com/mikkokko/csldr). 

### Основные возможности:
 - Протектор - защищает клиент от вредоносных команд с сервера
 - Смена мастер сервера (по умолчанию установлен [tsarvar.com](https://tsarvar.com))
 - Расширенные настройки видео - фикс FOV на разрешениях 16:9, возможность регулировать FOV и отдельно регулировать FOV для модели от первого лица
 - Расширенный killfeed - поддержка расширенного killfeed [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), отображение иконок убийств: через стену, через дым, без прицела, в прыжке, с доминированием, etc.
 - Расширенные настройки прицела - добавлены новые виды прицела: точка, T-образный, окружность
 - 2 схемы GUI с возможностью их смены через настройки, и возможность добавлять свои схемы не удаляя старые
 - Отображение более 255hp при использовании [серверного модуля](https://github.com/CS-NextClient/NextClientServerApi)
 - Отображение количества и размера оставшихся файлов, общего размера файлов и скорости загрузки при подключении на сервер
 - Цветной чат в консоли
 - Различные улучшения из [csldr](https://github.com/mikkokko/csldr) для оружия от первого лица:
    - Настраиваемое расположение модели
    - Новые типы bob
    - Поддержка sway/lag
    - Возможность отключить смещение модели при взгляде вверх/вниз
    - Поддержка анимации осмотра
    - Поддержка управления камерой для анимации осмотра

### Возможности для amxmodx разработчиков:
 - Песочница кваров, возможность менять квары клиенту (из ограниченного списка) на время его нахождения на сервере
 - Кастомизация killfeed
 - Sprite API, управление спрайтами на экране
 - Расширенное FOV сообщение
 - Поддержка эффектов для viewmodel
 - Раздельный прекэш для обычного клиента cs 1.6 и NextClient
 - Прекэш hud.txt и других стандартных ресурсов

### Новые квары
<details>
<summary>Нажмите, чтобы развернуть</summary>

| Cvar name | Default value | Available in sandbox*       | Description |
| --- |---------------|-----------------------------| --- |
| viewmodel_disable_shift | 0             | Yes                         | Disable viewmodel shifting (when you looking up or down). |
| viewmodel_offset_x | 0             | Yes                         |  |
| viewmodel_offset_y | 0             | Yes                         |  |
| viewmodel_offset_z | 0             | Yes                         |  |
| camera_movement_scale | 1             | No                          | Camera movement scale. |
| camera_movement_interp | 0             | No                          | Smooths out camera movement when switching weapons. Recommended value is 0.1. Set to 0 to disable smoothing. |
| viewmodel_fov | 90            | No                          | Min: 70<br/>Max: 100 |
| cl_crosshair_type | 0             | Yes                         | Crosshair type. 0 - crosshair, 1 - T-shaped, 2 - circle, 3 - dot. |
| cl_bob_camera | 0             | No                          | View origin bob, does nothing with cl_bobstyle 2. |
| cl_bobstyle | 0             | Yes                         | 0 for default bob, 1 for old style bob and 2 for CS:GO style bob. |
| cl_bobamt_vert | 0\.13         | Yes                         | Vertical scale for CS:GO style bob. |
| cl_bobamt_lat | 0\.32         | Yes                         | Lateral scale for CS:GO style bob. |
| cl_bob_lower_amt | 8             | Yes                         | Specifies how much the viewmodel moves inwards for CS:GO style bob. |
| cl_rollangle | 0             | Yes                         | Screen roll angle when strafing or looking (Quake effect). |
| cl_rollspeed | 200           | Yes                         | Screen roll speed when strafing or looking (Quake effect). |
| viewmodel_lag_style | 0             | No                          | Viewmodel sway style. 0 is off, 1 is HL2 style and 2 is CS:S/CS:GO style. |
| viewmodel_lag_scale | 0             | Yes                         | Scale of the viewmodel sway. |
| viewmodel_lag_speed | 8             | Yes                         |  Speed of the viewmodel sway. (HL2 sway only) |
| fov_horplus | 0             | No                          | Enables Hor+ scaling for FOV. Fixes the FOV when playing with aspect ratios besides 4:3. |
| fov_angle | 90            | No (use ncl_setfov instead) | Min: 70<br/>Max: 100 |
| fov_lerp | 0             | No (use ncl_setfov instead) | FOV interpolation time in seconds. |
| hud_deathnotice_max | 5             | No                          | The maximum number of killfeed entries that can be displayed. |
| hud_deathnotice_old | 0             | No                          | Enable the old style of killfeed. |
| http_max_active_requests | 5             | No                          |  |
| http_max_requests_retries | 3             | No                          |   |

*Может ли сервер изменять значение квара, используя функцию песочницы кваров.
</details>

## Установка

1. NextClient работает только с версией движка 8684, убедитесь, что вы находитесь на бета-ветви "steam_legacy - Pre-25th Anniversary Build" в Steam (⚠️ вам нужны файлы официальной игры из Steam! Вы не можете установить NextClient на пиратские клиенты!)
2. Скопируйте всю папку Counter-Strike 1.6 (Half-Life) в отдельную папку вне папки Steam
3. Перейдите в папку, куда вы только что скопировали все файлы CS 1.6 и переименуйте steam_api.dll в steam_api_orig.dll
4. Скопируейте все файлы NextClient в папку куда вы копировали все файлы CS 1.6
5. Запускайте игру через cstrike.exe

### ⚠️ Внимание! Никогда не помещайте файлы NextClient прямо в папку установки игры в Steam! Это приведет к VAC бану! ⚠️

## Смена мастер сервера

Файл конфигурации находится по пути `platform\config\MasterServer.vdf`
```vdf
"MasterServer"
{
   "Selected"     "1"       // Номер элемента из раздела Servers. Нумерация начинается с 0.
   "CacheServers" "false"   // В случае недоступности мастер сервера, список серверов будет взят из кэша.
                            // В кэш попадают сервера из последнего запроса, который был полностью завершён.
   "Servers"
   {
      "Steam"
      {
         "address" "hl1master.steampowered.com:27011" // Адрес мастер сервера.
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
Требования:
- Последняя версия MSVC 2022
- Cmake 3.21 или выше
- Ninja (необязательно, но крайне рекомендуется, для быстрой сборки проекта)

Запустите x86 Native Tools Command Prompt for VS 2022
```
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
cmake -G "Ninja" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
# или если у вас не установлен Ninja: cmake -G "NMake Makefiles" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release 
cmake --build cmake-build-release -t BUILD_ALL

```

Теперь можно выполнить цель INSTALL_ALL, она скопирует все необходимые файлы, включая ассеты, в отдельную папку.
```
set NEXTCLIENT_INSTALL_DIR=<абсолютный путь к папке>
cmake -G "Ninja" -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -t INSTALL_ALL
```

Вы также можете собрать проект из CLion, VS Code или Visual Studio. Используйте цель BUILD_ALL для сборки.  
Имейте в виду, что поддерживаются только генераторы Ninja и NMake Makefiles. Поэтому если вы хотите собрать проект в Visual Studio вы должны поменять его в настройках, потому-что Visual Studio по-умолчанию использует генератор Visual Studio 17 2022.

## Благодарности
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - за огромное количество фидбека и багрепортов
- [fl0werD](https://github.com/fl0werD) - за разработку Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - за проект [csldr](https://github.com/mikkokko/csldr), фичи из которого мы проинтегрировали в NextClient
- [Felipe](https://github.com/LAGonauta) - за проект [MetaAudio](https://github.com/LAGonauta/MetaAudio)
- [MoeMod](https://github.com/MoeMod) - за проект [Thanatos-Launcher](https://github.com/MoeMod/Thanatos-Launcher), он очень помог при реализации GameUI и VGUI2
- [tmp64](https://github.com/tmp64) - за проект [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk)
- [TsarVar](https://tsarvar.com) - за идею JS API для gameui
- Valve - за Counter-Strike 1.6 и лояльное отношение к моддерскому сообществу

Спасибо всем кто поддерживает проект баг репортами, предложениями и словами поддержки.
