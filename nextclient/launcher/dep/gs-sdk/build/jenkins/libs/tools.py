import os
import shutil


def rel_to_abs(*argv):
    """
        convert the path array given in argv to an absolute path relative to the directory, where this script is located.
        e.g.: if this script is located at /foo/bar/script.py, then rel_to_abs('..', '42') => /foo/42
    """
    return os.path.abspath(os.path.join(__file__, '..', '..', *argv))


def remove_folder(location):
    if os.path.exists(location):
        shutil.rmtree(location)


def create_folder(location):
    if not os.path.exists(location):
        os.makedirs(os.path.join(location))


def copy_folder(src, dst):
    shutil.copytree(src, dst)


def folder_exists(location):
    return os.path.exists(location)
