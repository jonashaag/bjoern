#include <stdio.h>
#include "statsd-client.h"

#define MAX_MSG_LEN_WITH_TAGS 1000

static int send_stats_with_tags(statsd_link* link, char *stat, size_t value, const char* type, const char* tags)
{
  if (link == NULL) {
    // Statsd disabled
    return 0;
  }

  char message[MAX_MSG_LEN_WITH_TAGS];
  snprintf(message, MAX_MSG_LEN_WITH_TAGS, "%s%s:%zd|%s|#%s", link->ns ? link->ns : "", stat, value, type, tags ? tags : "");
  return statsd_send(link, message);
}

int statsd_inc_with_tags(statsd_link* link, char* stat, const char* tags)
{
  return send_stats_with_tags(link, stat, 1, "c", tags);
}
