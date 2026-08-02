# Задание: обобщение GL-кода ветки feature/advanced-radar под будущий рендер

Этот документ - самодостаточная спецификация для исполняющей сессии. Цель:
вынести из радарного кода нейтральные GL-компоненты, которые понадобятся новому
рендеру (см. docs/new-renderer-plan.md, фазы 0-1), не меняя поведение радара.
Все пункты, кроме явно отмеченного добавления логирования ошибок шейдеров,
строго behavior-preserving (перенос кода, дедупликация, переименования).

Номера строк соответствуют коммиту `5c8ca11` (HEAD ветки `feature/advanced-radar`).

## Порядок работы

1. Работать в ветке `feature/advanced-radar`. `git push` не выполнять.
2. Один пункт = один коммит. Сообщение: одна строка, императив, строчная буква,
   без префиксов типа `fix:`, без трейлеров и Co-Authored-By. Предложенное
   сообщение указано в каждом пункте.
3. Диффы строго по месту: не переформатировать нетронутые строки, не менять
   соседний код "заодно". Существующие комментарии переезжают вместе с кодом
   (с нейтрализацией слова "radar" там, где это указано).
4. Комментарии в коде только на английском и только для неочевидного. Новые
   комментарии в рамках задания почти нигде не нужны.
5. Стиль нового кода: методы/функции `CamelCase`; локальные переменные
   `snake_case`; константы `constexpr` с префиксом `kCamelCase`; заголовки
   начинаются с `#pragma once`; глобальные/статические переменные без явной
   инициализации нулём (static storage обнуляется сам); control-flow блоки в
   фигурных скобках.
6. После каждого пункта: сборка цели `client_mini` (команды ниже) должна быть
   зелёной. После пункта 3.1 дополнительно собрать и запустить
   `client_mini-tests`.
7. Игровой smoke-тест выполняет пользователь после завершения всех пунктов;
   от исполняющей сессии требуется только зелёная сборка и тесты.

## Сборка и проверка

`cmake` не в PATH; использовать cmake из VS18 через окружение vcvarsall x86.
Надёжный способ - инструмент PowerShell (вывод ninja из-под голого
`cmd /c` в Bash часто теряется):

```powershell
$vcvars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat'
$cmake  = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

# сборка client_mini (конфигурация уже создана в build/ninja-local)
& cmd /c "`"$vcvars`" x86 >nul 2>&1 && `"$cmake`" --build build/ninja-local --target client_mini --config Debug" 2>&1 | Select-Object -Last 30

