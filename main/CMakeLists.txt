file(GLOB PROTOBUF_SRCS "protobuf/pb_*.c")
idf_component_register(SRCS ${PROTOBUF_SRCS} "protobuf/uart.pb.c" "scan.c"
                    PRIV_REQUIRES esp_wifi nvs_flash esp_driver_uart 
                    INCLUDE_DIRS "." "protobuf")