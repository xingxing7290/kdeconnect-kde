add_library(kdeconnectpanel STATIC paneldocument.cpp panelstore.cpp)
target_link_libraries(kdeconnectpanel PUBLIC Qt::Core)
target_include_directories(kdeconnectpanel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(kdeconnectpanel PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC ON)
