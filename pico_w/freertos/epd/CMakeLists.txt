if (NOT PICO_EPAPER_PATH)
    message("Skipping Pico ePaper examples with FreeRTOS as PICO_EPAPER_PATH not defined")
else()
if (EXISTS ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c)
    add_subdirectory(${PICO_EPAPER_PATH} Pico_ePaper)
    add_executable(picow_freertos_epd_nosys
            picow_freertos_epd.c
            ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
            )
    target_compile_definitions(picow_freertos_epd_nosys PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            )
    target_include_directories(picow_freertos_epd_nosys PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/../.. # for our common lwipopts
            ${PICO_LWIP_CONTRIB_PATH}/apps/ping
            ${PICO_EPAPER_PATH}/lib/Config
            ${PICO_EPAPER_PATH}/lib/e-Paper
            ${PICO_EPAPER_PATH}/lib/Fonts
            ${PICO_EPAPER_PATH}/lib/GUI
            )
    target_link_libraries(picow_freertos_epd_nosys
            pico_cyw43_arch_lwip_threadsafe_background
            pico_stdlib
            pico_lwip_iperf
            FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
            examples ePaper GUI Fonts Config pico_stdlib hardware_spi
            hardware_i2c
            )
    pico_add_extra_outputs(picow_freertos_epd_nosys)

    add_executable(picow_freertos_epd_sys
            picow_freertos_epd.c
            ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
            )
    target_compile_definitions(picow_freertos_epd_sys PRIVATE
            WIFI_SSID=\"${WIFI_SSID}\"
            WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
            NO_SYS=0            # don't want NO_SYS (generally this would be in your lwipopts.h)
            LWIP_SOCKET=1       # we need the socket API (generally this would be in your lwipopts.h)
            PING_USE_SOCKETS=1
            )
    target_include_directories(picow_freertos_epd_sys PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/../.. # for our common lwipopts
            ${PICO_LWIP_CONTRIB_PATH}/apps/ping
            ${PICO_EPAPER_PATH}/lib/Config
            ${PICO_EPAPER_PATH}/lib/e-Paper
            ${PICO_EPAPER_PATH}/lib/Fonts
            ${PICO_EPAPER_PATH}/lib/GUI
            )
    target_link_libraries(picow_freertos_epd_sys
            pico_cyw43_arch_lwip_sys_freertos
            pico_stdlib
            pico_lwip_iperf
            FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
            examples ePaper GUI Fonts Config pico_stdlib hardware_spi
            hardware_i2c
            )
    pico_add_extra_outputs(picow_freertos_epd_sys)
endif()
endif()