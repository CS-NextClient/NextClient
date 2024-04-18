#!/usr/bin/python

import os
import sys
import urllib
import urllib.request
import config
import zipfile
import tarfile
import shutil
import stat
import subprocess
import time

from sys import platform


def is_os_64bit():
    return 'PROGRAMFILES(X86)' in os.environ


if platform == 'win32':  # win32 and/or win64
    CMAKE_URL = 'https://github.com/Kitware/CMake/releases/download/v3.19.6/cmake-3.19.6-win64-x64.zip'
    if is_os_64bit():
        TIZEN_URL = 'http://download.tizen.org/sdk/Installer/tizen-sdk-2.4-rev8/tizen-web-cli_TizenSDK_2.4.0_Rev8_windows-64.exe'
    else:
        TIZEN_URL = 'http://download.tizen.org/sdk/Installer/tizen-sdk-2.4-rev8/tizen-web-cli_TizenSDK_2.4.0_Rev8_windows-32.exe'
elif platform == 'darwin':  # OSX
    CMAKE_URL = 'https://github.com/Kitware/CMake/releases/download/v3.18.4/cmake-3.18.4-Darwin-x86_64.tar.gz'
    TIZEN_URL = 'https://download.tizen.org/sdk/Installer/tizen-studio_4.1.1/web-cli_Tizen_Studio_4.1.1_macos-64.bin'
elif platform in ('linux', 'linux2'):
    CMAKE_URL = 'http://www.cmake.org/files/v3.2/cmake-3.2.2-Linux-x86_64.tar.gz'
else:
    raise NotImplementedError('platform %s is currently not supported' % platform)

cmake_package = os.path.join(config.BUILD_ROOT, CMAKE_URL.split('/')[-1])
if not (platform in ('linux', 'linux2')):
    tizen_package = os.path.join(config.BUILD_ROOT, TIZEN_URL.split('/')[-1])
profile_tmp_file = os.path.abspath(os.path.join(config.BUILD_ROOT, '..', 'tizen', 'profiles_tmp.xml'))
profile_file = os.path.abspath(os.path.join(config.BUILD_ROOT, '..', 'tizen', 'profiles.xml'))


def call_process(process_arguments, silent=False, shell=False):
    print('Call process ' + str(process_arguments))

    if silent is True:
        subprocess.check_call(process_arguments, stdout=open(os.devnull, 'wb'), shell=shell)
    else:
        subprocess.check_call(process_arguments, shell=shell)


def download(url, destination, silent=False):
    def reporthook(count, block_size, total_size):
        sys.stdout.write('{3}downloaded {0} bytes of {1} bytes ({2:.1f}%)               '.format(
            count*block_size,
            total_size,
            100.0 * count * block_size / total_size,
            '\r' * 80
        ))

    if silent is True:
        urllib.request.urlretrieve(url, destination)
    else:
        urllib.request.urlretrieve(url, destination, reporthook)
    print('download done')

# windows has an restriction about the size of a filename (260)
# the android sdk contains some eclipse files, which are very long
# and cause a extraction error during a simple extraction
# therefore I wrote this function, which extracts the files if possible
# def unzip(zip_file_location, extract_location):
#   max_path_size = 240
#   skipped_files = 0
#   with zipfile.ZipFile(zip_file_location, 'r') as zf:
#       root_in_zip = zf.namelist()[0].split('/')[0]
#
#       print(root_in_zip)
#       sys.exit(1)
#
#       for archive_member in zf.infolist():
#           dst_path = os.path.join(extract_location, archive_member.filename)
#           if len(dst_path) > max_path_size:
#               skipped_files += 1
#               print('skipping: ' + archive_member.filename)
#           else:
#               print('extract: ' + archive_member.filename)
#               zf.extract(archive_member, extract_location)
#       shutil.move( root_in_zip, extract_location)
#
#   print('skipped files total: ' + str(skipped_files))


def unzip(package, dst, silent=False):
    print('unzipping "%s"' % package)
    with zipfile.ZipFile(package, 'r') as zf:
        root_in_zip = zf.namelist()[0].split('/')[0]
        zf.extractall()
        shutil.move(root_in_zip, dst)
    print('[done]')


def untar(package, dst, silent=False):
    print('untarring "%s"' % package)
    # with tarfile.TarFile(package, 'r') as zf:
    with tarfile.open(package, 'r') as zf:
        root_in_zip = zf.getnames()[0].split('/')[0]
        zf.extractall()
        shutil.move(root_in_zip, dst)
    print('[done]')


