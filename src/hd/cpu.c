#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 0 /* __ia64__ */
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#endif

#include "hd.h"
#include "hd_int.h"
#include "klog.h"
#include "cpu.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * cpu info
 *
 * Note: on other architectures, entries differ (cf. Alpha)!!!
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void read_cpuinfo(hd_data_t *hd_data);
static void dump_cpu_data(hd_data_t *hd_data);

#ifdef __ia64__
static int ia64DetectSMP(hd_data_t *hd_data);
#endif

void hd_scan_cpu(hd_data_t *hd_data)
{
  hd_t *hd0, *hd;
  int i, cpus;
  unsigned u;

  if(!hd_probe_feature(hd_data, pr_cpu)) return;

  hd_data->module = mod_cpu;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->cpu = free_str_list(hd_data->cpu);

  PROGRESS(1, 0, "cpuinfo");

  read_cpuinfo(hd_data);

  for(hd0 = hd_data->hd; hd0; hd0 = hd0->next) {
    if(hd0->base_class == bc_internal && hd0->sub_class == sc_int_cpu) break;
  }

  if(!hd0 || hd0->next) return;		/* 0 or > 1 entries */

  /* only one entry, maybe UP kernel on SMP system */

  cpus = 0;

#ifdef __ia64__
  cpus = ia64DetectSMP(hd_data);
#endif

  for(i = 1; i < cpus; i++) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    u = hd->idx;
    hd_copy(hd, hd0);
    hd->idx = u;
    hd->slot = i;
  }
}


void read_cpuinfo(hd_data_t *hd_data)
{
  hd_t *hd;
  unsigned cpus = 0;
  cpu_info_t *ct;
  str_list_t *sl;

#ifdef __i386__
  char model_id[80], vendor_id[80], features[0x100];
  unsigned bogo, mhz, cache, family, model, stepping;
  char *t0, *t;
#endif

#ifdef __ia64__
  char model_id[80], vendor_id[80], features[0x100];
  unsigned mhz, stepping;
  char *t0, *t;
#endif

#ifdef __alpha__
  char model_id[80], system_id[80], serial_number[80], platform[80];
  unsigned cpu_variation, cpu_revision, u, hz;
  cpu_info_t *ct1;
#endif

#ifdef __PPC__
  char model_id[80], vendor_id[80], motherboard[80];
  unsigned bogo, mhz, cache, family, model, stepping;
#endif

#ifdef __sparc__
  char cpu_id[80], fpu_id[80], promlib[80], prom[80], type[80], mmu[80];
  unsigned u, bogo, cpus_active;
#endif

#if defined(__s390__) || defined(__s390x__)
  char vendor_id[80];
  unsigned bogo;
  unsigned u0, u1, u2, u3;
#endif

  hd_data->cpu = read_file(PROC_CPUINFO, 0, 0);
  if((hd_data->debug & HD_DEB_CPU)) dump_cpu_data(hd_data);
  if(!hd_data->cpu) return;

#ifdef __alpha__
  *model_id = *system_id = *serial_number = *platform = 0;
  cpu_variation = cpu_revision = hz = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu model : %79[^\n]", model_id) == 1) continue;
    if(sscanf(sl->str, "system type : %79[^\n]", system_id) == 1) continue;
    if(sscanf(sl->str, "cpu variation : %u", &cpu_variation) == 1) continue;
    if(sscanf(sl->str, "cpu revision : %u", &cpu_revision) == 1) continue;
    if(sscanf(sl->str, "system serial number : %79[^\n]", serial_number) == 1) continue;
    if(sscanf(sl->str, "cpus detected : %u", &cpus) == 1) continue;
    if(sscanf(sl->str, "cycle frequency [Hz] : %u", &hz) == 1) continue;
    if(sscanf(sl->str, "system variation : %79[^\n]", platform) == 1) continue;
  }

  if(*model_id || *system_id) {	/* at least one of those */
    ct = new_mem(sizeof *ct);
    ct->architecture = arch_alpha;
    if(model_id) ct->model_name = new_str(model_id);
    if(system_id) ct->vend_name = new_str(system_id);
    if(strncmp(serial_number, "MILO", 4) == 0)
      hd_data->boot = boot_milo;
    else
      hd_data->boot = boot_aboot;

    ct->family = cpu_variation;
    ct->model = cpu_revision;
    ct->stepping = 0;
    ct->cache = 0;
    ct->clock = (hz + 500000) / 1000000;

    if(platform && strcmp(platform, "0")) {
      ct->platform = new_str(platform);
    }

    if(!cpus) cpus = 1;		/* at least 1 machine had a "cpus: 0" entry... */
    for(u = 0; u < cpus; u++) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_internal;
      hd->sub_class = sc_int_cpu;
      hd->slot = u;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      if(u) {
        hd->detail->cpu.data = ct1 = new_mem(sizeof *ct);
        *ct1 = *ct;
        ct1->model_name = new_str(ct1->model_name);
        ct1->vend_name = new_str(ct1->vend_name);
        ct1->platform = new_str(ct1->platform);
      }
      else {
        hd->detail->cpu.data = ct;
      }
    }

  }
