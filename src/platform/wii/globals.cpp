/*
 Define DEFINE_GLOBALS if you have undefined reference errors
 for font_ttf_size, botton_over_pcm_size, etc.
*/
//#define DEFINE_GLOBALS

#ifdef DEFINE_GLOBALS

#include <gccore.h>

extern const u32 font_ttf_size = 29244;
extern const u32 button_over_pcm_size = 2876;
extern const u32 button_click_pcm_size = 7204;
extern const u32 enter_ogg_size = 59779;
extern const u32 exit_ogg_size = 51898;

#endif
