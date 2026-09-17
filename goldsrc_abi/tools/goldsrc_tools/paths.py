import functools
import os


@functools.lru_cache(maxsize=None)
def canonical(path):
    """Absolute path with the on-disk letter case, so one file always yields one spelling."""
    return os.path.realpath(os.path.abspath(path))


def key(path):
    return os.path.normcase(canonical(path))


def location_key(location, root):
    """'relative/file:line:column' identity of a (file, line, column) location."""
    return "%s:%d:%d" % (relative(location[0], root), location[1], location[2])


def relative(path, root):
    """Path relative to root with forward slashes; a path on another drive stays absolute."""
    try:
        return os.path.relpath(canonical(path), canonical(root)).replace("\\", "/")
    except ValueError:
        return canonical(path).replace("\\", "/")