def uncompress(package, dst, silent=False):
    if '.tar' in package:
        untar(package, dst, silent=silent)
    else:
        unzip(package, dst, silent=silent)


def install_cmake(silent=False):
    if not os.path.exists(config.CMAKE_ROOT):
        print("-------------- CMAKE ---------------")
        if not os.path.exists(cmake_package):
            print("--> DOWNLOADING CMAKE")
            download(CMAKE_URL, cmake_package, silent=silent)
        print("--> UNCOMPRESSING CMAKE")
        uncompress(cmake_package, config.CMAKE_ROOT, silent=silent)

        if platform == 'darwin':
            print("--> MOVING CMAKE + CLEANUP")
            shutil.move(
                os.path.join(config.CMAKE_ROOT, 'CMake.app', 'Contents'),
                config.CMAKE_ROOT + '_'
            )
            shutil.rmtree(config.CMAKE_ROOT)
            shutil.move(config.CMAKE_ROOT + '_', config.CMAKE_ROOT)

        os.unlink(cmake_package)


def install_tizen(silent=False):
    if (platform in ('linux', 'linux2')):
        print("Don't install tizen on Linux")
        return
    if not os.path.exists(config.TIZEN_ROOT):
        print("-------------- TIZEN ---------------")
        if not os.path.exists(tizen_package):
            print("--> DOWNLOADING TIZEN")
            download(TIZEN_URL, tizen_package, silent=silent)

        with open(profile_tmp_file) as infile, open(profile_file, 'w') as outfile:
            for line in infile:
                line = line.replace('<SDK_PATH>', config.TIZEN_ROOT)
                outfile.write(line)

        if platform == 'darwin':
            st = os.stat(tizen_package)
            os.chmod(tizen_package, st.st_mode | stat.S_IEXEC)

            call_process(
                [
                    tizen_package,
                    '--accept-license',
                    config.TIZEN_ROOT
                ],
                silent=silent
            )

            os.unlink(tizen_package)

            update_manager = os.path.join(
                config.TIZEN_ROOT, "package-manager", "package-manager-cli.bin")
            tizen_ide = os.path.join(config.TIZEN_ROOT, "tools", "ide", "bin", "tizen")

            st = os.stat(update_manager)
            os.chmod(update_manager, st.st_mode | stat.S_IEXEC)

            st = os.stat(tizen_ide)
            os.chmod(tizen_ide, st.st_mode | stat.S_IEXEC)

            call_process(
                [
                    update_manager,
                    'install',
                    '--accept-license',
                    'MOBILE-2.4-NativeAppDevelopment-CLI'
                ],
                silent=silent
            )

            call_process(
                [
                    tizen_ide,
                    'cli-config',
                    '-g',
                    '"default.profiles.path=' + profile_file + '"'
                ],
                silent=silent
            )

            call_process(
                [
                    tizen_ide,
                    'cli-config',
                    '-g',
                    'default.build.compiler=gcc'
                ],
                silent=silent
            )
        else:
            call_process(
                [
                    tizen_package,
                    '--accept-license',
                    config.TIZEN_ROOT
                ],
                shell=True
            )

            os.unlink(tizen_package)

            update_manager = os.path.join(config.TIZEN_ROOT, "update-manager", "update-manager-cli.exe")
            tizen_ide = os.path.join(config.TIZEN_ROOT, "tools", "ide", "bin", "tizen.bat")

            while not os.path.exists(update_manager):
                time.sleep(1)

            call_process(
                [
                    update_manager,
                    'install',
                    '--accept-license',
                    'MOBILE-2.4-NativeAppDevelopment-CLI'
                ],
                shell=True
            )

            time.sleep(15)

            temp_dir = os.path.join(config.TIZEN_ROOT, "temp")

            while os.path.exists(temp_dir):
                time.sleep(1)

            call_process(
                [
                    tizen_ide,
                    'cli-config',
                    '-g',
                    '"default.profiles.path=' + profile_file + '"'
                ]
            )

            call_process(
                [
                    tizen_ide,
                    'cli-config',
                    '-g',
                    'default.build.compiler=gcc'
                ]
            )


def install_dependencies(installTizen=False, silent=False):
    if silent is True:
        print("SILENT DEPENDENCY INSTALL")
    install_cmake(silent=silent)
    if installTizen is True:
        if not (platform in ('linux', 'linux2')):
            install_tizen(silent=silent)


if __name__ == '__main__':
    os.chdir(config.BUILD_ROOT)
    install_dependencies()
