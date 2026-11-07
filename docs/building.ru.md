# Сборка NextClient и настройка IDE

[Английская версия](building.md) | [Вся документация](README.ru.md) | [Проверки](testing/README.ru.md)

NextClient собирается для 32-битной платформы x86 с помощью CMake. Пресеты задают генератор, архитектуру и каталог сборки. Зависимости устанавливаются через vcpkg из репозитория. Ниже описаны подготовка исходников, сборка клиента и настройка IDE.

## Требования

Для обеих платформ нужны Git, Git LFS и CMake. Минимальная версия CMake для проекта: 3.21; установленная версия также должна поддерживать выбранный генератор.

### Windows

- Visual Studio 2022 или 2026 с актуальными инструментами MSVC для разработки на C++ и Windows SDK.
- Ninja при использовании пресетов `ninja`.

Откройте `x86 Native Tools Command Prompt for VS` установленной версии Visual Studio. Выполняйте команды сборки в этой среде, чтобы Ninja использовал компилятор x86.

### Linux

- GCC 15 или новее с поддержкой 32-битной сборки. В Ubuntu нужны пакеты `gcc-multilib` и `g++-multilib`.
- Ninja, `pkg-config`, Python 3, `curl`, `zip`, `unzip` и `tar`.
- 32-битные заголовки и библиотеки разработки OpenGL и X11.

Полный список пакетов среды Linux находится в [процессе сборки GitHub Actions](../.github/workflows/build.yml).

## Подготовка исходников

Настройте Git LFS до клонирования и загрузите подмодули:

```text
git lfs install
git clone --recurse-submodules https://github.com/CS-NextClient/NextClient.git
cd NextClient
```

Для существующего клона обновите подмодули и загрузите файлы Git LFS:

```text
git submodule update --init --recursive
git lfs pull
```

Подготовьте vcpkg из корня репозитория. В Windows:

