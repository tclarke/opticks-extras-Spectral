import os
import os.path
import SCons.Warnings

class SignatureWindowNotFound(SCons.Warnings.Warning):
    pass
SCons.Warnings.enableWarningClass(SignatureWindowNotFound)

def generate(env):
    path = "$SPECTRALCODEDIR/SignatureWindow"
    if not path:
       SCons.Warnings.warn(SignatureWindowNotFound,"Could not locate SignatureWindow header files")
    else:
       env.AppendUnique(CXXFLAGS="-I%s" % path)

def exists(env):
    return env.Detect('SignatureWindow')
