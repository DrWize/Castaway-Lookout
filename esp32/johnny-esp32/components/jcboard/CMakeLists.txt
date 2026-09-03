if(CONFIG_JOHNNY_QEMU)
    set(JCBOARD_SOURCE "jcboard_qemu.c")
    set(JCBOARD_REQUIRES esp_lcd espressif__esp_lcd_qemu_rgb)
else()
    set(JCBOARD_SOURCE "jcboard.c")
    set(JCBOARD_REQUIRES esp_driver_i2c esp_driver_gpio esp_lcd
                         espressif__esp_lcd_touch)
endif()

idf_component_register(SRCS "${JCBOARD_SOURCE}"
                       INCLUDE_DIRS "include"
                       REQUIRES ${JCBOARD_REQUIRES} heap)
