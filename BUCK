load("//drive-stack/conUDS/defs.bzl", "conUDS_batch")

export_file(
    name = ".git",
    src = ".git",
    visibility = ["//tools/build-info/..."],
)

filegroup(
    name = "cfr25-embedded",
    srcs = [
        "//components/bms_boss:crc-cfr25",
        "//components/bms_worker:crc-cfr25.all_nodes",
        "//components/bootloaders/STM/stm32f1:0-bin-crc",
        "//components/bootloaders/STM/stm32f1:1-bin-crc",
        "//components/bootloaders/STM/stm32f1:2-bin-crc",
        "//components/bootloaders/STM/stm32f1:3-bin-crc",
        "//components/bootloaders/STM/stm32f1:4-bin-crc",
        "//components/bootloaders/STM/stm32f1:5-bin-crc",
        "//components/bootloaders/STM/stm32f1:11-bin-crc",
        "//components/bootloaders/STM/stm32f1:30-bin-crc",
        "//components/bootloaders/STM/stm32f1:31-bin-crc",
        "//components/bootloaders/STM/stm32f1:32-bin-crc",
        "//components/bootloaders/STM/stm32f1:0-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:1-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:2-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:3-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:4-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:5-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:11-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:30-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:31-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:32-updater-bin-crc",
        "//components/sws:crc-cfr25",
        "//components/vc/front:crc-cfr25",
        "//components/vc/pdu:crc-cfr25",
        "//components/vc/rear:crc-cfr25",
    ],
)

filegroup(
    name = "cfr26-embedded",
    srcs = [
        "//components/bms_boss:crc-cfr26",
        "//components/bms_worker:crc-cfr26.all_nodes",
        "//components/bootloaders/STM/stm32f1:11-bin-crc",
        "//components/bootloaders/STM/stm32f1:30-bin-crc",
        "//components/bootloaders/STM/stm32f1:31-bin-crc",
        "//components/bootloaders/STM/stm32f1:32-bin-crc",
        "//components/bootloaders/STM/stm32f1:40-bin-crc",
        "//components/bootloaders/STM/stm32f1:41-bin-crc",
        "//components/bootloaders/STM/stm32f1:42-bin-crc",
        "//components/bootloaders/STM/stm32f1:43-bin-crc",
        "//components/bootloaders/STM/stm32f1:44-bin-crc",
        "//components/bootloaders/STM/stm32f1:45-bin-crc",
        "//components/bootloaders/STM/stm32f1:46-bin-crc",
        "//components/bootloaders/STM/stm32f1:47-bin-crc",
        "//components/bootloaders/STM/stm32f1:11-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:30-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:31-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:32-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:40-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:41-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:42-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:43-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:44-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:45-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:46-updater-bin-crc",
        "//components/bootloaders/STM/stm32f1:47-updater-bin-crc",
        "//components/sws:crc-cfr26",
        "//components/vc/front:crc-cfr26",
        "//components/vc/pdu:crc-cfr26",
        "//components/vc/rear:crc-cfr26",
    ],
)

conUDS_batch(
    name = "cfr25-download",
    srcs = [
        "//components/bms_boss:deploy-cfr25",
        "//components/bms_worker:deploy-cfr25-node-0",
        "//components/bms_worker:deploy-cfr25-node-1",
        "//components/bms_worker:deploy-cfr25-node-2",
        "//components/bms_worker:deploy-cfr25-node-3",
        "//components/bms_worker:deploy-cfr25-node-4",
        "//components/bms_worker:deploy-cfr25-node-5",
        "//components/sws:deploy-cfr25",
        "//components/vc/front:deploy-cfr25",
        "//components/vc/rear:deploy-cfr25",
        "//components/vc/pdu:deploy-cfr25",
    ],
    manifest = "//network:manifest-uds",
)

alias(
    name = "cfr25-ota",
    actual = "//drive-stack/carputer:cfr25-ota",
)

conUDS_batch(
    name = "cfr26-download",
    srcs = [
        "//components/bms_boss:deploy-cfr26",
        "//components/bms_worker:deploy-cfr26-node-0",
        "//components/bms_worker:deploy-cfr26-node-1",
        "//components/bms_worker:deploy-cfr26-node-2",
        "//components/bms_worker:deploy-cfr26-node-3",
        "//components/bms_worker:deploy-cfr26-node-4",
        "//components/bms_worker:deploy-cfr26-node-5",
        "//components/bms_worker:deploy-cfr26-node-6",
        "//components/bms_worker:deploy-cfr26-node-7",
        "//components/sws:deploy-cfr26",
        "//components/vc/front:deploy-cfr26",
        "//components/vc/rear:deploy-cfr26",
        "//components/vc/pdu:deploy-cfr26",
    ],
    manifest = "//network:manifest-uds",
)

alias(
    name = "cfr26-ota",
    actual = "//drive-stack/carputer:cfr26-ota",
)

alias(
    name = "cfr26-promote",
    actual = "//drive-stack/carputer:promote-carputer-cfr26",
)

alias(
    name = "status",
    actual = "//drive-stack/carputer:status",
)

alias(
    name = "revert",
    actual = "//drive-stack/carputer:revert",
)
