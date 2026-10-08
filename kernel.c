#include "video/video.h"
#include "input/input.h"
#include "time/time.h"
#include "intr/intr.h"
#include "system/shell.h"

void kmain(void)
{
    __asm__ volatile ("cli");
    video_init();
    video_clear(0x1F);
    input_init();
    intr_init();
    
    print("                      _____ _ _               ___  ____ \n", 0x1F);
    print("                     |  ___(_) | ___   _ ___ / _ \\/ ___| \n", 0x1F);
    print("                     | |_  | | |/ / | | / __| | | \\___ \\ \n", 0x1F);
    print("                     |  _| | |   <| |_| \\__ \\ |_| |___) | \n", 0x1F);
    print("                     |_|   |_|_|\\_\\__,_|___/\\___/|____/ \n\n", 0x1F);
    print("                                   FikusOS", 0x1F);
    delay_ms(2000);
    video_clear(0x0F);
    print("FikusOS terminal\n\n", 0x0F);
    console();
    
}