#endif	/* __alpha__ */


#ifdef __sparc__
  *cpu_id = *fpu_id = *promlib = *prom = *type = *mmu = 0;
  cpus = cpus_active = bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu             : %79[^\n]", cpu_id) == 1);
    if(sscanf(sl->str, "fpu             : %79[^\n]", fpu_id) == 1);
    if(sscanf(sl->str, "promlib         : %79[^\n]", promlib) == 1);
    if(sscanf(sl->str, "prom            : %79[^\n]", prom) == 1);
    if(sscanf(sl->str, "type            : %79[^\n]", type) == 1);
    if(sscanf(sl->str, "ncpus probed    : %u", &cpus) == 1);
    if(sscanf(sl->str, "ncpus active    : %u", &cpus_active) == 1);
    if(sscanf(sl->str, "BogoMips        : %u", &bogo) == 1);
    if(sscanf(sl->str, "MMU Type        : %79[^\n]", mmu) == 1);
  }

  if(*cpu_id) {
    for(u = 0; u < cpus; u++) {
      ct = new_mem(sizeof *ct);
      ct->platform = new_str (type);
      if(strcmp (type, "sun4u") == 0)
        ct->architecture = arch_sparc64;
      else
        ct->architecture = arch_sparc;

      ct->model_name = new_str(cpu_id);
      hd_data->boot = boot_silo;

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_internal;
      hd->sub_class = sc_int_cpu;
      hd->slot = u;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      hd->detail->cpu.data = ct;
    }
  }
#endif	/* sparc */


#ifdef __i386__
  *model_id = *vendor_id = *features = 0;
  bogo = mhz = cache = family = model = stepping = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "model name : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "vendor_id : %79[^\n]", vendor_id) == 1);
    if(sscanf(sl->str, "flags : %255[^\n]", features) == 1);
    if(sscanf(sl->str, "bogomips : %u", &bogo) == 1);
    if(sscanf(sl->str, "cpu MHz : %u", &mhz) == 1);
    if(sscanf(sl->str, "cache size : %u KB", &cache) == 1);

    if(sscanf(sl->str, "cpu family : %u", &family) == 1);
    if(sscanf(sl->str, "model : %u", &model) == 1);
    if(sscanf(sl->str, "stepping : %u", &stepping) == 1);

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id || *vendor_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
	ct->architecture = arch_intel;
        if(model_id) ct->model_name = new_str(model_id);
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
	hd_data->boot = boot_lilo;

        /* round clock to typical values */
        if(mhz >= 38 && mhz <= 42)
          mhz = 40;
        else if(mhz >= 88 && mhz <= 92)
          mhz = 90;
        else {
	  unsigned u, v;

          u = (mhz + 2) % 100;
          v = (mhz + 2) / 100;
          if(u <= 4)
            u = 2;
          else if(u >= 25 && u <= 29)
            u = 25 + 2;
          else if(u >= 33 && u <= 37)
            u = 33 + 2;
          else if(u >= 50 && u <= 54)
            u = 50 + 2;
          else if(u >= 66 && u <= 70)
            u = 66 + 2;
          else if(u >= 75 && u <= 79)
            u = 75 + 2;
          else if(u >= 80 && u <= 84)	/* there are 180MHz PPros */
            u = 80 + 2;
          u -= 2;
          mhz = v * 100 + u;
        }

        ct->clock = mhz;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_internal;
        hd->sub_class = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;

        if(*features) {
          for(t0 = features; (t = strsep(&t0, " ")); ) {
            add_str_list(&ct->features, t);
          }
        }

        *model_id = *vendor_id = 0;
        bogo = mhz = cache = family = model= 0;
        cpus++;
      }
    }
  }
