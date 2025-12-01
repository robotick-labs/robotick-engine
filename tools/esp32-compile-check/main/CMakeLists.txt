# robotick-engine/examples/cpp/esp32-testbed/main/CMakeLists.txt

idf_component_register(
    SRCS "esp32_testbed_main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES robotick-engine
)

