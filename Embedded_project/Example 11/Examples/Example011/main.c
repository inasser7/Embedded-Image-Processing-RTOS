#include <stdio.h>
#include "image_bridge.h"

int main(void)
{
    if (cam_init(0) != 0)
    {
        printf("Failed to init camera\n");
        return -1;
    }

    printf("Camera initialized. Grabbing 5 frames...\n");

    for (int i = 0; i < 5; ++i)
    {
        SimpleFrame f;
        int res = cam_get_frame(&f);
        if (res != 0)
        {
            printf("cam_get_frame failed, code = %d\n", res);
            break;
        }

        printf("Frame %d: %dx%d, first pixel BGR = (%u,%u,%u)\n",
            i,
            f.width, f.height,
            (unsigned)f.data[0],
            (unsigned)f.data[1],
            (unsigned)f.data[2]);

        cam_release_frame(&f);
    }

    cam_close();
    printf("Done.\n");
    return 0;
}