# юнит-тесты (конфигурация build/ninja-local-tests, NEXTCLIENT_BUILD_TESTS=ON)
& cmd /c "`"$vcvars`" x86 >nul 2>&1 && `"$cmake`" --build build/ninja-local-tests --target client_mini-tests --config Debug" 2>&1 | Select-Object -Last 30
& .\build\ninja-local-tests\out\bin\Debug\client_mini-tests.exe
```

Передавать ровно одну цель на один вызов `--build` (несколько целей за раз
ломают сборку ложной ошибкой про каталог). Если каталога `build/ninja-local`
или `build/ninja-local-tests` нет, сначала выполнить конфигурацию:
`"$cmake" --preset ninja-local` и `"$cmake" --preset ninja-local-tests`
(из корня репозитория, также из-под vcvarsall x86).

## Партия 1: пакет ncl_gl, загрузчик GL

### 1.1 Создать пакет ncl_gl и перенести в него загрузчик

Сейчас загрузчик GL-функций живёт в
`nextclient/client_mini/src/hud/radar/RadarGl.cpp/.h`
(функция `RadarGl_LoadProc`), а ленивая инициализация glad продублирована в
двух местах: `RadarShader.cpp:276-287` (функция `EnsureGlad`, флаги
`g_GladDone`/`g_GladOk` объявлены на строках 19-20) и `MapRender.cpp:17-28`
(функция `EnsureGlad`, флаг `g_GladReady`).

Шаги:

1. Создать пакет `nextclient/packages/ncl_gl` по образцу `ncl_utils`
   (STATIC-библиотека), но без vcpkg-зависимостей:

   `nextclient/packages/ncl_gl/CMakeLists.txt`:
   ```cmake
   cmake_minimum_required(VERSION 3.21)
   project(ncl_gl)

   set(CMAKE_CXX_STANDARD 23)

   add_library(${PROJECT_NAME} STATIC
       src/loader.cpp
   )
   add_library(ncl_gl::ncl_gl ALIAS ${PROJECT_NAME})

   target_include_directories(${PROJECT_NAME} PUBLIC
           include
   )

   target_link_libraries(${PROJECT_NAME} PUBLIC
           glad::glad
   )
   ```

   Цель `glad::glad` уже существует к моменту обработки пакетов
   (корневой CMakeLists.txt: `add_subdirectory(dep/glad)` на строке 73,
   `add_subdirectory(nextclient/packages)` на строке 78).

2. В `nextclient/packages/CMakeLists.txt` добавить строку
   `add_subdirectory(ncl_gl)` после строки 21 (`add_subdirectory(ncl_math)`).

3. Создать `nextclient/packages/ncl_gl/include/ncl_gl/loader.h`:
   ```cpp
   #pragma once

   namespace ncl_gl
   {
       // Resolves a GL entry point via wglGetProcAddress and falls back to
       // opengl32.dll for the GL 1.1 functions that wglGetProcAddress does not
       // return on some drivers. Returned as a raw pointer so this header does
       // not depend on glad; callers cast it to GLADloadproc.
       void* LoadProc(const char* name);

       // One-shot lazy glad initialization over LoadProc. Returns whether glad
       // loaded; subsequent calls return the cached result. Call only with a
       // current GL context on the calling thread.
       bool EnsureLoaded();

       // Whether EnsureLoaded already ran and succeeded. Never triggers loading.
       bool IsLoaded();
   }
   ```

4. Создать `nextclient/packages/ncl_gl/src/loader.cpp`. Файл начинается с
   включений (собственный заголовок обязателен: без видимого объявления
   квалифицированные определения `ncl_gl::...` не скомпилируются):
   ```cpp
   #include <ncl_gl/loader.h>

   #include <windows.h>
   #include <glad/glad.h>
   ```
   Тело `RadarGl_LoadProc` из `RadarGl.cpp:5-17` переносится дословно в
   `void* ncl_gl::LoadProc(const char* name)` (включая статический
   `LoadLibraryA("opengl32.dll")` фоллбэк). Остальное реализуется так:
   ```cpp
   namespace
   {
       bool g_LoadDone;
       bool g_LoadOk;
   }

   bool ncl_gl::EnsureLoaded()
   {
       if (g_LoadDone)
       {
           return g_LoadOk;
       }

       g_LoadDone = true;
       g_LoadOk = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(LoadProc)) != 0;

       return g_LoadOk;
   }

   bool ncl_gl::IsLoaded()
   {
       return g_LoadDone && g_LoadOk;
   }
   ```

5. Переключить потребителей, сохранив семантику каждого:
   - `RadarShader.cpp`: заменить `#include "RadarGl.h"` (строка 2) на
     `#include <ncl_gl/loader.h>`; удалить флаги `g_GladDone`/`g_GladOk`
     (строки 19-20); тело локальной `EnsureGlad` (строки 276-287) заменить на
     ```cpp
     bool EnsureGlad()
     {
         return ncl_gl::EnsureLoaded() && GLAD_GL_VERSION_2_0 != 0;
     }
     ```
     Имя и все вызовы `EnsureGlad` внутри файла не меняются. Проверка
     `GLAD_GL_VERSION_2_0` теперь вычисляется при каждом вызове вместо
     кэширования: это дешёвое чтение глобального int glad, семантика прежняя.

     Дополнительно: `g_GladOk` читается ещё в одном месте вне `EnsureGlad` -
     `RadarShader_ResetGl`, строка 563 (`if (g_GladOk)`, гейтит glDelete*-вызовы).
     Заменить на `if (ncl_gl::IsLoaded())`. Семантика эквивалентна: старый флаг
     дополнительно требовал GL 2.0, но GL-хэндлы ненулевые только если `Init()`
     прошёл `EnsureGlad` (который требует GL 2.0), поэтому в ветке удаления
     разница не наблюдаема. `IsLoaded()` намеренно без побочной загрузки:
     инициировать загрузку glad из ResetGl нельзя.
   - `MapRender.cpp`: заменить `#include "RadarGl.h"` (строка 2) на
     `#include <ncl_gl/loader.h>`; удалить флаг `g_GladReady` (строка 17);
     тело локальной `EnsureGlad` (строки 19-28) заменить на
     `return ncl_gl::EnsureLoaded();`. Вызовы внутри файла не меняются.
     Здесь проверки версии не было и не должно появиться.

6. Удалить `src/hud/radar/RadarGl.cpp` и `src/hud/radar/RadarGl.h`; убрать оба
   файла из списка исходников в `nextclient/client_mini/CMakeLists.txt`
   (строки 99-100); добавить `ncl_gl::ncl_gl` в `target_link_libraries`
   (после `ncl_math::ncl_math`, строка 132).

Проверка: сборка зелёная; `grep -rn "RadarGl" nextclient/` не находит ничего.

Коммит: `extract shared GL loader into an ncl_gl package`

## Партия 2: пакет ncl_gl, компиляция и линковка программ

