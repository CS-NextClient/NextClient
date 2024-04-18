#!/usr/bin/python
import os
import subprocess
import shutil
from os.path import expanduser

BUILD_ROOT = os.path.abspath(os.path.join(__file__, '..'))
CMAKE_BIN_FILE = os.path.abspath(os.path.join(__file__, '..', '..', 'build', 'jenkins', 'dependencies', 'cmake', 'bin', 'cmake'))
BUILD_DIR = os.path.join(BUILD_ROOT, "build")
GA_DIR = os.path.join(expanduser("~"), "GameAnalytics")


def make_test_build_dir():
    try:
        os.makedirs(BUILD_DIR)
    except OSError:
        pass


def remove_gameanalytics_dir():
    if os.path.exists(GA_DIR):
        shutil.rmtree(GA_DIR)


def remove_test_build_dir():
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)


def change_to_build_dir():
    os.chdir(BUILD_DIR)


def run_cmake_tests():
    cmake_call = CMAKE_BIN_FILE + ' -DPLATFORM:STRING=osx-static -DNO_SQLITE_SRC:STRING=NO -G Xcode ..'
    subprocess.call(cmake_call, shell=True)


def compile_and_run_tests():
    run_tests_call = 'xcodebuild -project GameAnalyticsTests.xcodeproj -target GameAnalyticsTests -configuration Debug && ./Debug/GameAnalyticsTests'
    subprocess.call(run_tests_call, shell=True)


def main():
    remove_gameanalytics_dir()
    remove_test_build_dir()
    make_test_build_dir()
    change_to_build_dir()
    run_cmake_tests()
    compile_and_run_tests()


if __name__ == '__main__':
    main()
