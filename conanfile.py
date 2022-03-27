from conans import ConanFile, CMake
from six import StringIO
import inspect

class EmulatorConan(ConanFile):
    name = "cpp-can-parser"
    settings = "os", "compiler", "build_type", "arch"
    generators = ["cmake", "cmake_paths", "cmake_find_package", "virtualenv"]
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        pass

    def set_version(self):
        version_io = StringIO()
        self.run("git tag | tail -n 1", output=version_io)

        version_tag = version_io.getvalue()
        version_major = version_io.getvalue()[1:2]
        version_minor = version_io.getvalue()[3:4]
        version_patch = version_io.getvalue()[5:6]

        version_tag_further_io = StringIO()
        self.run(f"git rev-list --count `git rev-list -n 1 {version_tag}`", output=version_tag_further_io)

        last_commit_further_io = StringIO()
        self.run(f"git rev-list --count `git log --pretty=format:%h -n 1`", output=last_commit_further_io)

        version_tweak = int(last_commit_further_io.getvalue()) - int(version_tag_further_io.getvalue())

        dirty_status_io = StringIO()
        self.run(f"git status --porcelain", output=dirty_status_io)

        if dirty_status_io.getvalue() == "":
            version_tweak_dirty = 0
        else:
            version_tweak_dirty = 1

        with open("version.cmake", mode="w") as file:
            file.write(f"set(VERSION_MAJOR {version_major})\n")
            file.write(f"set(VERSION_MINOR {version_minor})\n")
            file.write(f"set(VERSION_PATCH {version_patch})\n")
            file.write(f"set(VERSION_TWEAK {version_tweak})\n")
            file.write(f"set(VERSION_TWEAK_DIRTY {version_tweak_dirty})\n")

        self.version = version_tag[1:]

    def imports(self):
        self.copy("*.so*", dst="./lib", src="lib")

    def config_options(self):
        if self.settings.os == "Linux":
            del self.options.fPIC

    def export_sources(self):
        self.copy("*")
        self.copy("*.cmake", dst="", keep_path=False)

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder=self.source_folder)
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        self.copy("lib/*", dst="lib", keep_path=False, symlinks=True)
        # self.copy("*.h", dst="include", src="vtrix")
        # self.copy("*vtrix.lib", dst="lib", keep_path=False)
        # self.copy("*.dll", dst="bin", keep_path=False)
        # self.copy("*.so", dst="lib", keep_path=False)
        # self.copy("*.dylib", dst="lib", keep_path=False)
        # self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["cpp-can-parser"]