#endif /* __i386__  */


#ifdef __PPC__
  *model_id = *vendor_id = *motherboard = 0;
  bogo = mhz = cache = family = model = stepping = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "machine : %79[^\n]", vendor_id) == 1);
  }

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "motherboard : %79[^\n]", motherboard) == 1);
    if(sscanf(sl->str, "bogomips : %u", &bogo) == 1);
    if(sscanf(sl->str, "clock : %u", &mhz) == 1);
    if(sscanf(sl->str, "L2 cache : %u KB", &cache) == 1);

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
        ct->architecture = arch_ppc;
        if(model_id) {
          ct->model_name = new_str(model_id);
          if(strstr(model_id, "POWER3 ")) ct->architecture = arch_ppc64;
        }
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        if(motherboard) ct->platform = new_str(motherboard);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
        hd_data->boot = boot_ppc;
        ct->clock = mhz;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_internal;
        hd->sub_class = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;

        if(ct->vend_name && !strcmp(ct->vend_name, "PowerBook") && !hd_data->color_code) {
          hd_data->color_code = 7;	// black
        }
        
        *model_id = 0;
        bogo = mhz = cache = family = model= 0;
        cpus++;
      }
    }
  }
#endif /* __PPC__  */


#ifdef __ia64__
  *model_id = *vendor_id = *features = 0;
  mhz = stepping = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "model : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "vendor : %79[^\n]", vendor_id) == 1);
    if(sscanf(sl->str, "features : %255[^\n]", features) == 1);
    if(sscanf(sl->str, "cpu MHz : %u", &mhz) == 1);
    if(sscanf(sl->str, "revision : %u", &stepping) == 1);

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id || *vendor_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
	ct->architecture = arch_ia64;
        if(model_id) ct->model_name = new_str(model_id);
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        ct->stepping = stepping;
	hd_data->boot = boot_elilo;

        /* round clock to typical values */
        if(mhz >= 38 && mhz <= 42)
          mhz = 40;
        else if(mhz >= 88 && mhz <= 92)
          mhz = 90;
        else {
	  unsigned u, v;

          u = (mhz + 2) % 100;
          v = (mhz + 2) / 100;
          if(u <= 4)
            u = 2;
          else if(u >= 25 && u <= 29)
            u = 25 + 2;
          else if(u >= 33 && u <= 37)
            u = 33 + 2;
          else if(u >= 50 && u <= 54)
            u = 50 + 2;
          else if(u >= 66 && u <= 70)
            u = 66 + 2;
          else if(u >= 75 && u <= 79)
            u = 75 + 2;
          else if(u >= 80 && u <= 84)	/* there are 180MHz PPros */
            u = 80 + 2;
          u -= 2;
          mhz = v * 100 + u;
        }

        ct->clock = mhz;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_internal;
        hd->sub_class = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;

        if(*features) {
          for(t0 = features; (t = strsep(&t0, " ")); ) {
            add_str_list(&ct->features, t);
          }
        }

        *model_id = *vendor_id = 0;
        mhz = 0;
        cpus++;
      }
    }
  }

#endif /* __ia64__  */


#if defined(__s390__) || defined(__s390x__)
  *vendor_id = 0;
  bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "vendor_id : %79[^\n]", vendor_id) == 1);
    if(sscanf(sl->str, "bogomips per cpu : %u", &bogo) == 1);
  }

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(
      sscanf(sl->str, "processor %u : version = %u , identification = %u , machine = %u", &u0, &u1, &u2, &u3) == 4
    ) {
      ct = new_mem(sizeof *ct);
      ct->architecture = arch_s390;
      if(vendor_id) ct->vend_name = new_str(vendor_id);
      ct->stepping = u1;
      hd_data->boot = boot_s390;

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_internal;
      hd->sub_class = sc_int_cpu;
      hd->slot = cpus;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      hd->detail->cpu.data = ct;

      bogo = 0;
      cpus++;
    }
  }
