#!/usr/bin/python

# This script extract the neccessary tools from the package folder,
# calls cmake to generate project files and builds them
import os
import platform
import sys
import getopt
import subprocess
import config as Config
import libs.tools as LibTools
import shutil
import fileinput

if platform.system() == 'Windows':
    from winreg import ConnectRegistry
    from winreg import HKEY_LOCAL_MACHINE
    from winreg import OpenKey
    from winreg import QueryValueEx

if os.name == "nt":
    __CSL = None

    def symlink(source, link_name):
        '''symlink(source, link_name)
           Creates a symbolic link pointing to source named link_name'''
        global __CSL
        if __CSL is None:
            import ctypes
            csl = ctypes.windll.kernel32.CreateSymbolicLinkW
            csl.argtypes = (ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32)
            csl.restype = ctypes.c_ubyte
            __CSL = csl
        flags = 0
        if source is not None and os.path.isdir(source):
            flags = 1
        if __CSL(link_name, source, flags) == 0:
            raise ctypes.WinError()

    os.symlink = symlink


def call_process(process_arguments, process_workingdir, silent=False, useOutput=False):
    print('Call process ' + str(process_arguments) + ' in workingdir ' + process_workingdir)
    current_workingdir = os.getcwd()

    if not LibTools.folder_exists(process_workingdir):
        os.makedirs(process_workingdir)

    output = ''
    os.chdir(process_workingdir)
    if silent is True:
        if useOutput is True:
            output = subprocess.check_output(process_arguments, stdout=open(os.devnull, 'wb')).strip()
        else:
            subprocess.check_call(process_arguments, stdout=open(os.devnull, 'wb'))
    else:
        if useOutput is True:
            output = subprocess.check_output(process_arguments).strip()
        else:
            subprocess.check_call(process_arguments)

    os.chdir(current_workingdir)

    return output


class Target(object):
    def __init__(self, name):
        self.name = name


class TargetCMake(Target):
    def __init__(self, name, generator):
        super(TargetCMake, self).__init__(name)
        self.generator = generator

    def build_dir(self):
        return os.path.abspath(os.path.join(__file__, '..', 'build', self.name))

    def sqlite_build_dir(self):
        return os.path.abspath(os.path.join(__file__, '..', 'build', 'sqlite', self.name))

    def create_project_file(self, noSqliteSrc="NO"):
        call_process(
            [
                os.path.join(
                    Config.CMAKE_ROOT,
                    'bin',
                    'cmake'
                ),
                '../../../cmake/gameanalytics/',
                '-DPLATFORM:STRING=' + self.name,
                '-DNO_SQLITE_SRC:STRING=' + noSqliteSrc,
                '-G',
                self.generator
            ],
            self.build_dir()
        )

    def create_sqlite_project_file(self):
        call_process(
            [
                os.path.join(
                    Config.CMAKE_ROOT,
                    'bin',
                    'cmake'
                ),
                '../../../../cmake/sqlite/',
                '-DPLATFORM:STRING=' + self.name,
                '-G',
                self.generator
            ],
            self.sqlite_build_dir()
        )

    def build(self, silent=False):
        raise NotImplementedError()


