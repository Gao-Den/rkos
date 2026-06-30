
INCLUDE_FLAGS += -I./sdk/STM32G4xx_HAL_Driver/Inc
INCLUDE_FLAGS += -I./sdk/STM32G4xx_HAL_Driver/Inc

VPATH += ./sdk/STM32G4xx_HAL_Driver/Src

# C source files
SOURCE_C += stm32g4xx_ll_pwr.c
SOURCE_C += stm32g4xx_ll_utils.c
SOURCE_C += stm32g4xx_ll_gpio.c
SOURCE_C += stm32g4xx_ll_rcc.c
SOURCE_C += stm32g4xx_ll_usart.c
SOURCE_C += stm32g4xx_ll_i2c.c
SOURCE_C += stm32g4xx_ll_spi.c
SOURCE_C += stm32g4xx_ll_dma.c

SOURCE_C += stm32g4xx_hal.c
SOURCE_C += stm32g4xx_hal_cortex.c
SOURCE_C += stm32g4xx_hal_pwr_ex.c
SOURCE_C += stm32g4xx_hal_rcc.c
SOURCE_C += stm32g4xx_hal_gpio.c
SOURCE_C += stm32g4xx_hal_spi.c
