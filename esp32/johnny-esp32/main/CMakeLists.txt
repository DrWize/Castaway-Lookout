idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES jcboard jccontrol jcnet jcrez jcgfx jcengine esp_system esp_psram esp_timer heap
                                mbedtls nvs_flash spi_flash)
