#ifndef _env_gui_h
#define _env_gui_h

sockaddr_in * gui_addr();

void gui_force_update();
void gui_update(sockaddr_in * addr);

#endif
