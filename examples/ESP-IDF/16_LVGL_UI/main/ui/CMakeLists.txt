SET(SOURCES ui_comp_paratile.c
    ui_comp.c
    ui_SplashScreen.c
    ui_MainScreen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_background_png.c
    ui_img_black_slider_png.c
    ui_img_red_slider_png.c
    fonts/ui_font_SairaCondensedBold20.c
    fonts/ui_font_SairaSemiCondensedRegualar16.c
    fonts/ui_font_SairaSemiCondensedExtraBold56.c
    fonts/ui_font_SairaSemiCondensedExtraBold24.c
    fonts/ui_font_SairaSemiCondensedExtraBold230.c
    fonts/ui_font_SairaSemiCondensedExtraBold40.c)

add_library(ui ${SOURCES})
