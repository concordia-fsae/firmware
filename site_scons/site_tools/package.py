from os import walk
from SCons.Script import Action, Builder, File, Dir, Flatten, Glob

globs = ["*.hex", "*.bin", "*.elf", "*.dbc"]

def recursive_glob(dir, glob):
    ret = []
    for root, dirnames, _ in walk(dir.abspath):
        ret.extend([Glob(f"{dir}/{glob}") for dir in [root] + dirnames])
    return Flatten(ret)


def package(env, output_tar: File):
    files = []
    for glob in globs:
        for file in recursive_glob(env["PLATFORM_ARTIFACTS"], glob):
            files.append(file)
    
    env._package(output_tar=output_tar, file=files)
    return env["PLATFORM_ARTIFACTS"].File(f"{env['PLATFORM_ID']}-package.tgz")


def emit(target, source, env):
    for glob in globs:
        for file in recursive_glob(env["PLATFORM_ARTIFACTS"], glob):
            source.append(file)

    target.append(env["PLATFORM_ARTIFACTS"].File(f"{env['PLATFORM_ID']}-package.tgz"))
    return (target, source)


def generate(env):
    env.Replace(
        PACKAGECOM="tar -cvzf $output_tar $file",
        PACKAGECOMSTR=f"Packaging {env['PLATFORM_ID']} into $output_tar",
    )

    env["BUILDERS"]["_package"] = Builder(
        action=Action(
            "$PACKAGECOM",
            cmdstr="$PACKAGECOMSTR",
        ),
        emitter=emit,
    )

    env.AddMethod(package, "Package")


def exists():
    return True
