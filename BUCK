load("//drive-stack/conUDS/defs.bzl", "conUDS_batch")
load("//drive-stack/ota-agent/defs.bzl", "ota_agent_batch")

filegroup(
    name = "cfr25-embedded",
    srcs = [
        "//components/bms_boss:crc-1",
        "//components/bms_worker:crc-0.all_nodes",
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
        "//components/sws:crc-0",
        "//components/vc/front:crc-0",
        "//components/vc/pdu:crc-0",
        "//components/vc/rear:crc-0",
    ],
)

conUDS_batch(
    name = "cfr25-download",
    srcs = [
        "//components/bms_boss:deploy-1",
        "//components/bms_worker:deploy-node-0",
        "//components/bms_worker:deploy-node-1",
        "//components/bms_worker:deploy-node-2",
        "//components/bms_worker:deploy-node-3",
        "//components/bms_worker:deploy-node-4",
        "//components/bms_worker:deploy-node-5",
        "//components/sws:deploy-0",
        "//components/vc/front:deploy-0",
        "//components/vc/rear:deploy-0",
        "//components/vc/pdu:deploy-0",
    ],
    manifest = "//network:manifest-uds",
)

ota_agent_batch(
    name = "cfr25-ota",
    srcs = [
        "//components/bms_boss:deploy-1",
        "//components/bms_worker:deploy-node-0",
        "//components/bms_worker:deploy-node-1",
        "//components/bms_worker:deploy-node-2",
        "//components/bms_worker:deploy-node-3",
        "//components/bms_worker:deploy-node-4",
        "//components/bms_worker:deploy-node-5",
        "//components/sws:deploy-0",
        "//components/vc/front:deploy-0",
        "//components/vc/rear:deploy-0",
        "//components/vc/pdu:deploy-0",
    ],
)