class TargetOSX(TargetCMake):
    def build(self, silent=False):
        call_process(
            [
                'xcodebuild',
                '-configuration',
                'Release'
            ],
            self.build_dir(),
            silent=silent
        )

        call_process(
            [
                'xcodebuild',
                '-configuration',
                'Debug'
            ],
            self.build_dir(),
            silent=silent
        )

        self.binary_name = {
            'osx-shared': 'libGameAnalytics.dylib',
            'osx-static': 'libGameAnalytics.a',
            'osx-static-no-sqlite-src': 'libGameAnalytics.a',
        }[self.name]
        self.target_name = {
            'osx-shared': 'GameAnalytics.bundle',
            'osx-static': 'libGameAnalytics.a',
            'osx-static-no-sqlite-src': 'libGameAnalytics.a',
        }[self.name]

        debug_dir = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Debug'))
        release_dir = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Release'))

        # remove folders if there
        LibTools.remove_folder(debug_dir)
        LibTools.remove_folder(release_dir)

        if not LibTools.folder_exists(debug_dir):
            os.makedirs(debug_dir)

        if not LibTools.folder_exists(release_dir):
            os.makedirs(release_dir)

        shutil.copy(
            os.path.join(self.build_dir(), 'Debug', self.binary_name),
            os.path.join(debug_dir, self.binary_name)
        )

        shutil.move(
            os.path.join(self.build_dir(), 'Debug', self.binary_name),
            os.path.join(debug_dir, self.target_name)
        )

        shutil.copy(
            os.path.join(self.build_dir(), 'Release', self.binary_name),
            os.path.join(release_dir, self.binary_name)
        )

        shutil.move(
            os.path.join(self.build_dir(), 'Release', self.binary_name),
            os.path.join(release_dir, self.target_name)
        )

        if "no-sqlite-src" in self.name:
            self.create_sqlite_project_file()

            sqlite_debug_dir = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Debug'))
            sqlite_release_dir = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Release'))

            call_process(
                [
                    'xcodebuild',
                    '-configuration',
                    'Release'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            call_process(
                [
                    'xcodebuild',
                    '-configuration',
                    'Debug'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            if not LibTools.folder_exists(sqlite_debug_dir):
                os.makedirs(sqlite_debug_dir)

            if not LibTools.folder_exists(sqlite_release_dir):
                os.makedirs(sqlite_release_dir)

            shutil.copy(
                os.path.join(self.sqlite_build_dir(),
                             'Debug', 'libSqlite.a'),
                os.path.join(sqlite_debug_dir, 'libSqlite.a')
            )

            shutil.copy(
                os.path.join(self.sqlite_build_dir(),
                             'Release', 'libSqlite.a'),
                os.path.join(sqlite_release_dir, 'libSqlite.a')
            )


class TargetWin(TargetCMake):
    @staticmethod
    def get_msbuild_path(vs="2019"):
        if vs == "2015":
            try:
                aReg = ConnectRegistry(None, HKEY_LOCAL_MACHINE)
                aKey = OpenKey(aReg, r'SOFTWARE\Microsoft\MSBuild\ToolsVersions\14.0')
                return QueryValueEx(aKey, "MSBuildToolsPath")[0]
            except OSError:
                print('msbuild path not found')
            return ''
        elif vs == "2017" or vs == "2019":
            path = call_process(
                [
                    os.path.join(
                        Config.BUILD_ROOT,
                        'vswhere.exe'
                    ),
                    '-latest',
                    '-requires',
                    'Microsoft.Component.MSBuild',
                    '-property',
                    'installationPath'
                ],
                os.getcwd(),
                useOutput=True
            ).decode("utf-8")

            if vs == "2017":
                path = path.decode("utf-8").replace("2019", "2017")
                path = os.path.join(path, "MSBuild", "15.0", "Bin")
            else:
                path = os.path.join(path, "MSBuild", "Current", "Bin")

            print("get_msbuild_path: " + path)

            return path
        else:
            return ''

    def build(self, silent=False, vs="2019"):
        # call msbuild and compile projects in solution
        subprocess.check_call([
            os.path.join(self.get_msbuild_path(vs), 'MSBuild.exe'),
            os.path.join(self.build_dir(), 'GameAnalytics.sln'),
            '/m',  # parallel builds
            '/t:GameAnalytics',
            '/p:Configuration=Release',
        ])

        subprocess.check_call([
            os.path.join(self.get_msbuild_path(vs), 'MSBuild.exe'),
            os.path.join(self.build_dir(), 'GameAnalytics.sln'),
            '/m',  # parallel builds
            '/t:GameAnalytics',
            '/p:Configuration=Debug',
        ])

        debug_dir = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Debug'))
        release_dir = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Release'))

        # remove folders if there
        LibTools.remove_folder(debug_dir)
        LibTools.remove_folder(release_dir)

        shutil.move(
            os.path.join(self.build_dir(), 'Debug'),
            debug_dir
        )

        shutil.move(
            os.path.join(self.build_dir(), 'Release'),
            release_dir
        )

        if "no-sqlite-src" in self.name:
            self.create_sqlite_project_file()

            sqlite_debug_dir = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Debug'))
            sqlite_release_dir = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Release'))

            subprocess.check_call([
                os.path.join(self.get_msbuild_path(vs), 'MSBuild.exe'),
                os.path.join(self.sqlite_build_dir(), 'Sqlite.sln'),
                '/m',  # parallel builds
                '/t:Sqlite',
                '/p:Configuration=Release',
            ])

            subprocess.check_call([
                os.path.join(self.get_msbuild_path(vs), 'MSBuild.exe'),
                os.path.join(self.sqlite_build_dir(), 'Sqlite.sln'),
                '/m',  # parallel builds
                '/t:Sqlite',
                '/p:Configuration=Debug',
            ])

            LibTools.remove_folder(sqlite_debug_dir)
            LibTools.remove_folder(sqlite_release_dir)

            shutil.move(
                os.path.join(self.sqlite_build_dir(),
                             'Debug'),
                sqlite_debug_dir
            )

            shutil.move(
                os.path.join(self.sqlite_build_dir(),
                             'Release'),
                sqlite_release_dir
            )


class TargetWin10(TargetWin):
    def __init__(self, name, generator, architecture):
        super(TargetWin10, self).__init__(name, generator)
        self.architecture = architecture

    def create_project_file(self, noSqliteSrc="NO"):
        call_process(
            [
                os.path.join(
                    Config.CMAKE_ROOT,
                    'bin',
                    'cmake'
                ),
                '../../../cmake/gameanalytics/',
                '-DPLATFORM:STRING=' + self.name,
                '-DCMAKE_SYSTEM_NAME=WindowsStore',
                '-DCMAKE_SYSTEM_VERSION=10.0',
                '-DNO_SQLITE_SRC:STRING=' + noSqliteSrc,
                '-G',
                self.generator,
                '-A',
                self.architecture
            ],
            self.build_dir()
        )


class TargetTizen(TargetCMake):
    def create_project_file(self, noSqliteSrc="NO"):
        build_folder = os.path.join(Config.BUILD_DIR, self.name)

        if sys.platform == 'darwin':
            tizen_ide = os.path.join(Config.TIZEN_ROOT, "tools", "ide", "bin", "tizen.sh")
        else:
            tizen_ide = os.path.join(Config.TIZEN_ROOT, "tools", "ide", "bin", "tizen.bat")

        tizen_src_dir = os.path.join(build_folder, "src")
        tizen_include_dir = os.path.join(build_folder, "inc")

        if LibTools.folder_exists(build_folder):
            if sys.platform != 'darwin':
                if LibTools.folder_exists(tizen_include_dir):
                    os.rmdir(tizen_include_dir)
                if LibTools.folder_exists(tizen_src_dir):
                    os.rmdir(tizen_src_dir)
            shutil.rmtree(build_folder)

        libType = 'StaticLibrary'
        lib_type = 'staticLib'
        if 'shared' in self.name:
            libType = 'SharedLibrary'
            lib_type = 'sharedLib'

        call_process(
            [
                tizen_ide,
                'create',
                'native-project',
                '-p',
                'mobile-2.4',
                '-t',
                libType,
                '-n',
                self.name,
                '--',
                Config.BUILD_DIR
            ],
            Config.BUILD_DIR
        )

        src_dir = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'source'))
        dependencies_dir = os.path.join(src_dir, "dependencies")
        project_def_tmp = os.path.abspath(os.path.join(__file__, '..', '..', 'tizen', 'project_def_tmp.prop'))
        project_def = os.path.join(build_folder, 'project_def.prop')

        shutil.rmtree(tizen_src_dir)
        shutil.rmtree(tizen_include_dir)

        os.symlink(src_dir, tizen_src_dir)
        os.symlink(dependencies_dir, tizen_include_dir)

        shutil.copy(project_def_tmp, project_def)

        for line in fileinput.input(project_def, inplace=True):
            if '<LIB_TYPE>' in line:
                line = line.replace('<LIB_TYPE>', lib_type)
            if '<ASYNC>' in line:
                if self.generator == 'x86':
                    line = line.replace('<ASYNC>', 'NO_ASYNC')
                else:
                    line = line.replace('<ASYNC>', '')
            sys.stdout.write(line)

        flags_file = os.path.join(build_folder, 'Build', 'flags.mk')

        with open(flags_file, 'r') as file:
            lines = file.readlines()

        for index, line in enumerate(lines):
            if line.startswith('CPP_COMPILE_FLAGS'):
                lines[index] = line.strip() + " -std=c++11\n"

        with open(flags_file, 'w') as file:
            file.writelines(lines)

    def build(self, silent=False):
        build_folder = os.path.join(Config.BUILD_DIR, self.name)
        if sys.platform == 'darwin':
            tizen_ide = os.path.join(Config.TIZEN_ROOT, "tools", "ide", "bin", "tizen.sh")
        else:
            tizen_ide = os.path.join(Config.TIZEN_ROOT, "tools", "ide", "bin", "tizen.bat")

        compiler = 'gcc'

        call_process(
            [
                tizen_ide,
                'build-native',
                '-a',
                self.generator,
                '-c',
                compiler,
                '-C',
                'Release',
                '--',
                build_folder
            ],
            self.build_dir(),
            silent=silent
        )

        call_process(
            [
                tizen_ide,
                'build-native',
                '-a',
                self.generator,
                '-c',
                compiler,
                '-C',
                'Debug',
                '--',
                build_folder
            ],
            self.build_dir(),
            silent=silent
        )

        libEnding = 'a'
        if 'shared' in self.name:
            libEnding = 'so'

        debug_file = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Debug', 'libGameAnalytics.' + libEnding))
        release_file = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Release', 'libGameAnalytics.' + libEnding))

        if not os.path.exists(os.path.dirname(debug_file)):
            os.makedirs(os.path.dirname(debug_file))

        if not os.path.exists(os.path.dirname(release_file)):
            os.makedirs(os.path.dirname(release_file))

        shutil.move(
            os.path.join(self.build_dir(), 'Debug', 'libGameAnalytics.' + libEnding),
            debug_file
        )

        shutil.move(
            os.path.join(self.build_dir(), 'Release', 'libGameAnalytics.' + libEnding),
            release_file
        )


class TargetLinux(TargetCMake):
    def __init__(self, name, generator, architecture, ccompiler, cppcompiler):
        super(TargetLinux, self).__init__(name, generator)
        self.architecture = architecture
        self.ccompiler = ccompiler
        self.cppcompiler = cppcompiler

    def create_project_file(self, noSqliteSrc="NO"):
        print('Skip create_project_file for Linux')

    def build(self, silent=False):
        noSqliteSrc = "NO"
        if "no-sqlite-src" in self.name:
            noSqliteSrc = "YES"

        call_process(
            [
                os.path.join(
                    Config.CMAKE_ROOT,
                    'bin',
                    'cmake'
                ),
                '../../../cmake/gameanalytics/',
                '-DPLATFORM:STRING=' + self.name,
                '-DNO_SQLITE_SRC:STRING=' + noSqliteSrc,
                '-DCMAKE_BUILD_TYPE=RELEASE',
                '-DTARGET_ARCH:STRING=' + self.architecture,
                '-DCMAKE_C_COMPILER=' + self.ccompiler,
                '-DCMAKE_CXX_COMPILER=' + self.cppcompiler,
                '-G',
                self.generator
            ],
            self.build_dir()
        )

        call_process(
            [
                'make',
                'clean'
            ],
            self.build_dir(),
            silent=silent
        )

        call_process(
            [
                'make',
                '-j4'
            ],
            self.build_dir(),
            silent=silent
        )

        call_process(
            [
                os.path.join(
                    Config.CMAKE_ROOT,
                    'bin',
                    'cmake'
                ),
                '../../../cmake/gameanalytics/',
                '-DPLATFORM:STRING=' + self.name,
                '-DNO_SQLITE_SRC:STRING=' + noSqliteSrc,
                '-DCMAKE_BUILD_TYPE=DEBUG',
                '-DTARGET_ARCH:STRING=' + self.architecture,
                '-DCMAKE_C_COMPILER=' + self.ccompiler,
                '-DCMAKE_CXX_COMPILER=' + self.cppcompiler,
                '-G',
                self.generator
            ],
            self.build_dir()
        )

        call_process(
            [
                'make',
                'clean'
            ],
            self.build_dir(),
            silent=silent
        )

        call_process(
            [
                'make',
                '-j4'
            ],
            self.build_dir(),
            silent=silent
        )

        libEnding = 'a'
        if 'shared' in self.name:
            libEnding = 'so'

        debug_file = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Debug', 'libGameAnalytics.' + libEnding))
        release_file = os.path.abspath(os.path.join(__file__, '..', '..', '..', 'export', self.name, 'Release', 'libGameAnalytics.' + libEnding))

        if not os.path.exists(os.path.dirname(debug_file)):
            os.makedirs(os.path.dirname(debug_file))

        if not os.path.exists(os.path.dirname(release_file)):
            os.makedirs(os.path.dirname(release_file))

        shutil.move(
            os.path.join(self.build_dir(), 'Debug', 'libGameAnalytics.' + libEnding),
            debug_file
        )

        shutil.move(
            os.path.join(self.build_dir(), 'Release', 'libGameAnalytics.' + libEnding),
            release_file
        )

        if "no-sqlite-src" in self.name:
            sqlite_debug_file = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Debug', 'libSqlite.' + libEnding))
            sqlite_release_file = os.path.abspath(os.path.join(
                __file__, '..', '..', '..', 'export', 'sqlite', self.name, 'Release', 'libSqlite.' + libEnding))

            call_process(
                [
                    os.path.join(
                        Config.CMAKE_ROOT,
                        'bin',
                        'cmake'
                    ),
                    '../../../../cmake/sqlite/',
                    '-DPLATFORM:STRING=' + self.name,
                    '-DCMAKE_BUILD_TYPE=RELEASE',
                    '-DTARGET_ARCH:STRING=' + self.architecture,
                    '-DCMAKE_C_COMPILER=' + self.ccompiler,
                    '-DCMAKE_CXX_COMPILER=' + self.cppcompiler,
                    '-G',
                    self.generator
                ],
                self.sqlite_build_dir()
            )

            call_process(
                [
                    'make',
                    'clean'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            call_process(
                [
                    'make',
                    '-j4'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            call_process(
                [
                    os.path.join(
                        Config.CMAKE_ROOT,
                        'bin',
                        'cmake'
                    ),
                    '../../../../cmake/sqlite/',
                    '-DPLATFORM:STRING=' + self.name,
                    '-DCMAKE_BUILD_TYPE=DEBUG',
                    '-DTARGET_ARCH:STRING=' + self.architecture,
                    '-DCMAKE_C_COMPILER=' + self.ccompiler,
                    '-DCMAKE_CXX_COMPILER=' + self.cppcompiler,
                    '-G',
                    self.generator
                ],
                self.sqlite_build_dir()
            )

            call_process(
                [
                    'make',
                    'clean'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            call_process(
                [
                    'make',
                    '-j4'
                ],
                self.sqlite_build_dir(),
                silent=silent
            )

            if not os.path.exists(os.path.dirname(sqlite_debug_file)):
                os.makedirs(os.path.dirname(sqlite_debug_file))

            if not os.path.exists(os.path.dirname(sqlite_release_file)):
                os.makedirs(os.path.dirname(sqlite_release_file))

            shutil.move(
                os.path.join(self.sqlite_build_dir(), 'Debug',
                             'libSqlite.' + libEnding),
                sqlite_debug_file
            )

            shutil.move(
                os.path.join(self.sqlite_build_dir(), 'Release',
                             'libSqlite.' + libEnding),
                sqlite_release_file
            )


all_targets = {
    'win32-vc141-static': TargetWin('win32-vc141-static', 'Visual Studio 15 2017'),
    'win32-vc141-mt-static': TargetWin('win32-vc141-mt-static', 'Visual Studio 15 2017'),
    'win32-vc140-static': TargetWin('win32-vc140-static', 'Visual Studio 14 2015'),
    'win32-vc140-static-no-sqlite-src': TargetWin('win32-vc140-static-no-sqlite-src', 'Visual Studio 14 2015'),
    'win32-vc140-mt-static': TargetWin('win32-vc140-mt-static', 'Visual Studio 14 2015'),
    'win32-vc141-shared': TargetWin('win32-vc141-shared', 'Visual Studio 15 2017'),
    'win32-vc140-shared': TargetWin('win32-vc140-shared', 'Visual Studio 14 2015'),
    'win64-vc141-static': TargetWin('win64-vc141-static', 'Visual Studio 15 2017 Win64'),
    'win64-vc141-mt-static': TargetWin('win64-vc141-mt-static', 'Visual Studio 15 2017 Win64'),
    'win64-vc140-static': TargetWin('win64-vc140-static', 'Visual Studio 14 2015 Win64'),
    'win64-vc140-static-no-sqlite-src': TargetWin('win64-vc140-static-no-sqlite-src', 'Visual Studio 14 2015 Win64'),
    'win64-vc140-mt-static': TargetWin('win64-vc140-mt-static', 'Visual Studio 14 2015 Win64'),
    'win64-vc141-shared': TargetWin('win64-vc141-shared', 'Visual Studio 15 2017 Win64'),
    'win64-vc140-shared': TargetWin('win64-vc140-shared', 'Visual Studio 14 2015 Win64'),
    'uwp-x86-vc140-static': TargetWin10('uwp-x86-vc140-static', 'Visual Studio 16 2019', 'Win32'),
    'uwp-x64-vc140-static': TargetWin10('uwp-x64-vc140-static', 'Visual Studio 16 2019', 'x64'),
    'uwp-arm-vc140-static': TargetWin10('uwp-arm-vc140-static', 'Visual Studio 16 2019', 'ARM'),
    'uwp-x86-vc140-shared': TargetWin10('uwp-x86-vc140-shared', 'Visual Studio 16 2019', 'Win32'),
    'uwp-x64-vc140-shared': TargetWin10('uwp-x64-vc140-shared', 'Visual Studio 16 2019', 'x64'),
    'uwp-arm-vc140-shared': TargetWin10('uwp-arm-vc140-shared', 'Visual Studio 16 2019', 'ARM'),
    'osx-static': TargetOSX('osx-static', 'Xcode'),
    'osx-static-no-sqlite-src': TargetOSX('osx-static-no-sqlite-src', 'Xcode'),
    'osx-shared': TargetOSX('osx-shared', 'Xcode'),
    'tizen-arm-static': TargetTizen('tizen-arm-static', 'arm'),
    'tizen-arm-shared': TargetTizen('tizen-arm-shared', 'arm'),
    'tizen-x86-static': TargetTizen('tizen-x86-static', 'x86'),
    'tizen-x86-shared': TargetTizen('tizen-x86-shared', 'x86'),
    #'linux-x86-clang-static': TargetLinux('linux-x86-clang-static', 'Unix Makefiles', '-m32', 'clang', 'clang++'),
    #'linux-x86-gcc-static': TargetLinux('linux-x86-gcc-static', 'Unix Makefiles', '-m32', 'gcc', 'g++'),
    #'linux-x86-clang-shared': TargetLinux('linux-x86-clang-shared', 'Unix Makefiles', '-m32', 'clang', 'clang++'),
    #'linux-x86-gcc-shared': TargetLinux('linux-x86-gcc-shared', 'Unix Makefiles', '-m32', 'gcc', 'g++'),
    'linux-x64-clang-static': TargetLinux('linux-x64-clang-static', 'Unix Makefiles', '-m64', 'clang', 'clang++'),
    'linux-x64-clang-static-no-sqlite-src': TargetLinux('linux-x64-clang-static-no-sqlite-src', 'Unix Makefiles', '-m64', 'clang', 'clang++'),
    'linux-x64-gcc-static': TargetLinux('linux-x64-gcc-static', 'Unix Makefiles', '-m64', 'gcc', 'g++'),
    'linux-x64-gcc5-static': TargetLinux('linux-x64-gcc5-static', 'Unix Makefiles', '-m64', 'gcc', 'g++'),
    'linux-x64-clang-shared': TargetLinux('linux-x64-clang-shared', 'Unix Makefiles', '-m64', 'clang', 'clang++'),
    'linux-x64-gcc-shared': TargetLinux('linux-x64-gcc-shared', 'Unix Makefiles', '-m64', 'gcc', 'g++'),
    'linux-x64-gcc5-shared': TargetLinux('linux-x64-gcc5-shared', 'Unix Makefiles', '-m64', 'gcc', 'g++'),
}

available_targets = {
    'Darwin': {
        'osx-static': all_targets['osx-static'],
        'osx-static-no-sqlite-src': all_targets['osx-static-no-sqlite-src'],
        'osx-shared': all_targets['osx-shared'],
        'tizen-arm-static': all_targets['tizen-arm-static'],
        'tizen-arm-shared': all_targets['tizen-arm-shared'],
        'tizen-x86-static': all_targets['tizen-x86-static'],
        'tizen-x86-shared': all_targets['tizen-x86-shared'],
    },
    'Windows': {
        # 'win32-vc141-static': all_targets['win32-vc141-static'],
        # 'win32-vc141-mt-static': all_targets['win32-vc141-mt-static'],
        'win32-vc140-static': all_targets['win32-vc140-static'],
        'win32-vc140-static-no-sqlite-src': all_targets['win32-vc140-static-no-sqlite-src'],
        'win32-vc140-mt-static': all_targets['win32-vc140-mt-static'],
        # 'win32-vc120-static': all_targets['win32-vc120-static'],
        # 'win32-vc120-mt-static': all_targets['win32-vc120-mt-static'],
        # 'win32-vc141-shared': all_targets['win32-vc141-shared'],
        'win32-vc140-shared': all_targets['win32-vc140-shared'],
        # 'win32-vc120-shared': all_targets['win32-vc120-shared'],
        # 'win64-vc141-static': all_targets['win64-vc141-static'],
        # 'win64-vc141-mt-static': all_targets['win64-vc141-mt-static'],
        'win64-vc140-static': all_targets['win64-vc140-static'],
        'win64-vc140-static-no-sqlite-src': all_targets['win64-vc140-static-no-sqlite-src'],
        'win64-vc140-mt-static': all_targets['win64-vc140-mt-static'],
        # 'win64-vc120-static': all_targets['win64-vc120-static'],
        # 'win64-vc120-mt-static': all_targets['win64-vc120-mt-static'],
        # 'win64-vc141-shared': all_targets['win64-vc141-shared'],
        'win64-vc140-shared': all_targets['win64-vc140-shared'],
        # 'win64-vc120-shared': all_targets['win64-vc120-shared'],
        'uwp-x86-vc140-static': all_targets['uwp-x86-vc140-static'],
        'uwp-x64-vc140-static': all_targets['uwp-x64-vc140-static'],
        'uwp-arm-vc140-static': all_targets['uwp-arm-vc140-static'],
        'uwp-x86-vc140-shared': all_targets['uwp-x86-vc140-shared'],
        'uwp-x64-vc140-shared': all_targets['uwp-x64-vc140-shared'],
        'uwp-arm-vc140-shared': all_targets['uwp-arm-vc140-shared'],
        'tizen-arm-static': all_targets['tizen-arm-static'],
        'tizen-arm-shared': all_targets['tizen-arm-shared'],
        'tizen-x86-static': all_targets['tizen-x86-static'],
        'tizen-x86-shared': all_targets['tizen-x86-shared'],
    },
    'Linux': {
        #'linux-x86-clang-static': all_targets['linux-x86-clang-static'],
        #'linux-x86-gcc-static': all_targets['linux-x86-gcc-static'],
        #'linux-x86-clang-shared': all_targets['linux-x86-clang-shared'],
        #'linux-x86-gcc-shared': all_targets['linux-x86-gcc-shared'],
        'linux-x64-clang-static': all_targets['linux-x64-clang-static'],
        'linux-x64-clang-static-no-sqlite-src': all_targets['linux-x64-clang-static-no-sqlite-src'],
        'linux-x64-gcc-static': all_targets['linux-x64-gcc-static'],
        # 'linux-x64-gcc5-static': all_targets['linux-x64-gcc5-static'],
        'linux-x64-clang-shared': all_targets['linux-x64-clang-shared'],
        'linux-x64-gcc-shared': all_targets['linux-x64-gcc-shared'],
        # 'linux-x64-gcc5-shared': all_targets['linux-x64-gcc5-shared'],
    }
}[platform.system()]

valid_visual_studio = [
    '2017',
    '2019'
]

# Sorted since we want android-wrapper to be built after android-shared (due to jni generation)
valid_target_names = sorted(available_targets.keys())


def print_help():
    print('build.py <list_of_targets>')
    print('valid targets:')
    for target in valid_target_names:
        print('  ' + target)
    print('valid visual studios:')
    for vs in valid_visual_studio:
        print('  ' + vs)


def build(target_name, vs, silent=False):
    if target_name not in available_targets:
        print('target %(target_name)s not supported on this platform' % locals())
        sys.exit(1)

    if vs not in valid_visual_studio:
        print('visual studio %(vs)s not supported')
        sys.exit(1)

    if platform.system() == 'Windows':
        if (vs != "2017" or vs != "2019") and 'vc141' in target_name:
            print('can only build vc141 target on visual studio 2017 or 2019')
            sys.exit(1)

    target = available_targets[target_name]
    noSqliteSrc = "NO"
    if "no-sqlite-src" in target_name:
        noSqliteSrc = "YES"

    target.create_project_file(noSqliteSrc=noSqliteSrc)

    if platform.system() == 'Windows':
        if 'tizen' in target_name:
            target.build(silent=silent)
        else:
            target.build(silent=silent, vs=vs)
    else:
        target.build(silent=silent)


def build_targets(target_names, silent=False, vs="2019", skip_tizen=False, no_sqlite_src=False):

    for target_name in target_names:
        if skip_tizen and 'tizen' in target_name:
            continue
        if skip_tizen and 'clang' in target_name:
            continue
        if no_sqlite_src and 'no-sqlite-src' not in target_name:
            continue
        print("")
        print("-----------------------------------------")
        print("")
        print("    BUILDING TARGET: " + target_name)
        print("")
        print("-----------------------------------------")
        print("")

        build(target_name, vs, silent=silent)


# @timing - benchmarking build
def main(argv, silent=False):
    print("-----------------------------------------")
    print("    BUILDING")
    print("-----------------------------------------")
    print("Build arguments...")
    print(argv)

    try:
        opts, args = getopt.getopt(
            argv, "t:v:nqh", ["target=", "vs=", "notizen",  "nosqlitesrc", "help"])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)

    build_target_name = None
    visual_studio = "2019"
    skip_tizen = False
    no_sqlite_src = False

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            print_help()
            sys.exit()
        elif opt in ('-t', '--target'):
            if arg in valid_target_names:
                build_target_name = arg
            else:
                print("Target: " + arg + " is not part of allowed targets.")
                print_help()
                sys.exit(2)
        elif opt in ('-n', '--notizen'):
            skip_tizen = True
        elif opt in ('-q', '--nosqlitesrc'):
            no_sqlite_src = True
        elif opt in ('-v', '--vs'):
            if arg in valid_visual_studio:
                visual_studio = arg
            else:
                print("Visual Studio version: " + arg + " is not part of allowed visual studio versions.")
                print_help()
                sys.exit(2)

    if build_target_name:
        build_targets([build_target_name], silent=silent, vs=visual_studio,
                      skip_tizen=skip_tizen, no_sqlite_src=no_sqlite_src)
    else:
        # build all
        build_targets(valid_target_names, silent=silent, vs=visual_studio,
                      skip_tizen=skip_tizen, no_sqlite_src=no_sqlite_src)


if __name__ == '__main__':
    main(sys.argv[1:])
