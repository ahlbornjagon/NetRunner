idf_component_register(SRCS "protobuf/uart.pb.c" "scan.c"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
