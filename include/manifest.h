#ifndef MANIFEST_H
#define MANIFEST_H

#include "greybus-manifest.h"
#include <stdint.h>
#include <zephyr/sys/slist.h>

struct gb_cport {
  sys_snode_t node;
  int id;
  int bundle;
  int protocol;
};

sys_slist_t gb_manifest_get_cports(void *, size_t);

void gb_cports_free(sys_slist_t);

#endif
