from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake
from conan.tools.files import copy
import os


class DisklessWorkstationConan(ConanFile):
    name = "diskless_workstation"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    description = "Diskless Workstation project"
    author = "zzj_484133578@163.com"

    def requirements(self):
        self.requires("boost/1.81.0")
        self.requires("libtorrent/2.0.10")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def configure(self):
        # 配置 boost 选项
        self.options["boost"].shared = False
        # 配置 libtorrent 选项
        self.options["libtorrent"].shared = False