#endif /* defined(__s390__) || defined(__s390x__) */
}

/*
 * Add some cpu data to the global log.
 */
void dump_cpu_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- /proc/cpuinfo -----\n");
  for(sl = hd_data->cpu; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/cpuinfo end -----\n");
}


#if 0
#ifdef __ia64__

/*
 * IA64 SMP detection code
 */

#define ACPI_RSDP_SIG "RSD PTR "
#define ACPI_RSDT_SIG "RSDT"
#define ACPI_SAPIC_SIG "SPIC"

#define PAGE_OFFSET(addr) ((uintptr_t)(addr) & (getpagesize() - 1))

struct acpi20_rsdp
{
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt;
  uint32_t lenth;
  unsigned long xsdt;
  uint8_t ext_checksum;
  uint8_t reserved[3];
} __attribute__ ((packed));
typedef struct acpi20_rsdp acpi20_rsdp_t;

struct acpi_desc_table_hdr
{
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__ ((packed));
typedef struct acpi_desc_table_hdr acpi_desc_table_hdr_t;

#define ACPI_XSDT_SIG "XSDT"
struct acpi_xsdt
{
  acpi_desc_table_hdr_t header;
  unsigned long entry_ptrs[0];
} __attribute__ ((packed));
typedef struct acpi_xsdt acpi_xsdt_t;

#define ACPI_MADT_SIG "APIC"
struct acpi_madt {
  acpi_desc_table_hdr_t header;
  uint32_t lapic_address;
  uint32_t flags;
} __attribute__ ((packed));
typedef struct acpi_madt acpi_madt_t;

#define ACPI20_ENTRY_LOCAL_SAPIC                7

struct acpi20_entry_lsapic
{
  uint8_t type;
  uint8_t length;
  uint8_t acpi_processor_id;
  uint8_t id;
  uint8_t eid;
  uint8_t reserved[3];
  uint32_t flags;
} __attribute__ ((packed));
typedef struct acpi20_entry_lsapic acpi20_entry_lsapic_t;

struct acpi_rsdt
{
  acpi_desc_table_hdr_t header;
  uint8_t reserved[4];
  unsigned long entry_ptrs[0];
} __attribute__ ((packed));
typedef struct acpi_rsdt acpi_rsdt_t;

struct acpi_sapic
{
  acpi_desc_table_hdr_t header;
  uint8_t reserved[4];
  unsigned long interrupt_block;
} __attribute__ ((packed));
typedef struct acpi_sapic acpi_sapic_t;

/* SAPIC structure types */
#define ACPI_ENTRY_LOCAL_SAPIC         0

struct acpi_entry_lsapic
{
  uint8_t type;
  uint8_t length;
  uint16_t acpi_processor_id;
  uint16_t flags;
  uint8_t id;
  uint8_t eid;
} __attribute__ ((packed));
typedef struct acpi_entry_lsapic acpi_entry_lsapic_t;

/* Local SAPIC flags */
#define LSAPIC_ENABLED                (1<<0)
#define LSAPIC_PERFORMANCE_RESTRICTED (1<<1)
#define LSAPIC_PRESENT                (1<<2)

/*
 * Map an ACPI table into virtual memory
 */
static acpi_desc_table_hdr_t *
acpi_map_table (int mem, unsigned long addr, char *signature)
{
  /* mmap header to determine table size */
  acpi_desc_table_hdr_t *table = NULL;
  unsigned long offset = PAGE_OFFSET (addr);
  uint8_t *mapped = mmap (NULL,
			  sizeof (acpi_desc_table_hdr_t) + offset,
			  PROT_READ,
			  MAP_PRIVATE,
			  mem,
			  (unsigned long) addr - offset);
  table = (acpi_desc_table_hdr_t *) (mapped != MAP_FAILED
				     ? mapped + offset
				     : NULL);
  if (table)
    {
      if (memcmp (table->signature, signature, sizeof (table->signature)))
	{
	  munmap ((char *) table - offset,
		  sizeof (acpi_desc_table_hdr_t) + offset);
	  return NULL;
	}
      {
	/* re-mmap entire table */
	unsigned long size = table->length;
	munmap ((uint8_t *) table - offset,
		sizeof (acpi_desc_table_hdr_t) + offset);
	mapped = mmap (NULL, size + offset, PROT_READ, MAP_PRIVATE, mem,
		       (unsigned long) addr - offset);
	table = (acpi_desc_table_hdr_t *) (mapped != MAP_FAILED
					   ? mapped + offset
					   : NULL);
      }
    }
  return table;
}

/*
 * Unmap an ACPI table from virtual memory
 */
static void
acpi_unmap_table (acpi_desc_table_hdr_t * table)
{
  if (table)
    {
      unsigned long offset = PAGE_OFFSET (table);
      munmap ((uint8_t *) table - offset, table->length + offset);
    }
}

/*
 * Locate the RSDP
 */
static unsigned long
acpi_find_rsdp (int mem,
		unsigned long base, unsigned long size, acpi20_rsdp_t *rsdp)
{
  unsigned long addr = 0;
  uint8_t *mapped = mmap (NULL, size, PROT_READ, MAP_PRIVATE, mem, base);
  if (mapped != MAP_FAILED)
    {
      uint8_t *i;
      for (i = mapped; i < mapped + size; i += 16)
	{
	  acpi20_rsdp_t *r = (acpi20_rsdp_t *) i;
	  if (memcmp (r->signature, ACPI_RSDP_SIG, sizeof (r->signature)) == 0)
	    {
	      memcpy (rsdp, r, sizeof (*rsdp));
	      addr = base + (i - mapped);
	      break;
	    }
	}
      munmap (mapped, size);
    }
  return addr;
}

int
acpi20_lsapic (char *p)
{
  acpi20_entry_lsapic_t *lsapic = (acpi20_entry_lsapic_t *) p;

  return ((lsapic->flags & LSAPIC_ENABLED) != 0);
}

static int
acpi20_parse_madt (acpi_madt_t *madt)
{
  char *p, *end;
  int n_cpu = 0;

  p = (char *) (madt + 1);
  end = p + (madt->header.length - sizeof (acpi_madt_t));

  while (p < end)
    {
      if (*p == ACPI20_ENTRY_LOCAL_SAPIC)
	n_cpu += acpi20_lsapic (p);

      p += p[1];
    }

  return n_cpu;
}

static int
acpi_lsapic (char *p)
{
  acpi_entry_lsapic_t *lsapic = (acpi_entry_lsapic_t *) p;

  if ((lsapic->flags & LSAPIC_PRESENT) == 0
      || (lsapic->flags & LSAPIC_ENABLED) == 0
      || lsapic->flags & LSAPIC_PERFORMANCE_RESTRICTED)
    return 0;
  return 1;
}

static int
acpi_parse_msapic (acpi_sapic_t *msapic)
{
  char *p, *end;
  int n_cpu = 0;

  p = (char *) (msapic + 1);
  end = p + (msapic->header.length - sizeof (acpi_sapic_t));

  while (p < end)
    {
      if (*p == ACPI_ENTRY_LOCAL_SAPIC)
	n_cpu += acpi_lsapic(p);

      p += p[1];
    }

  return n_cpu;
}

static int
acpi_parse_rsdp (int mem_fd, acpi20_rsdp_t *rsdp)
{
  int n_cpu = 0;
  int i;
  acpi_rsdt_t *rsdt = 0;
  acpi_xsdt_t *xsdt = 0;
  int tables;

  if (rsdp->xsdt)
    xsdt = (acpi_xsdt_t *) acpi_map_table (mem_fd, rsdp->xsdt, ACPI_XSDT_SIG);
  if (xsdt)
    {
      tables = (xsdt->header.length - sizeof (acpi_desc_table_hdr_t)) / 8;
      for (i = 0; i < tables; i++)
	{
	  acpi_desc_table_hdr_t *dt
	    = acpi_map_table (mem_fd, xsdt->entry_ptrs[i], ACPI_MADT_SIG);
	  if (dt)
	    n_cpu += acpi20_parse_madt ((acpi_madt_t *) dt);
	  acpi_unmap_table (dt);
	}
      acpi_unmap_table ((acpi_desc_table_hdr_t *) xsdt);
    }
  else
    {
      if (rsdp->rsdt)
	rsdt = (acpi_rsdt_t *) acpi_map_table (mem_fd, rsdp->rsdt,
					       ACPI_RSDT_SIG);
      if (rsdt)
	{
	  tables = (rsdt->header.length - sizeof (acpi_desc_table_hdr_t)) / 8;
	  for (i = 0; i < tables; i++)
	    {
	      acpi_desc_table_hdr_t *dt
		= acpi_map_table (mem_fd, rsdt->entry_ptrs[i], ACPI_SAPIC_SIG);
	      if (dt)
		n_cpu += acpi_parse_msapic ((acpi_sapic_t *) dt);
	      acpi_unmap_table (dt);
	    }
	  acpi_unmap_table ((acpi_desc_table_hdr_t *) rsdt);
	}
    }
  return n_cpu;
}

int ia64DetectSMP(hd_data_t *hd_data)
{
  int n_cpu = 0, mem_fd, i;
  acpi20_rsdp_t rsdp;
  unsigned long addr = 0;
  int ok = 0;
  str_list_t *sl;
  static char *rsd_klog = "Root System Description Ptr at ";
  char *s;
  off_t o;

  unsigned long ranges[][2] = {
    { 0x7fe00000, 0x80000000 - 0x7fe00000 },
    { 0x3fe00000, 0x40000000 - 0x3fe00000 },
    { 0x000e0000, 0x00100000 - 0x000e0000 }
  };

  mem_fd = open("/dev/mem", O_RDONLY);
  if(mem_fd == -1) return -1;

  if(!hd_data->klog) read_klog(hd_data);

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if((s = strstr(sl->str, rsd_klog))) {
      if(sscanf(s + strlen(rsd_klog), "%lx", &addr) == 1) {
//        addr &= ~(0xfL << 60);
        if(lseek(mem_fd, (off_t) addr, SEEK_SET) != -1) {
          ADD2LOG("seek to 0x%lx\n", addr);
          if((o = read(mem_fd, &rsdp, sizeof rsdp)) == sizeof rsdp) {
            ADD2LOG("got rsdp at 0x%lx\n", addr);
            ok = 1;
          }
          ADD2LOG("size = 0x%lx\n", o);
        }
        break;
      }
    }
  }

#if 0
  for(i = 0; i < sizeof ranges / sizeof *ranges; i++) {
    if(ranges[i][0]) {
      ADD2LOG("looking for RSDP in 0x%lx - 0x%lx\n",
        ranges[i][0], ranges[i][0] + ranges[i][1] - 1
      );
      if((addr = acpi_find_rsdp(mem_fd, ranges[i][0], ranges[i][1], &rsdp))) {
        n_cpu = acpi_parse_rsdp(mem_fd, &rsdp);
        if(n_cpu) {
          ADD2LOG("RSDP found at 0x%lx\n", addr);
          break;
        }
      }
    }
  }
#endif

  if(ok) {
    n_cpu = acpi_parse_rsdp(mem_fd, &rsdp);
    if(n_cpu) {
      ADD2LOG("RSDP found at 0x%lx\n", addr);
    }
  }

  close (mem_fd);

  ADD2LOG("n_cpu = %d\n", n_cpu);

  return n_cpu;
}


#endif	/* __ia64__ */

#else	/* 0 */

int ia64DetectSMP(hd_data_t *hd_data)
{
  str_list_t *sl;
  unsigned u1 = 0, u2;

  if(!hd_data->klog) read_klog(hd_data);

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if(sscanf(sl->str, "<4> %u CPUs available, %u CPUs total", &u1, &u2) == 2) {
      ADD2LOG("cpus: %u available, %u total\n", u1, u2);
      break;
    }
  }

  return u1;
}



#endif	/* 0 */

