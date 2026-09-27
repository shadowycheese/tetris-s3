idf_component_register(SRCS 
                        "main.cpp"
                        "matrix/hub75_driver.cpp"
                        "esp/espio.c"
                        "engine/ctetris.c"
                        "engine/renderer.cpp"
                       INCLUDE_DIRS 
                        "."
                        "matrix/"
                        "esp/"
                        "engine/"
                       REQUIRES driver)
