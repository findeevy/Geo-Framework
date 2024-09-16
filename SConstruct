#!/usr/bin/env python
import os
import sys
from SCons.Script import *

env = SConscript("godot-cpp/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["wms_server/"])
env.Append(CCFLAGS=["-fexceptions"])
env.Append(CXXFLAGS=["-std=c++20"])
env.Append(CPPFLAGS=["-fexceptions"])
env.Append(LIBPATH=["godot_project/bin/"])
env.Append(LIBS=["libsockpp", "libjsoncpp", "libtinycbor"])

sources = ["wms_server/godot_bindings.cpp", "wms_server/cbor.cpp", "wms_server/socket.cpp"]

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "godot_project/bin/gdwmsclient.{}.{}.framework/gdwmsclient.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "godot_project/bin/gdwmsclient{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
