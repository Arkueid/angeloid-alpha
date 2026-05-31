import os
import platform
import re
import subprocess
import sys

from setuptools import setup, find_packages, Extension, Command
from setuptools.command.build_ext import build_ext
from setuptools.command.bdist_wheel import bdist_wheel
from setuptools.command.install import install

# Read version from package (single source of truth)
with open(os.path.join(os.path.dirname(__file__), "package", "angeloid", "__init__.py"), encoding="utf-8") as _f:
    for _line in _f:
        if _line.startswith("__version__"):
            VERSION = _line.split('"')[1]
            break

NAME = "angeloid"
DESCRIPTION = "MMD PMX Viewer library with Python bindings"
LONG_DESCRIPTION = (
    open("README.md", "r", encoding="utf-8")
    .read()
    .replace(
        "./", "https://raw.githubusercontent.com/Arkueid/angeloid-alpha/refs/heads/master/"
    )
)
AUTHOR = "Arkueid"
AUTHOR_EMAIL = "thetardis@qq.com"
URL = "https://github.com/Arkueid/angeloid-alpha"
REQUIRES_PYTHON = ">=3.10"
INSTALL_REQUIRES = []


cmake_built = False


def is_virtualenv():
    return "VIRTUAL_ENV" in os.environ


def get_base_python_path(venv_path):
    return re.search(
        "home = (.*)\n", open(os.path.join(venv_path, "pyvenv.cfg"), "r").read()
    ).group(1)


def run_cmake():
    global cmake_built
    if cmake_built:
        return

    cmake_args = ["-DBUILD_PYTHON_WRAPPER=ON", "-DBUILD_VIEWER=OFF", "-DENABLE_STACKTRACE=OFF"]
    build_args = ["--config", "Release"]

    if platform.system() == "Windows":
        if platform.architecture()[0] == "64bit":
            cmake_args += ["-A", "x64"]
        else:
            cmake_args += ["-A", "Win32"]
        build_args += ["--", "/m:2"]
    else:
        cmake_args += ["-DCMAKE_BUILD_TYPE=Release"]
        build_args += ["--", "-j2"]

    build_folder = os.path.join(os.getcwd(), "build")

    if not os.path.exists(build_folder):
        os.makedirs(build_folder)
    else:
        # Remove stale CMakeCache to avoid platform mismatch errors
        cache_file = os.path.join(build_folder, "CMakeCache.txt")
        if os.path.exists(cache_file):
            os.remove(cache_file)

    if is_virtualenv():
        python_installation_path = get_base_python_path(os.environ["VIRTUAL_ENV"])
    else:
        python_installation_path = os.path.split(sys.executable)[0]
    print("Python installation path: " + python_installation_path)
    sys.stdout.flush()

    cmake_args += ["-DPYTHON_INSTALLATION_PATH=" + python_installation_path]

    cmake_setup = ["cmake", ".."] + cmake_args
    cmake_build = ["cmake", "--build", "."] + build_args

    print("Building extension for Python {}".format(sys.version.split("\n", 1)[0]))
    print("Invoking CMake setup: '{}'".format(" ".join(cmake_setup)))
    sys.stdout.flush()
    subprocess.check_call(cmake_setup, cwd=build_folder)
    print("Invoking CMake build: '{}'".format(" ".join(cmake_build)))
    sys.stdout.flush()
    subprocess.check_call(cmake_build, cwd=build_folder)

    cmake_built = True


class FakeExtension(Extension):

    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[], py_limited_api=True)
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):

    def run(self):
        run_cmake()


class BuildWheel(bdist_wheel):
    def run(self):
        run_cmake()
        bdist_wheel.run(self)


class Install(install):
    def run(self):
        run_cmake()
        install.run(self)


setup(
    name=NAME,
    version=VERSION,
    description=DESCRIPTION,
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    author=AUTHOR,
    author_email=AUTHOR_EMAIL,
    license="MIT",
    url=URL,
    install_requires=INSTALL_REQUIRES,
    ext_modules=[FakeExtension("_angeloid", ".")],
    cmdclass={"build_ext": CMakeBuild, "bdist_wheel": BuildWheel, "install": Install},
    packages=find_packages(where="package"),
    package_data={"": ["**/*.pyd", "**/*.so", "**/*.pyi", "**/*.py"]},
    package_dir={"": "package"},
    keywords=["MMD", "PMX", "PMD", "VMD", "MikuMikuDance"],
    python_requires=REQUIRES_PYTHON,
)
