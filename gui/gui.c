#include "gui.h"
#include "../video/video.h"

void initg(void) {
     video_clear(0x1F);
     video_fill(0, 0, 25, 80, 0x07);
     video_flush();
}