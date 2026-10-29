# Launcher de re-wor: elige resolucion y nombre de usuario, muestra los DLC y abre el juego.
add_executable(re_wor_launcher WIN32 launcher.cpp launcher.rc)
set_target_properties(re_wor_launcher PROPERTIES OUTPUT_NAME "Launcher")
target_link_libraries(re_wor_launcher PRIVATE comctl32 shell32 gdiplus uxtheme ole32)
# Controles con el estilo actual de Windows (Common Controls 6).
target_link_options(re_wor_launcher PRIVATE
    "LINKER:/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
