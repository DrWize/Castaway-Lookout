idf_component_register(SRCS "jcnet.c"
                       INCLUDE_DIRS "include"
                       REQUIRES jccontrol jcengine esp_event esp_http_server
                                esp_netif esp_wifi mdns nvs_flash mbedtls
                                json esp_timer)
