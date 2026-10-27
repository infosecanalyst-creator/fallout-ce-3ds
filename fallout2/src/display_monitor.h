#ifndef DISPLAY_MONITOR_H
#define DISPLAY_MONITOR_H

namespace fallout {

int displayMonitorInit();
int displayMonitorReset();
void displayMonitorExit();
void displayMonitorAddMessage(char* string);
#ifdef __3DS__
int displayMonitorGetRecentLines(const char** lines, int count, unsigned int* version);
#endif
void displayMonitorDisable();
void displayMonitorEnable();

} // namespace fallout

#endif /* DISPLAY_MONITOR_H */
