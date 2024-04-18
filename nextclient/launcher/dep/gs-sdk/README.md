[![Travis](https://img.shields.io/travis/GameAnalytics/GA-SDK-CPP/master.svg)](https://github.com/GameAnalytics/GA-SDK-CPP)

GA-SDK-CPP
==========

GameAnalytics C++ SDK

Documentation can be found [here](https://gameanalytics.com/docs/cpp-sdk).

Supported platforms:

* Mac OS X
* Windows 32-bit and 64-bit
* Linux (tested on Ubuntu 16.04)
* UWP
* Tizen

Dependencies
------------

* python 2.7 or higher (not working for python 3)
  * **Windows:** Go [here](https://www.python.org/downloads/) to download and install Python
  * **Mac:** Should come with Python out of the box or else run *'brew install python'*
* cmake (will be automatically downloaded)
* **Mac:** XCode
* **Windows:** Visual Studio 2015 or 2017 + Windows Development Kit
* **Linux:** LLVM's libc++

Changelog
---------
**3.2.6**
* changed event uuid field name

**3.2.5**
* added event uuid to events sent

**3.2.4**
* fixed progression tries bug
* fixed max path length for configure writable path method

**3.2.3**
* added error events to be sent for invalid custom event fields used
* added optional mergeFields argument to event methods to merge with global custom fields instead of overwrite them

**3.2.2**
* fixed missing custom event fields for when trying to fix missing session end events

**3.2.1**
* added option to use custom log handler

**3.2.0**
* added global custom event fields function to allow to add custom fields to events sent automatically by the SDK

**3.1.3**
* added functionality to force a new user in a/b testing without having to uninstall app first, simply use custom user id function to set a new user id which hasn't been used yet

**3.1.2**
* fix to custom event fields

**3.1.1**
* small fixes to shared libraries

**3.1.0**
* added custom event fields feature

**3.0.8**
* updated client ts validator

**3.0.7**
* removed memory info from automatic crash reporting

**3.0.6**
* added disable device info method

**3.0.5**
* added godot to version validator

**3.0.4**
* removed gender, birth year and facebook methods
* updated curl and openssl libraries

**3.0.3**
* A/B testing fixes

**3.0.2**
* remote configs fixes

**3.0.1**
* various bug fixes

**3.0.0**
* command center is now called remote configs
* A/B testing support added

**2.1.9**
* fixes to command center

**2.1.8**
* error reporting improved

**2.1.7**
* bug fix to too large log files

**2.1.6**
* added check if log files and database can't be created

**2.1.5**
* refactored code for singleton classes

**2.1.4**
* fixed hanging background thread after closing application
* progression event bug fix

**2.1.3**
* removed std::string from the SDK

**2.1.2**
* thread fixes
* dll freeze bug fix

**2.1.1**
* thread fixes

**2.1.0**
* added automatic error reporting
* added enable/disable event submission

**2.0.4**
* fixed business event validation

**2.0.3**
* fixed shutdown issues

**2.0.2**
* fixed thread hanging on shutdown

**2.0.1**
* added command center functions to extern interface

**2.0.0**
* added command center functionality

**1.4.6**
* fixes for occasional crashes when shutting down
* fixes crashes on UWP

**1.4.5**
* fixes for Linux interface

**1.4.4**
* bug fix to events thread stopping and not starting again

**1.4.3**
* added custom dimensions to design and error events
* added GAState zero initialisation of fields
* fixed linux build script

**1.4.2**
* fixed session length bug
* fixed to not allow adding events when not in a session

**1.4.1**
* renamed onStop function to onSuspend
* added a onQuit function

**1.4.0**
* updated to new logging library (windows, osx, linux)
* added precompiled GameAnalytics libraries

**1.3.8**
* small correction to use int instead of double for session num

**1.3.7**
* https fix in curl library for mac

**1.3.6**
* bug fix for end session when using manual session handling

**1.3.5**
* session length precision improvement

**1.3.4**
* custom user id bug fix

**1.3.3**
* bug fix to dupplicate logs send to console (windows, mac, linux)

**1.3.2**
* added OS version, device model and device manufacturer to event (windows, mac)

**1.3.1**
* added support for Linux
* logging initialisation updated for windows and mac (windows, mac)

**1.3.0**
* added support for Tizen

**1.2.4**
* removed unused files

**1.2.3**
* fixed build script for UWP
* changed persistent path for UWP

**1.2.2**
* fixed issue with onstop method

**1.2.1**
* possible to set custom dimensions and demographics before initialise

**1.2.0**
* added UWP support

**1.1.1**
* fix to empty user id

**1.1.0**
* switched to use curl as network library

**1.0.1**
* fix for empty user id in events

**1.0.0**
* Initial version with Windows and Mac OS X support

How to build
------------

To start a build for all supported targets simply call

**Mac/Linux: ./build.sh**

**Windows: build.bat** (must be run as administrator when building for Tizen). By default the build script tries to use Visual Studio 2017 to use Visual Studio 2015 call **build.bat -v 2015**

This will build all targets and first time intall CMake and Tizen SDK.

Or call **build.bat -h** or **./build.sh -h** to display the usage of the script and it will list all available targets.

To build just a specific target call command with argument **-t <TARGET>** (available targets can be shown calling with **-h** argument).

How to develop
--------------

If you developing the SDK, you can use the generated project files to work with.

They are located under **build/jenkins/build/[target]** after calling the **build.sh** or **build.bat**.


Folderstructure
---------------

* **build** - Contains scripts for automated project setup, building and creating of the final exstracted library
* **build/cmake** - Contains the cmake files for the project
* **build/jenkins** - Contains the scripts and working directories for the automated builds
* **export** - Target folder for the automated export of the GA lib
* **precompiled** - Contains precompiled libraries for the different targets if you don't want to comile them yourself
* **source** - Contains the complete source code for the project including the dependencies
* **tests** - Contains tests for testing the functionality in the GA SDK, to run tests run **tests/run_tests_osx.py** (mac only)

Lib Dependencies
----------------

* **crossguid** (*as source*) - Cross platform library to generate a Guid.
* **cryptoC++** (*as source*) - collection of functions and classes for cryptography related tasks.
* **curl** (*as binary*) - library used to make HTTP requests.
* **jsonCpp** (*as source*) - lightweight C++ library for manipulating JSON values including serialization and deserialization.
* **openssl** (*as binary*) - used by **curl** to make HTTPS requests.
* **plog** (*as source*) - library used for logging.
* **SQLite** (*as source*) - SQLite is a software library that implements a self-contained, serverless, zero-configuration, transactional SQL database engine.

*as source* means the dependency will be compiled with the project itself, *as binary* means the dependency is prebuild and will be linked to the project

Preperation before usage of the SDK
-----------------------------------

* Include the following directories in your C++ project's include paths:
  * **source/gameanalytics**

* In your C++ project, link to the following libraries:
  * All libraries in **source/dependencies/curl/lib/[target]**
  * All libraries in **source/dependencies/openssl/1.0.2h/libs/[target]**
  * Produced library from build script found in **export/[target]-static/Release/**


Usage of the SDK
----------------

Remember to include the GameAnalytics header file wherever you are using the SDK:

``` c++
 #include "GameAnalytics.h"
```

### Custom log handler
If you want to use your own custom log handler here is how it is done:
``` c++
void logHandler(const char *message, gameanalytics::EGALoggerMessageType type)
{
    // add your logging in here
}

gameanalytics::GameAnalytics::configureCustomLogHandler(logHandler);
```

### Configuration

Example:

``` c++
 gameanalytics::GameAnalytics::setEnabledInfoLog(true);
 gameanalytics::GameAnalytics::setEnabledVerboseLog(true);

 gameanalytics::GameAnalytics::configureBuild("0.10");

 {
     std::vector<std::string> list;
     list.push_back("gems");
     list.push_back("gold");
     gameanalytics::GameAnalytics::configureAvailableResourceCurrencies(list);
 }
 {
     std::vector<std::string> list;
     list.push_back("boost");
     list.push_back("lives");
     gameanalytics::GameAnalytics::configureAvailableResourceItemTypes(list);
 }
 {
     std::vector<std::string> list;
     list.push_back("ninja");
     list.push_back("samurai");
     gameanalytics::GameAnalytics::configureAvailableCustomDimensions01(list);
 }
 {
     std::vector<std::string> list;
     list.push_back("whale");
     list.push_back("dolphin");
     gameanalytics::GameAnalytics::configureAvailableCustomDimensions02(list);
 }
 {
     std::vector<std::string> list;
     list.push_back("horde");
     list.push_back("alliance");
     gameanalytics::GameAnalytics::configureAvailableCustomDimensions03(list);
 }
```

### Initialization

Example:

``` c++
 gameanalytics::GameAnalytics::initialize("<your game key>", "<your secret key");
```

### Send events

Example:

``` c++
 gameanalytics::GameAnalytics::addDesignEvent("testEvent");
 gameanalytics::GameAnalytics::addBusinessEvent("USD", 100, "boost", "super_boost", "shop");
 gameanalytics::GameAnalytics::addResourceEvent(gameanalytics::Source, "gems", 10, "lives", "extra_life");
 gameanalytics::GameAnalytics::addProgressionEvent(gameanalytics::Start, "progression01", "progression02");
```
