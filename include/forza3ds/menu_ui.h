#ifndef F3DS_MENU_UI_H
#define F3DS_MENU_UI_H

#include <3ds.h>

void menu_ui_begin(void);
void menu_ui_item(int terminal_row, int selected);
void menu_ui_panel(int y, int height);

#endif
