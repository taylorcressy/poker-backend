SET(EXEC_SRC
    main.cpp
)



add_executable(pokergame ${EXEC_SRC})

target_link_libraries(
        pokergame
        PRIVATE
        pokercore
)