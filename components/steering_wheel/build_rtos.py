Import ("env")

from os.path import basename, join, splitext

rtos_src_path = join(env.subst("$PROJECT_DIR"), "RTOS")
rtos_build_path = join(env.subst("$BUILD_DIR"), "RTOS")

ccflags = [
    "-ffunction-sections",
    "-fdata-sections",
    "-Wall",
    "-mthumb",
    "-mcpu=cortex-m3",
    "-nostdlib",
    "-Og",
    "-g2",
    "-ggdb2",
]

freertos_lib_src_files = [
    join("Source", "croutine.c"),
    join("Source", "event_groups.c"),
    join("Source", "list.c"),
    join("Source", "queue.c"),
    join("Source", "stream_buffer.c"),
    join("Source", "tasks.c"),
    join("Source", "timers.c"),
    join("Source", "portable", "MemMang", "heap_4.c"),
    join("Source", "portable", "GCC", "ARM_CM3", "port.c"),
    join("CMSIS", "RTOS2", "FreeRTOS", "Source", "cmsis_os1.c"),
    join("CMSIS", "RTOS2", "FreeRTOS", "Source", "cmsis_os2.c"),
    join("CMSIS", "RTOS2", "FreeRTOS", "Source", "freertos_evr.c"),
    join("CMSIS", "RTOS2", "FreeRTOS", "Source", "handlers.c"),
    join("CMSIS", "RTOS2", "FreeRTOS", "Source", "os_systick.c"),
]

freertos_proj_src_files = [
    join(rtos_src_path, "FreeRTOSResources.c"),
]

def get_filename_no_ext(path):
    return splitext(basename(path))[0]

for src in freertos_lib_src_files:
    obj = env.Object(
        target=join(rtos_build_path, "FreeRTOS", get_filename_no_ext(src)),
        source=join(rtos_src_path, "FreeRTOS", src),
        CCFLAGS=ccflags + ["-Wno-missing-prototypes", "-Wno-cast-align"]
    )

    DefaultEnvironment().Append(PIOBUILDFILES=[obj])

for src in freertos_proj_src_files:
    obj = env.Object(
        target=join(rtos_build_path, get_filename_no_ext(src)),
        source=join(rtos_src_path, src),
        CCFLAGS=ccflags
    )
    DefaultEnvironment().Append(PIOBUILDFILES=[obj])

env.Append(CPPPATH=[rtos_build_path, join(rtos_build_path, "FreeRTOS")])

print("RTOS build script executed")
