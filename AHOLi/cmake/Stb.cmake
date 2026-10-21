# Keep image IO reproducible in clean checkouts, including linked worktrees.
set(AHO_STB_INCLUDE_DIR "${CMAKE_BINARY_DIR}/_deps/aho-stb" CACHE PATH "Directory containing stb_image.h and stb_image_write.h")
set(aho_stb_commit a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d)
set(aho_stb_headers stb_image.h stb_image_write.h)
set(aho_stb_hashes
    594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
    cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05)
foreach(index RANGE 0 1)
    list(GET aho_stb_headers ${index} header)
    list(GET aho_stb_hashes ${index} hash)
    if(NOT EXISTS "${AHO_STB_INCLUDE_DIR}/${header}")
        file(MAKE_DIRECTORY "${AHO_STB_INCLUDE_DIR}")
        file(DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/${aho_stb_commit}/${header}"
            "${AHO_STB_INCLUDE_DIR}/${header}" EXPECTED_HASH "SHA256=${hash}"
            TLS_VERIFY ON STATUS result TIMEOUT 90)
        list(GET result 0 status)
        if(NOT status EQUAL 0)
            file(REMOVE "${AHO_STB_INCLUDE_DIR}/${header}")
            message(FATAL_ERROR "Could not fetch ${header}: ${result}. Set AHO_STB_INCLUDE_DIR for offline builds.")
        endif()
    endif()
endforeach()
