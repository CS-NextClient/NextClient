import os

BUILD_ROOT = os.path.abspath(os.path.join(__file__, '..'))
DEPENDENCIES_ROOT = os.path.join(BUILD_ROOT, "dependencies")
CMAKE_ROOT = os.path.join(DEPENDENCIES_ROOT, "cmake")
TIZEN_ROOT = os.path.join(DEPENDENCIES_ROOT, "tizen")
BUILD_DIR = os.path.join(BUILD_ROOT, "build")
PACKAGING_OUTPUT_DIRECTORY = os.path.join(BUILD_ROOT, "dist")
