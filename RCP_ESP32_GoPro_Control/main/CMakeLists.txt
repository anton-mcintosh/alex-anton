idf_component_register(SRCS "softap_example_main.c"
                    INCLUDE_DIRS ".")
                    set(EXTRA_COMPONENT_DIRS components)
                    include_directories($ENV{IDF_PATH}/components)