```bat
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

В Linux:

```bash
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
```

Остальные команды выполняются из корня репозитория.

## Выбор пресета

Пресет конфигурации определяет среду сборки. Пресет сборки выбирает конфигурацию `Release` или `Debug`. Стандартные пресеты хранятся в [`CMakePresets.json`](../CMakePresets.json).

| Пресет конфигурации | Среда | Пресеты сборки |
|---|---|---|
| `vs2022` | Windows, Visual Studio 2022, Win32 | `vs2022-release`, `vs2022-debug` |
| `vs2026` | Windows, Visual Studio 2026, Win32 | `vs2026-release`, `vs2026-debug` |
| `ninja` | Windows, MSVC x86, Ninja Multi-Config | `ninja-release`, `ninja-debug` |
| `linux` | Linux, GCC x86, Ninja Multi-Config | `linux-release`, `linux-debug` |

Для тестов предусмотрены `vs2022-tests`, `ninja-tests` и `ninja-profiler-tests`. Для сборки с профилированием используется `ninja-profiler`. У этих пресетов есть соответствующие пресеты сборки с суффиксом `-release`. Порядок выбора проверок описан в [указателе проверок](testing/README.ru.md).

## Сборка клиента

Выберите одну пару команд для установленного набора инструментов.

### Windows

Visual Studio 2022:

```bat
cmake --preset vs2022
cmake --build --preset vs2022-release --target BUILD_ALL
```

Visual Studio 2026:

```bat
cmake --preset vs2026
cmake --build --preset vs2026-release --target BUILD_ALL
```

Ninja с MSVC x86:

```bat
cmake --preset ninja
cmake --build --preset ninja-release --target BUILD_ALL
```

### Linux

```bash
cmake --preset linux
cmake --build --preset linux-release --target BUILD_ALL
```

### Отдельный модуль

Чтобы собрать один модуль, укажите его цель, например `client_mini`:

```text
cmake --build --preset ninja-debug --target client_mini
```

Перед этой командой выполните конфигурацию `cmake --preset ninja`. Собирайте каждую выбранную цель отдельной командой.

Цель `BUILD_ALL` собирает комплект модулей клиента и доступна при `NEXTCLIENT_BUILD_TESTS=OFF`. При включенных тестах выбирайте отдельные цели из [указателя проверок](testing/README.ru.md).

## Каталоги результатов

Стандартный каталог сборки: `build/<пресет конфигурации>`. Он содержит конфигурацию CMake и промежуточные файлы. Собранные исполняемые файлы и библиотеки клиента находятся в следующих каталогах:

| Сборка | Каталог результатов |
|---|---|
| Windows, тесты и профилирование отключены | `out/bin/<конфигурация>/` |
| Windows, тесты или профилирование включены | `build/<пресет>/out/bin/<конфигурация>/` |
| Linux | `build/<пресет>/out/bin/<конфигурация>/` |

В Windows библиотеки клиента и GameUI находятся в подкаталоге `cstrike/cl_dlls/`. Для стандартного пресета `linux` значение `<пресет>` равно `linux`. В локальном пресете каталог сборки определяется его параметром `binaryDir`.

## Копирование в папку назначения

Чтобы цель `BUILD_ALL` скопировала модули и ресурсы в выбранную папку, задайте абсолютный путь в `NEXTCLIENT_INSTALL_DIR` при конфигурации. Например, в Windows:

```bat
cmake --preset vs2022 -DNEXTCLIENT_INSTALL_DIR="C:/Games/CS 1.6 - NextClient"
cmake --build --preset vs2022-release --target BUILD_ALL
```

В Linux:

```bash
cmake --preset linux -DNEXTCLIENT_INSTALL_DIR="/path/to/Half-Life-NextClient"
cmake --build --preset linux-release --target BUILD_ALL
```

Для запуска клиента подготовьте в папке назначения отдельную копию игры вне Steam по [инструкции установки](../README.ru.md#установка). В Linux используйте файлы нативной Linux-версии игры. `BUILD_ALL` копирует библиотеки, ресурсы и скрипт запуска `nextclient.sh` для Linux. Если включено профилирование, в комплект также входит библиотека Tracy.

Для подготовки пакета Windows к распространению укажите отдельную папку пакета и следуйте [руководству по сборке установщика](tools/installer.ru.md).

### Локальный пресет

Чтобы сохранить путь назначения, создайте `CMakeUserPresets.json` в корне репозитория. Файл исключен из Git. Пример для Visual Studio 2022:

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

Используйте имена локальных пресетов при конфигурации и сборке:

```bat
cmake --preset vs2022-local
cmake --build --preset vs2022-local-release --target BUILD_ALL
```

## Работа в IDE

Откройте корень репозитория и выберите пресеты для установленного набора инструментов. Если задан `NEXTCLIENT_INSTALL_DIR`, сборка `BUILD_ALL` также копирует результат в эту папку.

### CLion

1. В Windows выберите набор инструментов Visual Studio с архитектурой x86 и пресет `ninja`. В Linux выберите GCC и пресет `linux`.
2. В `Settings -> Build -> CMake` включите выбранные пресеты и нужную конфигурацию `Release` или `Debug`.
3. Соберите цель `BUILD_ALL` или отдельный модуль.

### Visual Studio

1. Откройте папку репозитория.
2. Выберите пресет `vs2022` или `vs2026`, соответствующий установленной версии Visual Studio, и конфигурацию `Release` или `Debug`.
3. Соберите цель `BUILD_ALL` или отдельный модуль.

### VS Code

1. Установите расширение `ms-vscode.cmake-tools`.
2. В Windows при использовании Ninja запустите VS Code из `x86 Native Tools Command Prompt for VS`.
3. Выберите пресет конфигурации `vs2022`, `vs2026`, `ninja` или `linux` и соответствующий пресет сборки.
4. Соберите цель `BUILD_ALL` или отдельный модуль.

## Проверка результата

Убедитесь, что команда сборки завершилась успешно и файлы появились в ожидаемом каталоге результатов. Если задан `NEXTCLIENT_INSTALL_DIR`, проверьте также папку назначения. Проверки измененного поведения выбирайте по [указателю проверок](testing/README.ru.md), а подготовку и установку согласованного комплекта модулей выполняйте по [процедуре установки и проверки NitroApi](testing/installation-and-factory.ru.md).

Если CMake сообщает об указателях Git LFS, выполните `git lfs pull` и повторите конфигурацию. Если отсутствует цель `BUILD_ALL`, проверьте значение `NEXTCLIENT_BUILD_TESTS` и выберите конфигурацию с отключенными тестами для сборки распространяемого клиента.
