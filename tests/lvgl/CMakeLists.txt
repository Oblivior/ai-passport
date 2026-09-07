cmake_minimum_required(VERSION 3.16)
project(pet_view_regression C CXX)
if(NOT LVGL_SOURCE_DIR)
    message(FATAL_ERROR "Set LVGL_SOURCE_DIR to an LVGL 9.5.0 checkout")
endif()
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" lvgl)
add_executable(test_pet_view test_pet_view.c ../../main/pet_view.c ../../main/pet_catalog.c ../../assets/images/digimon_sprites.c)
target_include_directories(test_pet_view PRIVATE ../../main)
target_link_libraries(test_pet_view PRIVATE lvgl::lvgl)
enable_testing()
add_test(NAME pet_view_regression COMMAND test_pet_view)
add_executable(test_pet_page test_pet_page.c ../../main/demo_pet.c ../../main/demo_ble.c ../../main/pet_view.c
    ../../assets/images/digimon_sprites.c
    ../../main/pet_life.c ../../main/pet_lunch.c ../../main/pet_model.c ../../main/pet_ui_text.c
    ../../main/pet_house.c ../../main/pet_catalog.c ../../main/pet_bond.c ../../main/pet_training.c ../../main/pet_meet.c
    ../../main/ui_pixel.c ../../main/ui_pixel_math.c
    ../../assets/fonts/passport_zh_14.c ../../assets/fonts/passport_zh_20.c)
target_include_directories(test_pet_page PRIVATE stubs ../../main ../../components/bsp/include)
target_link_libraries(test_pet_page PRIVATE lvgl::lvgl)
add_test(NAME pet_page_regression COMMAND test_pet_page)
set_tests_properties(pet_view_regression pet_page_regression PROPERTIES TIMEOUT 30)
