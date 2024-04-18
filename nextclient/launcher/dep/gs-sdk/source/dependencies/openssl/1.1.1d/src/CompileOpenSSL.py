import os
from subprocess import call
import sys
import re

vs_version = "140"
compile_flags = "-no-asm -no-shared -no-capieng"
debug = False

openssl_32_flag = "VC-WIN32"
openssl_64_flag = "VC-WIN64A"

src_32_suffix = "_" + "vs" + vs_version + "_32"
src_64_suffix = "_" + "vs" + vs_version + "_64"

vs_tools_env_var = "VS" + vs_version + "COMNTOOLS"

if len(sys.argv) < 2:
    print("First argument must be the tar.gz file of OpenSSL source")
    exit(1)

if len(sys.argv) < 3:
    print("Second argument must be 32 or 64")
    exit(1)

filename = sys.argv[1]
dirname  = filename.replace(".tar.gz","")
working_dir = os.getcwd()
arch = sys.argv[2]

if arch != "32" and arch != "64":
    print("Second argument must be 32 or 64")
    exit(1)

if len(sys.argv) >= 4:
    debug = sys.argv[3] == "debug"

if debug is True:
    openssl_32_flag = "debug-VC-WIN32"
    openssl_64_flag = "debug-VC-WIN64A"

if not bool(re.match("(openssl-){1}(\d)+(.)(\d)+(.)(\d)+(\w)+(.tar.gz)",filename)):
    print("The file given doesn't seem to be an openssl source file. It must be in the form: openssl-x.y.zw.tar.gz")
    exit(1)


call("7z x " + filename) #extract the .gz file

dirname_src_32 = dirname + src_32_suffix
dirname_src_64 = dirname + src_64_suffix
dirname_bin_32 = dirname + src_32_suffix + "_build"
dirname_bin_64 = dirname + src_64_suffix + "_build"

openssl_tar_file = filename[0:-3]

if arch == "32":
#extract tar file for 32
    call("7z x " + openssl_tar_file)
    os.rename(dirname, dirname_src_32)

#Compile 32
    os.chdir(dirname_src_32)

    call("perl Configure " + openssl_32_flag + " --prefix=" + os.path.join(working_dir,dirname_bin_32) + " " + compile_flags,shell=True)
    call(r"ms\do_ms.bat",shell=True)
    call(r"nmake -f ms\nt.mak",shell=True)
    call(r"nmake -f ms\nt.mak instalL",shell=True)

    print("32-bit compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

if arch == "64":
#extract for 64
    call("7z x " + openssl_tar_file)
    os.rename(dirname, dirname_src_64)

#Compile 64
    os.chdir(dirname_src_64)

    call("perl Configure " + openssl_64_flag + " --prefix=" + os.path.join(working_dir,dirname_bin_64) + " " + compile_flags,shell=True)
    call(r"ms\do_ms.bat",shell=True)
    call(r"nmake -f ms\nt.mak",shell=True)
    call(r"nmake -f ms\nt.mak instalL",shell=True)

    print("64-bit compilation complete.")

#Go back to base dir
os.chdir(working_dir)
################

#os.remove(openssl_tar_file)
