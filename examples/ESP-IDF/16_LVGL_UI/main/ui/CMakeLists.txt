SET(SOURCES ui_comp_paratile.c
    ui_comp.c
    ui_SplashScreen.c
    ui_MainScreen.c
    ui_AnalogClusterScreen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_background_png.c
    ui_img_black_slider_png.c
    ui_img_red_slider_png.c
    ui_img_left_indicator_png.c
    ui_img_high_beam_png.c
    ui_img_low_beam_png.c
    ui_img_brake_png.c
    ui_img_engine_earning_png.c
    ui_img_right_indicator_png.c
    ui_img_hor_bottom_slider_png.c
    ui_img_hor_top_slider_png.c
    ui_img_ver_bottom_slider_png.c
    ui_img_ver_top_slider_png.c
    ui_img_big_gauge_png.c
    ui_img_big_gauge_indicator_png.c
    ui_img_rpm_gauge_png.c
    ui_img_small_gauge_png.c
    ui_img_small_indicator_png.c
    fonts/ui_font_SairaCondensedBold20.c
    fonts/ui_font_SairaSemiCondensedExtraBold230.c
    fonts/ui_font_SairaSemiCondensedExtraBold24.c
    fonts/ui_font_SairaSemiCondensedExtraBold40.c
    fonts/ui_font_SairaSemiCondensedExtraBold56.c
    fonts/ui_font_SairaSemiCondensedRegualar16.c)

add_library(ui ${SOURCES})
