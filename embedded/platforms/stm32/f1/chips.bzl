chips = {
    "stm32f103": {
        "defaultLinkerFile": "f103/STM32F103C8.ld",
        "srcs": [
            "Src/stm32f1xx_hal.c",
        ],
        "headers": [
            "Inc/stm32f1xx_hal.h",
            ("Legacy/stm32_hal_legacy.h", "Inc/Legacy/stm32_hal_legacy.h"),
            ("stm32f1xx_hal_def.h", "Inc/stm32f1xx_hal_def.h"),
        ],
        "drivers": {
            "adc": {
                "srcs": [
                    "Src/stm32f1xx_hal_adc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_adc.h",
                    "Inc/stm32f1xx_hal_adc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_adc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_adc.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "adc_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_adc_ex.c",
                ],
            },
            "can": {
                "srcs": [
                    "Src/stm32f1xx_hal_can.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_can.h",
                ],
            },
            "cec": {
                "srcs": [
                    "Src/stm32f1xx_hal_cec.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_cec.h",
                ],
            },
            "cortex": {
                "required": True,
                "srcs": [
                    "Src/stm32f1xx_hal_cortex.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_cortex.h",
                ],
            },
            "crc": {
                "srcs": [
                    "Src/stm32f1xx_hal_crc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_crc.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_crc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_crc.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "dac": {
                "srcs": [
                    "Src/stm32f1xx_hal_dac.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_dac.h",
                    "Inc/stm32f1xx_hal_dac_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_dac.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_dac.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "dac_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_dac_ex.c",
                ],
            },
            "dma": {
                "srcs": [
                    "Src/stm32f1xx_hal_dma.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_dma.h",
                    "Inc/stm32f1xx_hal_dma_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_dma.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_dma.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "eth": {
                "srcs": [
                    "Src/stm32f1xx_hal_eth.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_eth.h",
                ],
            },
            "exti": {
                "srcs": [
                    "Src/stm32f1xx_hal_exti.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_exti.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_exti.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_exti.h",
                    ],
                },
            },
            "flash": {
                "srcs": [
                    "Src/stm32f1xx_hal_flash.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_flash.h",
                    "Inc/stm32f1xx_hal_flash_ex.h",
                ],
            },
            "flash_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_flash_ex.c",
                ],
            },
            "fsmc": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_fsmc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_fsmc.h",
                    ],
                },
            },
            "gpio": {
                "srcs": [
                    "Src/stm32f1xx_hal_gpio.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_gpio.h",
                    "Inc/stm32f1xx_hal_gpio_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_gpio.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_gpio.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "gpio_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_gpio_ex.c",
                ],
            },
            "hcd": {
                "srcs": [
                    "Src/stm32f1xx_hal_hcd.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_hcd.h",
                ],
            },
            "i2c": {
                "srcs": [
                    "Src/stm32f1xx_hal_i2c.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_i2c.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_i2c.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_i2c.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "i2s": {
                "srcs": [
                    "Src/stm32f1xx_hal_i2s.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_i2s.h",
                ],
            },
            "irda": {
                "srcs": [
                    "Src/stm32f1xx_hal_irda.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_irda.h",
                ],
            },
            "iwdg": {
                "srcs": [
                    "Src/stm32f1xx_hal_iwdg.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_iwdg.h",
                ],
            },
            "mmc": {
                "srcs": [
                    "Src/stm32f1xx_hal_mmc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_mmc.h",
                ],
            },
            "nand": {
                "srcs": [
                    "Src/stm32f1xx_hal_nand.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_nand.h",
                ],
            },
            "nor": {
                "srcs": [
                    "Src/stm32f1xx_hal_nor.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_nor.h",
                ],
            },
            "pccard": {
                "srcs": [
                    "Src/stm32f1xx_hal_pccard.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pccard.h",
                ],
            },
            "pcd": {
                "srcs": [
                    "Src/stm32f1xx_hal_pcd.c",
                    "Inc/stm32f1xx_hal_pcd_ex.h",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pcd.h",
                ],
            },
            "pcd_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_pcd_ex.c",
                ],
            },
            "pwr": {
                "srcs": [
                    "Src/stm32f1xx_hal_pwr.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pwr.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_pwr.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_pwr.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "rcc": {
                "srcs": [
                    "Src/stm32f1xx_hal_rcc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_rcc.h",
                    "Inc/stm32f1xx_hal_rcc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_rcc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "rcc_ex": {
                "required": True,
                "srcs": [
                    "Src/stm32f1xx_hal_rcc_ex.c",
                ],
            },
            "rtc": {
                "srcs": [
                    "Src/stm32f1xx_hal_rtc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_rtc.h",
                    "Inc/stm32f1xx_hal_rtc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_rtc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_rtc.h",
                    ],
                },
            },
            "rtc_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_rtc_ex.c",
                ],
            },
            "sd": {
                "srcs": [
                    "Src/stm32f1xx_hal_sd.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_sd.h",
                ],
            },
            "sdmmc": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_sdmmc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_sdmmc.h",
                    ],
                },
            },
            "smartcard": {
                "srcs": [
                    "Src/stm32f1xx_hal_smartcard.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_smartcard.h",
                ],
            },
            "spi": {
                "srcs": [
                    "Src/stm32f1xx_hal_spi.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_spi.h",
                ],
                "ll": {
                    "srcs": [
                        ("Src/stm32f1xx_ll_spi.c", ["-DUSE_FULL_LL_DRIVER"]),
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_spi.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "sram": {
                "srcs": [
                    "Src/stm32f1xx_hal_sram.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_sram.h",
                ],
            },
            "tim": {
                "srcs": [
                    "Src/stm32f1xx_hal_tim.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_tim.h",
                    "Inc/stm32f1xx_hal_tim_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_tim.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_tim.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "tim_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_tim_ex.c",
                ],
            },
            "uart": {
                "srcs": [
                    "Src/stm32f1xx_hal_uart.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_uart.h",
                ],
            },
            "usart": {
                "srcs": [
                    "Src/stm32f1xx_hal_usart.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_usart.h",
                ],
                "ll": {
                    "srcs": [
                        "stm32f1xx_ll_usart",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_usart.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "usb": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_usb.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_usb.h",
                    ],
                },
            },
            "utils": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_utils.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_utils.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "wwdg": {
                "srcs": [
                    "Src/stm32f1xx_hal_wwdg.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_wwdg.h",
                ],
            },
        },
    },
    "stm32f105": {
        "defaultLinkerFile": "f105/STM32F105VC.ld",
        "srcs": [
            "Src/stm32f1xx_hal.c",
        ],
        "headers": [
            "Inc/stm32f1xx_hal.h",
            ("Legacy/stm32_hal_legacy.h", "Inc/Legacy/stm32_hal_legacy.h"),
            ("stm32f1xx_hal_def.h", "Inc/stm32f1xx_hal_def.h"),
        ],
        "drivers": {
            "adc": {
                "srcs": [
                    "Src/stm32f1xx_hal_adc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_adc.h",
                    "Inc/stm32f1xx_hal_adc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_adc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_adc.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "adc_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_adc_ex.c",
                ],
            },
            "can": {
                "srcs": [
                    "Src/stm32f1xx_hal_can.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_can.h",
                ],
            },
            "cec": {
                "srcs": [
                    "Src/stm32f1xx_hal_cec.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_cec.h",
                ],
            },
            "cortex": {
                "required": True,
                "srcs": [
                    "Src/stm32f1xx_hal_cortex.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_cortex.h",
                ],
            },
            "crc": {
                "srcs": [
                    "Src/stm32f1xx_hal_crc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_crc.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_crc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_crc.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "dac": {
                "srcs": [
                    "Src/stm32f1xx_hal_dac.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_dac.h",
                    "Inc/stm32f1xx_hal_dac_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_dac.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_dac.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "dac_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_dac_ex.c",
                ],
            },
            "dma": {
                "srcs": [
                    "Src/stm32f1xx_hal_dma.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_dma.h",
                    "Inc/stm32f1xx_hal_dma_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_dma.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_dma.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "eth": {
                "srcs": [
                    "Src/stm32f1xx_hal_eth.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_eth.h",
                ],
            },
            "exti": {
                "srcs": [
                    "Src/stm32f1xx_hal_exti.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_exti.h",
                    "Inc/stm32f1xx_hal_flash_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_exti.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_exti.h",
                    ],
                },
            },
            "flash": {
                "srcs": [
                    "Src/stm32f1xx_hal_flash.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_flash.h",
                ],
            },
            "flash_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_flash_ex.c",
                ],
            },
            "fsmc": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_fsmc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_fsmc.h",
                    ],
                },
            },
            "gpio": {
                "srcs": [
                    "Src/stm32f1xx_hal_gpio.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_gpio.h",
                    "Inc/stm32f1xx_hal_gpio_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_gpio.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_gpio.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "gpio_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_gpio_ex.c",
                ],
            },
            "hcd": {
                "srcs": [
                    "Src/stm32f1xx_hal_hcd.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_hcd.h",
                ],
            },
            "i2c": {
                "srcs": [
                    "Src/stm32f1xx_hal_i2c.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_i2c.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_i2c.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_i2c.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "i2s": {
                "srcs": [
                    "Src/stm32f1xx_hal_i2s.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_i2s.h",
                ],
            },
            "irda": {
                "srcs": [
                    "Src/stm32f1xx_hal_irda.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_irda.h",
                ],
            },
            "iwdg": {
                "srcs": [
                    "Src/stm32f1xx_hal_iwdg.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_iwdg.h",
                ],
            },
            "mmc": {
                "srcs": [
                    "Src/stm32f1xx_hal_mmc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_mmc.h",
                ],
            },
            "nand": {
                "srcs": [
                    "Src/stm32f1xx_hal_nand.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_nand.h",
                ],
            },
            "nor": {
                "srcs": [
                    "Src/stm32f1xx_hal_nor.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_nor.h",
                ],
            },
            "pccard": {
                "srcs": [
                    "Src/stm32f1xx_hal_pccard.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pccard.h",
                ],
            },
            "pcd": {
                "srcs": [
                    "Src/stm32f1xx_hal_pcd.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pcd.h",
                    "Inc/stm32f1xx_hal_pcd_ex.h",
                ],
            },
            "pcd_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_pcd_ex.c",
                ],
            },
            "pwr": {
                "srcs": [
                    "Src/stm32f1xx_hal_pwr.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_pwr.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_pwr.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_pwr.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "rcc": {
                "srcs": [
                    "Src/stm32f1xx_hal_rcc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_rcc.h",
                    "Inc/stm32f1xx_hal_rcc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_rcc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "rcc_ex": {
                "required": True,
                "srcs": [
                    "Src/stm32f1xx_hal_rcc_ex.c",
                ],
            },
            "rtc": {
                "srcs": [
                    "Src/stm32f1xx_hal_rtc.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_rtc.h",
                    "Inc/stm32f1xx_hal_rtc_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_rtc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_rtc.h",
                    ],
                },
            },
            "rtc_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_rtc_ex.c",
                ],
            },
            "sd": {
                "srcs": [
                    "Src/stm32f1xx_hal_sd.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_sd.h",
                ],
            },
            "sdmmc": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_sdmmc.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_sdmmc.h",
                    ],
                },
            },
            "smartcard": {
                "srcs": [
                    "Src/stm32f1xx_hal_smartcard.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_smartcard.h",
                ],
            },
            "spi": {
                "srcs": [
                    "Src/stm32f1xx_hal_spi.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_spi.h",
                ],
                "ll": {
                    "srcs": [
                        ("Src/stm32f1xx_ll_spi.c", ["-DUSE_FULL_LL_DRIVER"]),
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_spi.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "sram": {
                "srcs": [
                    "Src/stm32f1xx_hal_sram.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_sram.h",
                ],
            },
            "tim": {
                "srcs": [
                    "Src/stm32f1xx_hal_tim.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_tim.h",
                    "Inc/stm32f1xx_hal_tim_ex.h",
                ],
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_tim.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_tim.h",
                        "Inc/stm32f1xx_ll_bus.h",
                    ],
                },
            },
            "tim_ex": {
                "srcs": [
                    "Src/stm32f1xx_hal_tim_ex.c",
                ],
            },
            "uart": {
                "srcs": [
                    "Src/stm32f1xx_hal_uart.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_uart.h",
                ],
            },
            "usart": {
                "srcs": [
                    "Src/stm32f1xx_hal_usart.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_usart.h",
                ],
                "ll": {
                    "srcs": [
                        "stm32f1xx_ll_usart",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_usart.h",
                        "Inc/stm32f1xx_ll_bus.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "usb": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_usb.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_usb.h",
                    ],
                },
            },
            "utils": {
                "ll": {
                    "srcs": [
                        "Src/stm32f1xx_ll_utils.c",
                    ],
                    "headers": [
                        "Inc/stm32f1xx_ll_utils.h",
                        "Inc/stm32f1xx_ll_rcc.h",
                    ],
                },
            },
            "wwdg": {
                "srcs": [
                    "Src/stm32f1xx_hal_wwdg.c",
                ],
                "headers": [
                    "Inc/stm32f1xx_hal_wwdg.h",
                ],
            },
        },
    },
}
