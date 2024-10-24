#ifndef _WGMENU_H
#define _WGMENU_H

int wgmenu_hook(int key);
void wgmenu_add(const char *label, void (*fn)(void *user), void *user);
void wgmenu_show(void);
int wgmenu_selected(void);
int wgmenu_inmenu(void);
void wgmenu_exit(void);

#endif /* _WGMENU_H */
