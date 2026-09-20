add_library(stroke_domain STATIC src/config.cpp)
add_library(stroke::domain ALIAS stroke_domain)
target_include_directories(stroke_domain PUBLIC include)
stroke_configure_target(stroke_domain)
