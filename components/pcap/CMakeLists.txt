idf_component_register(
    SRCS "pcap.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_timer esp_driver_uart esp_driver_gpio esp_ringbuf netscan
)
