if (NOT PICO_EPAPER_PATH OR NOT BME280_DRIVER_PATH OR NOT BME68x_DRIVER_PATH OR NOT BSEC_LIB_PATH OR NOT MQTT_SERVER_ADDR OR NOT MQTT_USER OR NOT MQTT_PASS)
    message("Skipping Pico ePaper examples with FreeRTOS")
else()

set(PICOW_FREERTOS_EPD_SOURCES
        picow_freertos_epd.c
        dev/bme280_i2c_dev.c
        dev/bme68x_i2c_dev.c
        bsec_handler.c
        ${BME280_DRIVER_PATH}/bme280.c
        ${BME68x_DRIVER_PATH}/bme68x.c
        )

set(PICOW_FREERTOS_EPD_DEFINES
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        MQTT_SERVER_ADDR=\"${MQTT_SERVER_ADDR}\"
        MQTT_USER=\"${MQTT_USER}\"
        MQTT_PASS=\"${MQTT_PASS}\"
        )
set(PICOW_FREERTOS_EPD_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/dev
        ${CMAKE_CURRENT_LIST_DIR}/../.. # for our common lwipopts
        ${PICO_LWIP_CONTRIB_PATH}/apps/ping
        ${PICO_EPAPER_PATH}/lib/Config
        ${PICO_EPAPER_PATH}/lib/e-Paper
        ${PICO_EPAPER_PATH}/lib/Fonts
        ${PICO_EPAPER_PATH}/lib/GUI
        ${BME280_DRIVER_PATH}/
        ${BME68x_DRIVER_PATH}/
        ${BSEC_LIB_PATH}/src/inc/
        )

 set(PICOW_FREERTOS_EPD_LINK_DIRS
        ${BSEC_LIB_PATH}/src/cortex-m0plus/
)
set(PICOW_FREERTOS_EPD_INCLUDES_LIBS
        pico_stdlib
        pico_lwip_iperf
        pico_lwip_mqtt
        pico_lwip_sntp
        FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
        examples ePaper GUI Fonts Config pico_stdlib hardware_spi
        hardware_i2c
        hardware_rtc
        hardware_exception
        hardware_timer
        algobsec
        )


add_subdirectory(${PICO_EPAPER_PATH} Pico_ePaper)

add_executable(picow_freertos_epd_nosys ${PICOW_FREERTOS_EPD_SOURCES})
target_compile_definitions(picow_freertos_epd_nosys PRIVATE ${PICOW_FREERTOS_EPD_DEFINES})
target_include_directories(picow_freertos_epd_nosys PRIVATE ${PICOW_FREERTOS_EPD_INCLUDES})
target_link_directories(picow_freertos_epd_nosys PRIVATE ${PICOW_FREERTOS_EPD_LINK_DIRS})
target_link_libraries(picow_freertos_epd_nosys
        pico_cyw43_arch_lwip_threadsafe_background
        ${PICOW_FREERTOS_EPD_INCLUDES_LIBS}
        )
pico_add_extra_outputs(picow_freertos_epd_nosys)

add_executable(picow_freertos_epd_sys ${PICOW_FREERTOS_EPD_SOURCES})
target_compile_definitions(picow_freertos_epd_sys PRIVATE
        ${PICOW_FREERTOS_EPD_DEFINES}
        NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
        LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
        PING_USE_SOCKETS=1
        )
target_include_directories(picow_freertos_epd_sys PRIVATE ${PICOW_FREERTOS_EPD_INCLUDES})
target_link_directories(picow_freertos_epd_sys PRIVATE ${PICOW_FREERTOS_EPD_LINK_DIRS})
target_link_libraries(picow_freertos_epd_sys
        pico_cyw43_arch_lwip_sys_freertos
        ${PICOW_FREERTOS_EPD_INCLUDES_LIBS}
        )
pico_add_extra_outputs(picow_freertos_epd_sys)
endif()