### 2.1 Перенести Program/CompileShader/LinkProgram/LoadProgram и добавить лог ошибок

Сейчас в `RadarShader.cpp` в анонимном namespace живут: `kMaxUniforms` и
структура `Program` (строки 23-33 вместе с комментарием), `CompileShader`
(строки 180-196), `LinkProgram` (строки 198-245, включая обязательный к
сохранению комментарий про glBindAttribLocation до линковки на строках
222-225) и `LoadProgram` (строки 247-274). Ошибки компиляции/линковки сейчас
молча глотаются (InfoLog не читается) - это единственное разрешённое изменение
поведения: добавить возврат текста ошибки и вывод его в консоль разработчика.

Шаги:

1. Создать `nextclient/packages/ncl_gl/include/ncl_gl/program.h`:
   ```cpp
   #pragma once
   #include <string>

   #include <glad/glad.h>

   namespace ncl_gl
   {
       inline constexpr int kMaxUniforms = 16;

       // <перенести сюда комментарий из RadarShader.cpp:23-25, заменив
       //  "the per-program uniform enum below" на "a per-program uniform enum">
       struct Program
       {
           GLuint prog;
           bool ok;
           GLint loc[kMaxUniforms];
       };

       GLuint CompileShader(GLenum type, const char* src, std::string* error_log = nullptr);
       GLuint LinkProgram(
           const char* vs_src,
           const char* fs_src,
           const char* const* attribs,
           int attrib_count,
           std::string* error_log = nullptr
       );
       void LoadProgram(
           Program& p,
           const char* vs_src,
           const char* fs_src,
           const char* const* names,
           int count,
           const char* const* attribs = nullptr,
           int attrib_count = 0,
           std::string* error_log = nullptr
       );
   }
   ```

2. Создать `nextclient/packages/ncl_gl/src/program.cpp`. Файл начинается с
   `#include <ncl_gl/program.h>`; определения либо оборачиваются в
   `namespace ncl_gl { ... }`, либо квалифицируются `ncl_gl::`. Значения
   параметров по умолчанию остаются ТОЛЬКО в объявлениях в program.h; в
   определениях в program.cpp их не повторять (у переносимого `LoadProgram`
   убрать `= nullptr` и `= 0` из списка параметров, иначе MSVC даст C2572
   "redefinition of default argument"). Тела переносятся дословно, плюс на
   путях неуспеха перед `glDeleteShader`/`glDeleteProgram` при непустом
   `error_log` дописывается InfoLog. Образец для шейдера (аналогично для
   программы с glGetProgramiv/glGetProgramInfoLog):
   ```cpp
   if (error_log != nullptr)
   {
       GLint len = 0;
       glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

       if (len > 1)
       {
           std::string log(static_cast<size_t>(len), '\0');
           glGetShaderInfoLog(shader, len, nullptr, log.data());
           error_log->append(log.c_str());
       }
   }
   ```
   `LinkProgram` пробрасывает `error_log` в оба вызова `CompileShader` и
   использует его сам; `LoadProgram` пробрасывает в `LinkProgram`. Добавить
   `src/program.cpp` в `add_library` в CMakeLists пакета. Пакет собирается с
   `/WX`: следить за преобразованиями типов (пример выше их избегает).

3. В `RadarShader.cpp`: удалить перенесённое; добавить
   `#include <ncl_gl/program.h>` и `#include "../../main.h"` (доступ к
   `gEngfuncs`, по образцу MapRender.cpp:3); типы `g_Mask`/`g_Marker`/`g_Blur`
   (строки 109-111) сменить на `ncl_gl::Program`; `static_assert` на строке
   107 переписать через `ncl_gl::kMaxUniforms`; в `RadarShader_ResetGl` на
   строках 602-604 (`g_Mask = Program{};` и т.д.) квалифицировать тип:
   `ncl_gl::Program{}`; вызовы `LoadProgram(...)` в `Init()` (строки 304,
   313, 323) заменить на `ncl_gl::LoadProgram(...)` с передачей лога и
   выводом при неуспехе:
   ```cpp
   std::string error_log;
   ncl_gl::LoadProgram(g_Mask, vs_src.c_str(), mask_src.c_str(), kMaskUniforms, kMaskCount, nullptr, 0, &error_log);

   if (!g_Mask.ok)
   {
       gEngfuncs.Con_DPrintf("radar mask shader failed: %s\n", error_log.c_str());
   }
   ```
   Для каждого из трёх вызовов очищать `error_log` перед использованием
   (`error_log.clear()`) и указывать имя программы в сообщении
   (`radar mask shader failed` / `radar marker shader failed` /
   `radar backdrop shader failed`).

Проверка: сборка зелёная. Опционально (не коммитить): временно испортить
`assets/shaders/radar_mask.frag` и убедиться при запуске с `-dev`, что ошибка
печатается в консоль; затем откатить порчу.

