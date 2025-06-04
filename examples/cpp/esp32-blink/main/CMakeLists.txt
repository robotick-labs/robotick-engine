idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES robotick-engine
)

# Define ESP32 macro for app code (optional — only needed if app itself uses it)
add_definitions(-DROBOTICK_PLATFORM_ESP32)
