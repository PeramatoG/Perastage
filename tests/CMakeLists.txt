add_executable(pdf_text_test pdf_text_test.cpp ../core/pdftext.cpp)

# Link with pdftext and podofo
find_package(podofo CONFIG REQUIRED)

target_link_libraries(pdf_text_test PRIVATE podofo::podofo)

target_include_directories(pdf_text_test PRIVATE ../core)

add_test(NAME PdfTextComparison
         COMMAND pdf_text_test
         ${CMAKE_CURRENT_SOURCE_DIR}/data/simple.pdf
         ${CMAKE_CURRENT_SOURCE_DIR}/data/translated.pdf)
