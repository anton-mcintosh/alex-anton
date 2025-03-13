
idf_component_register(SRCS "ble_gopro.c" "peer.c" "misc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "nvs_flash" "bt" "json")
