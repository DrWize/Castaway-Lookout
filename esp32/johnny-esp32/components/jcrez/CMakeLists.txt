idf_component_register(SRCS "jcrez.c" "jcunpack.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_partition
                       PRIV_REQUIRES mbedtls)
