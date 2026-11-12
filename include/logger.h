#ifndef EDGE_LOGGER_H
#define EDGE_LOGGER_H
void edge_log_set_level(int level);
void edge_log(int level, const char *fmt, ...);
#endif
