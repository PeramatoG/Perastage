enable_testing()

find_package(podofo CONFIG)
if(podofo_FOUND)
    add_executable(pdf_text_test pdf_text_test.cpp ../core/pdftext.cpp)
    target_link_libraries(pdf_text_test PRIVATE podofo::podofo)
    target_include_directories(pdf_text_test PRIVATE ../core)
    add_test(NAME PdfTextComparison
             COMMAND pdf_text_test
             ${CMAKE_CURRENT_SOURCE_DIR}/data/simple.pdf
             ${CMAKE_CURRENT_SOURCE_DIR}/data/translated.pdf)
endif()

add_executable(autopatcher_test autopatcher_test.cpp
               ../core/autopatcher.cpp
               ../core/patchmanager.cpp)
target_include_directories(autopatcher_test PRIVATE ../core ../models ../viewer3d)
add_test(NAME AutoPatchTypeGrouping COMMAND autopatcher_test)
