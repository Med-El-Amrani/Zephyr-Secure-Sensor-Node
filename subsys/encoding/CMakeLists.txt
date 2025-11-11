# Encoding subsystem CMakeLists

zephyr_library()

zephyr_library_sources(
    json_encoder.c
)

# Optional: Add CBOR encoder if enabled
# zephyr_library_sources_ifdef(CONFIG_CBOR cbor_encoder.c)

zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)