Коммит: `move GL program helpers into ncl_gl with compile and link error logging`

## Партия 3: ncl_math и нейтральные переименования

### 3.1 Перенести SegPointDistSq в ncl_math

Функция `SegPointDistSq(const float* a, const float* b, const float* p)`
(квадрат расстояния от точки до отрезка) сейчас локальна в
`SmokeRegistry.cpp:46-56`, но полностью общая и понадобится рендеру для
culling-тестов сфера/отрезок.

Шаги:

1. Перенести функцию дословно в
   `nextclient/packages/ncl_math/include/ncl_math/vec3.h` в namespace
   `ncl_math`, рядом с другими хелперами над сырыми `const float*` (после
   `VectorMA`/`VectorMA_2`, то есть после строки 210), с `inline`. Функция
   использует `std::clamp`: убедиться, что в vec3.h включён `<algorithm>`,
   при отсутствии добавить.
2. В `SmokeRegistry.cpp` удалить локальную копию (строки 46-56) и
   квалифицировать вызов на строке 159: `ncl_math::SegPointDistSq(...)`.
3. Добавить тесты: новый файл `nextclient/client_mini/tests/NclMathTests.cpp`
   (добавить в `add_executable` в `tests/CMakeLists.txt`, строки 7-11) с
   проверками: проекция внутрь отрезка; точка за пределами каждого из концов
   (clamp к концу); вырожденный отрезок `a == b`; точка на отрезке
   (расстояние 0). Файл по образцу `RadarUtilTests.cpp`:
   `#include <gtest/gtest.h>` и `#include <ncl_math/vec3.h>`, тесты в форме
   `TEST(...)`. Других правок `tests/CMakeLists.txt` не требуется:
   `ncl_math::ncl_math` уже слинкован (строка 16).

Проверка: сборка `client_mini` и `client_mini-tests` зелёная, тесты проходят.

Коммит: `move segment-point distance helper into ncl_math`

### 3.2 Переименовать радарные view-глобалы в нейтральные

`g_RadarViewOrigin`/`g_RadarViewAngles` - это главная камера кадра, ценная не
только радару (рендер будет строить по ней view-матрицы). Радарное имя мешает
следующим потребителям.

Полный список вхождений (проверить свежим grep перед правкой):

- `nextclient/client_mini/src/view.h:5-8` (комментарий и extern-объявления);
- `nextclient/client_mini/src/view.cpp:20-21` (определения), `486-487` (запись);
- `nextclient/client_mini/src/hud/radar/Visibility.cpp:217-218`;
- `nextclient/client_mini/src/hud/HudRadarAdvanced.cpp:199-200`, `588`.

Шаги:

1. Переименовать `g_RadarViewOrigin` -> `g_MainViewOrigin`,
   `g_RadarViewAngles` -> `g_MainViewAngles` во всех перечисленных местах.
2. Комментарий в `view.h:5-6` заменить на:
   `// Local view origin/angles captured from the last main-view ViewCalcRefdef pass (spectator picture-in-picture passes excluded).`
   Существующий комментарий с обоснованием проверки `nextView == 0` на месте
   записи в `view.cpp` не трогать.

Проверка: сборка зелёная; `grep -rn "g_RadarView" nextclient/` пуст.

Коммит: `rename radar view capture globals to main view`

## Вне объёма задания (не делать)

- Не перегенерировать glad под 3.3: остаётся 3.1, это фаза 0 плана рендера.
- Не переносить `RadarUtil_AnchorPath`: на этой ветке у неё нет потребителей,
  а перенос создаст конфликт при будущем cherry-pick харнесса из ветки
  `radar-autotests-local`.
- Не менять семантику `RadarShader_ResetGl` и VidInit-жизненного цикла
  (известные дыры закрываются централизованным журналом в фазе 0 рендера).
- Не выделять из RadarShader захват фреймбуфера/blur: это меняет пиксели и
  требует регрессионных базлайнов, которых на этой ветке нет.
- Не трогать батчер маркеров, uniform-таблицы программ, cmrc-упаковку
  шейдеров, engine_mini.

## Критерии приёмки всего задания

- Четыре коммита в указанном порядке, каждый собирается независимо.
- `client_mini` (build/ninja-local, Debug) и `client_mini-tests`
  (build/ninja-local-tests, Debug) зелёные; тесты проходят.
- `grep -rn "RadarGl" nextclient/` и `grep -rn "g_RadarView" nextclient/` пусты;
  в `SmokeRegistry.cpp` нет определения `SegPointDistSq`.
- Никаких изменений в `assets/`, `engine_mini/`, `gameui/` (кроме отсутствия
  таковых - проверить `git diff --stat` перед завершением).
