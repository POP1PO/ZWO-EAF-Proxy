set(backend_srcs
    "backends/simulated_focuser.cpp"
    "backends/stepper_focuser.cpp"
)

# The Gemini USB-host backend is ESP32-P4 only (it needs the second OTG as a
# host) and is an experimental scaffold; it is not compiled on the S3 build.
if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND backend_srcs "backends/gemini_usbh_focuser.cpp")
endif()

idf_component_register(
    SRCS
        "main.cpp"
        ${backend_srcs}
        "personas/eaf_descriptors.cpp"
        "personas/eaf_protocol.cpp"
        "personas/eaf_persona.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_driver_gpio
        esp_timer
        nvs_flash
)
