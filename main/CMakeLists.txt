idf_component_register(SRCS 
                        "main.cpp"
                        "matrix/Hub75.cpp"
                        "matrix/hub75_driver.cpp"
                        "esp/espio.c"
                       INCLUDE_DIRS 
                        "."
                        "matrix/"
                        "esp/"
                       REQUIRES driver)
