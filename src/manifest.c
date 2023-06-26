#include "manifest.h"
#include "zephyr/sys/slist.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_DECLARE(cc1352_greybus, CONFIG_BEAGLEPLAY_GREYBUS_LOG_LEVEL);

static struct gb_cport *gb_cport_alloc(sys_slist_t *list) {
  struct gb_cport *cport = k_malloc(sizeof(struct gb_cport));
  sys_slist_append(list, &cport->node);
  return cport;
}

static void gb_cport_dealloc(struct gb_cport *cport) { k_free(cport); }

sys_slist_t gb_manifest_get_cports(void *data, size_t manifest_size) {
  struct greybus_manifest *manifest = data;
  struct greybus_manifest_header *header = &manifest->header;
  struct greybus_descriptor *desc;
  size_t size = header->size;
  int desc_size;
  sys_slist_t cports;
  sys_slist_init(&cports);
  struct gb_cport *cport;

  LOG_DBG("Manifest Size: %u", size);
  LOG_DBG("Manifest version: %u.%u", header->version_major,
          header->version_minor);

  if (size > manifest_size) {
    LOG_ERR("Manifest size larger than supplied payload");
    goto early_fail;
  }

  desc = (struct greybus_descriptor *)(header + 1);
  size -= sizeof(*header);
  while (size) {
    if (desc->header.type == GREYBUS_TYPE_CPORT) {
      cport = gb_cport_alloc(&cports);
      cport->id = desc->cport.id;
      cport->bundle = desc->cport.bundle;
      cport->protocol = desc->cport.protocol_id;
    }
    desc_size = desc->header.size;
    desc = (struct greybus_descriptor *)((char *)desc + desc_size);
    size -= desc_size;
  }

early_fail:
  return cports;
}

void gb_cports_free(sys_slist_t list) {
  struct gb_cport *cport, *cport_safe;
  SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&list, cport, cport_safe, node) {
    sys_slist_remove(&list, NULL, &cport->node);
    gb_cport_dealloc(cport);
  }
}
