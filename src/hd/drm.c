

#include <fcntl.h>

#include "hd.h"
#include "hd_int.h"

int is_kms_active(hd_data_t *hd_data) {
  int kms = open("/sys/class/drm/card0", O_RDONLY) > 0;
  ADD2LOG("  KMS detected: %d\n", kms);

  return kms; 
}

