#include <3ds.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    consoleClear();
    printf("Forza3DS\n");
    printf("Native New Nintendo 3DS prototype\n\n");
    printf("Bootstrap only: 3D renderer and driving\n");
    printf("systems are not implemented yet.\n\n");
    printf("Press START to exit.\n");

    while (aptMainLoop())
    {
        hidScanInput();
        const u32 keysDown = hidKeysDown();

        if (keysDown & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
