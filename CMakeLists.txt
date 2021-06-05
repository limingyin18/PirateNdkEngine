project(PirateNdkEngine)

file(GLOB src "./*.cpp" "*.hpp" "*.h")
add_library(${PROJECT_NAME} SHARED ${src})

target_link_libraries(${PROJECT_NAME}
        android
        log
        camera2ndk
        mediandk
        jnigraphics
        spdlog
        )