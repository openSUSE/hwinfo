
typedef struct isa_isdn_s {
  struct isa_isdn_s *next;
  unsigned has_mem:1, has_io:1, has_irq:1;
  unsigned type, subtype, mem, io, irq;
} isa_isdn_t;

isa_isdn_t *new_isa_isdn(isa_isdn_t **ii);

void hd_scan_isa(hd_data_t *hd_data);

isa_isdn_t *isdn_detect(void);
