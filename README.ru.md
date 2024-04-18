Language: [EN](https://github.com/CS-NextClient/NextClient/README.md) | RU

NextClient
==========

NextClient это модификация для Counter-Strike 1.6, нацеленная на введение новой функциональности как для игроков, так и для разработчиков серверных модификаций на amxmodx.
В NextClient интегрированы некоторые фичи из [csldr](https://github.com/mikkokko/csldr), а так же интегрирован [MetaAudio](https://github.com/LAGonauta/MetaAudio). 

### Основные возможности:
 - Протектор - защищает клиент от вредоносных команд с сервера
 - Расширенные настройки видео - фикс FOV на разрешениях 16:9, возможность регулировать FOV и отдельно регулировать FOV для модели от первого лица
 - Расширенный kill feed - поддержка расширенного  kill feed [regamedll](https://github.com/s1lentq/ReGameDLL_CS/pull/858), отображение иконок убийств: через стену, через дым, без прицела, в прыжке, с доминированием, etc.
 - Расширенные настройки прицела - добавлены новые виды прицела: точка, T-образный, окружность
 - 2 схемы GUI с возможностью их смены через настройки, и возможность добавлять свои схемы не удаляя старые. 
 - Отображение более 255hp при использовании [серверного модуля](https://github.com/CS-NextClient/NextClientServerApi)
 - Отображение количества и размера оставшихся файлов, общего размера файлов и скорости загрузки при подключении на сервер
 - Цветной чат в консоли
 - Поддержка моделей оружия с анимациями осмотра
 - Различные улучшения из csldr (см. раздел кваров)

### Возможности для amxmodx разработчиков:
 - песочница кваров, возможность менять квары клиенту (из ограниченного списка) на время его нахождения на сервере
 - кастомизация kill feed
 - управление спрайтами на экране
 - расширенное FOV сообщение
 - поддержка эффектов для viewmodel
 - раздельный прекэш для обычного клиента cs 1.6 и NextClient
 - прекэш hud.txt и других стандартных ресурсов

### Новые квары

| Cvar name | Default value | Available in sandbox*       | Description |
| --- |---------------|-----------------------------| --- |
| viewmodel_disable_shift | 0             | Yes                         | Disable viewmodel shifting (when you looking up or down). |
| viewmodel_offset_x | 0             | Yes                         |  |
| viewmodel_offset_y | 0             | Yes                         |  |
| viewmodel_offset_z | 0             | Yes                         |  |
| viewmodel_fov | 90            | No                          | Min: 70<br/>Max: 100 |
| cl_crosshair_type | 0             | Yes                         | Crosshair type. 0 - crosshair, 1 - T-shaped, 2 - circle, 3 - dot. |
| cl_bobstyle | 0             | Yes                         | 0 for default bob, 1 for old style bob and 2 for CS:GO style bob. |
| cl_bobamt_vert | 0\.13         | Yes                         | Vertical scale for CS:GO style bob. |
| cl_bobamt_lat | 0\.32         | Yes                         | Lateral scale for CS:GO style bob. |
| cl_bob_lower_amt | 8             | Yes                         | Specifies how much the viewmodel moves inwards for CS:GO style bob. |
| cl_rollangle | 0             | Yes                         | Screen roll angle when strafing or looking (Quake effect). |
| cl_rollspeed | 200           | Yes                         | Screen roll speed when strafing or looking (Quake effect). |
| viewmodel_lag_scale | 0             | Yes                         | The value of the lag of the viewmodel from the crosshair (CS:GO effect). |
| viewmodel_lag_speed | 8             | Yes                         | The speed of the viewmodel following the crosshair (CS:GO effect). |
| fov_horplus | 0             | No                          | Enables Hor+ scaling for FOV. Fixes the FOV when playing with aspect ratios besides 4:3. |
| fov_angle | 90            | No (use ncl_setfov instead) | Min: 70<br/>Max: 100 |
| fov_lerp | 0             | No (use ncl_setfov instead) | FOV interpolation time in seconds. |
| hud_deathnotice_max | 5             | No                          | The maximum number of kill feed entries that can be displayed. |
| hud_deathnotice_old | 0             | No                          | Enable the old style of kill feed. |
| http_max_active_requests | 5             | No                          |  |
| http_max_requests_retries | 3             | No                          |   |

*Может ли сервер изменять значение квара, используя функцию песочницы кваров.

## Установка

1. NextClient работает только с версией движка 8684, убедитесь, что вы находитесь на бета-ветви "steam_legacy - Pre-25th Anniversary Build" в Steam
2. Скопируйте всю папку Counter-Strike 1.6 (Half-Life) в отдельное место вне папки Steam
3. Переименуйте steam_api.dll в steam_api_orig.dll
4. Поместите все файлы NextClient в папку с игрой
5. Запускайте игру через cs.exe

### ⚠️ Внимание! Никогда не помещайте файлы NextClient в папку установки игры в Steam! Это приведет к VAC бану! ⚠️

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

## Благодарности
- [Nordic Warrior](https://github.com/Nord1cWarr1or) - за огромное количество фидбека и багрепортов
- [fl0werD](https://github.com/fl0werD) - за разработку Sprite API
- [Mikko Kokko](https://github.com/mikkokko) - за проект [csldr](https://github.com/mikkokko/csldr), фичи из которого мы проинтегрировали в NextClient
- [Felipe](https://github.com/LAGonauta) - за проект [MetaAudio](https://github.com/LAGonauta/MetaAudio), который мы проинтегрировали в NextClient
- [TsarVar](https://tsarvar.com) - за идею JS API для gameui
- Valve - за Counter-Strike 1.6 и лояльное отношение к моддерскому сообществу

Спасибо всем кто поддерживает проект баг репортами, предложениями и словами поддержки.