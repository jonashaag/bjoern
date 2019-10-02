#ifndef __statsd_tags__
#define __statsd_tags__

#include "statsd-client.h"

int send_stats_with_tags(statsd_link* link, char *stat, size_t value, const char *type, const char* tags);
int statsd_inc_with_tags(statsd_link* link, char *stat, const char* tags);

#endif