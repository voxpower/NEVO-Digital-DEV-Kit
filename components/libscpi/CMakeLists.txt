cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "src/error.c"
                            "src/expression.c"
                            "src/fifo.c"
                            "src/ieee488.c"
                            "src/lexer.c"
                            "src/minimal.c"
                            "src/parser.c"
                            "src/units.c"
                            "src/utils.c"
                    INCLUDE_DIRS "inc" "."
                    PRIV_INCLUDE_DIRS "src"
                    REQUIRES main
                    )
