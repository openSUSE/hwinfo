#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __ia64__
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#endif

#ifdef __powerpc__
#include <sys/utsname.h>
#endif

#include "hd.h"
#include "hd_int.h"
#include "klog.h"
#include "cpu.h"

/**
 * @defgroup CPUint CPU information
 * @ingroup  libhdINFOint
 * @brief CPU detection functions
 *
 * Note: on other architectures, entries differ (cf. Alpha)!!!
 * 
 * @{
 */

static void read_cpuinfo(hd_data_t *hd_data);
static void dump_cpu_data(hd_data_t *hd_data);

#if defined(__i386__) || defined(__x86_64__)
static inline unsigned units_per_cpu();
#endif
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
    if(hd0->base_class.id == bc_internal && hd0->sub_class.id == sc_int_cpu) break;
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

#if defined(__i386__) || defined (__x86_64__)
  char model_id[80], vendor_id[80], features[0x100];
  unsigned mhz, cache, family, model, stepping;
  double bogo;
  char *t0, *t;
#endif

#ifdef __ia64__
  char model_id[80], vendor_id[80], features[0x100];
  unsigned mhz, stepping;
  double bogo;
  char *t0, *t;
#endif

#ifdef __alpha__
  char model_id[80], system_id[80], serial_number[80], platform[80];
  unsigned cpu_variation, cpu_revision, u, hz;
  cpu_info_t *ct1;
#endif

#ifdef __PPC__
  char model_id[80], vendor_id[80], motherboard[80];
  unsigned mhz, cache, family, model, stepping;
  double bogo;
  struct utsname un;
#endif

#ifdef __sparc__
  char cpu_id[80], fpu_id[80], promlib[80], prom[80], type[80], mmu[80];
  unsigned u, cpus_active;
  double bogo;
#endif

#if defined(__s390__) || defined(__s390x__)
  char vendor_id[80];
  double bogo;
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
    ct->bogo = bogo;

    if(platform && strcmp(platform, "0")) {
      ct->platform = new_str(platform);
    }

    if(!cpus) cpus = 1;		/* at least 1 machine had a "cpus: 0" entry... */
    for(u = 0; u < cpus; u++) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_internal;
      hd->sub_class.id = sc_int_cpu;
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
  cpus = cpus_active = 0;
  bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu             : %79[^\n]", cpu_id) == 1);
    if(sscanf(sl->str, "fpu             : %79[^\n]", fpu_id) == 1);
    if(sscanf(sl->str, "promlib         : %79[^\n]", promlib) == 1);
    if(sscanf(sl->str, "prom            : %79[^\n]", prom) == 1);
    if(sscanf(sl->str, "type            : %79[^\n]", type) == 1);
    if(sscanf(sl->str, "ncpus probed    : %u", &cpus) == 1);
    if(sscanf(sl->str, "ncpus active    : %u", &cpus_active) == 1);
    if(sscanf(sl->str, "BogoMips        : %lg", &bogo) == 1);
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
      ct->bogo = bogo;

      hd_data->boot = boot_silo;

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_internal;
      hd->sub_class.id = sc_int_cpu;
      hd->slot = u;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      hd->detail->cpu.data = ct;
    }
  }
#endif	/* sparc */


#if defined(__i386__) || defined (__x86_64__)
  *model_id = *vendor_id = *features = 0;
  mhz = cache = family = model = stepping = 0;
  bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "model name : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "vendor_id : %79[^\n]", vendor_id) == 1);
    if(sscanf(sl->str, "flags : %255[^\n]", features) == 1);
    if(sscanf(sl->str, "bogomips : %lg", &bogo) == 1);
    if(sscanf(sl->str, "cpu MHz : %u", &mhz) == 1);
    if(sscanf(sl->str, "cache size : %u KB", &cache) == 1);

    if(sscanf(sl->str, "cpu family : %u", &family) == 1);
    if(sscanf(sl->str, "model : %u", &model) == 1);
    if(sscanf(sl->str, "stepping : %u", &stepping) == 1);

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id || *vendor_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
#ifdef __i386__
	ct->architecture = arch_intel;
#endif
#ifdef __x86_64__
	ct->architecture = arch_x86_64;
#endif
        if(model_id) ct->model_name = new_str(model_id);
        if(vendor_id) ct->vend_name = new_str(vendor_id);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
        ct->bogo = bogo;
	hd_data->boot = boot_grub;

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
        hd->base_class.id = bc_internal;
        hd->sub_class.id = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;

        if(*features) {
          for(t0 = features; (t = strsep(&t0, " ")); ) {
            add_str_list(&ct->features, t);
            if(!strcmp(t, "ht")) ct->units = units_per_cpu();
          }
        }

        *model_id = *vendor_id = 0;
        mhz = cache = family = model= 0;
        bogo = 0;
        cpus++;
      }
    }
  }
#endif /* __i386__ || __x86_64__ */


#ifdef __PPC__
  *model_id = *vendor_id = *motherboard = 0;
  mhz = cache = family = model = stepping = 0;
  bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "machine : %79[^\n]", vendor_id) == 1);
  }

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "cpu : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "motherboard : %79[^\n]", motherboard) == 1);
    if(sscanf(sl->str, "bogomips : %lg", &bogo) == 1);
    if(sscanf(sl->str, "clock : %u", &mhz) == 1);
    if(sscanf(sl->str, "L2 cache : %u KB", &cache) == 1);

    if(strstr(sl->str, "processor") == sl->str || !sl->next) {		/* EOF */
      if(*model_id) {	/* at least one of those */
        ct = new_mem(sizeof *ct);
        ct->architecture = arch_ppc;
        if(model_id) {
          ct->model_name = new_str(model_id);
        }

        if(!uname(&un))
        	if(strstr(un.machine,"ppc64"))
        		ct->architecture = arch_ppc64;

        if(vendor_id) ct->vend_name = new_str(vendor_id);
        if(motherboard) ct->platform = new_str(motherboard);
        ct->family = family;
        ct->model = model;
        ct->stepping = stepping;
        ct->cache = cache;
        hd_data->boot = boot_ppc;
        ct->clock = mhz;
        ct->bogo = bogo;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_internal;
        hd->sub_class.id = sc_int_cpu;
        hd->slot = cpus;
        hd->detail = new_mem(sizeof *hd->detail);
        hd->detail->type = hd_detail_cpu;
        hd->detail->cpu.data = ct;

        if(ct->vend_name && !strcmp(ct->vend_name, "PowerBook") && !hd_data->color_code) {
          hd_data->color_code = 7;	// black
        }
        
        *model_id = 0;
        mhz = cache = family = model= 0;
        bogo = 0;
        cpus++;
      }
    }
  }
#endif /* __PPC__  */


#ifdef __ia64__
  *model_id = *vendor_id = *features = 0;
  mhz = stepping = 0;
  bogo = 0;

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(sscanf(sl->str, "family : %79[^\n]", model_id) == 1);
    if(sscanf(sl->str, "vendor : %79[^\n]", vendor_id) == 1);
    if(sscanf(sl->str, "features : %255[^\n]", features) == 1);
    if(sscanf(sl->str, "cpu MHz : %u", &mhz) == 1);
    if(sscanf(sl->str, "revision : %u", &stepping) == 1);
    if(sscanf(sl->str, "BogoMIPS : %lg", &bogo) == 1);

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
        ct->bogo = bogo;

        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class.id = bc_internal;
        hd->sub_class.id = sc_int_cpu;
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
    if(sscanf(sl->str, "bogomips per cpu : %lg", &bogo) == 1);
  }

  for(sl = hd_data->cpu; sl; sl = sl->next) {
    if(
      sscanf(sl->str, "processor %u : version = %x , identification = %x , machine = %x", &u0, &u1, &u2, &u3) == 4
    ) {
      ct = new_mem(sizeof *ct);
#ifdef __s390x__
      ct->architecture = arch_s390x;
#else
      ct->architecture = arch_s390;
#endif
      if(vendor_id) ct->vend_name = new_str(vendor_id);
      ct->stepping = u1;
      hd_data->boot = boot_s390;
      ct->bogo = bogo;

      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class.id = bc_internal;
      hd->sub_class.id = sc_int_cpu;
      hd->slot = cpus;
      hd->detail = new_mem(sizeof *hd->detail);
      hd->detail->type = hd_detail_cpu;
      hd->detail->cpu.data = ct;

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


#if defined(__i386__) || defined(__x86_64__)
inline unsigned units_per_cpu()
{
  unsigned u;

  asm(
#ifdef __i386__
    "push %%ebx\n\t"
#else
    "push %%rbx\n\t"
#endif
    "mov $1,%%eax\n\t"
    "cpuid\n\t"
    "shr $8,%%ebx\n\t"
    "movzx %%bh,%%eax\n\t"
#ifdef __i386__
    "pop %%ebx"
#else
    "pop %%rbx"
#endif
    : "=a" (u)
    :: "%ecx", "%edx"
  );

  return u;
}
#endif


#ifdef __ia64__

/*
 * IA64 SMP detection code
 */

#define PAGE_OFFSET(addr) ((uintptr_t) (addr) & (getpagesize () - 1))

typedef struct
{
  uint8_t type;
  uint8_t length;
} __attribute__ ((packed)) acpi_table_entry_header;

struct acpi20_table_rsdp
{
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
  uint32_t length;
  unsigned long xsdt_address;
  uint8_t ext_checksum;
  uint8_t reserved[3];
} __attribute__ ((packed));

struct acpi_table_header
{
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  char asl_compiler_id[4];
  uint32_t asl_compiler_revision;
};

#define ACPI_XSDT_SIG "XSDT"
struct acpi_table_xsdt
{
  struct acpi_table_header header;
  unsigned long entry[0];
} __attribute__ ((packed));

#define ACPI_MADT_SIG "ACPI"
struct acpi_table_madt
{
  struct acpi_table_header header;
  uint32_t lapic_address;
  struct
  {
    uint32_t pcat_compat:1;
    uint32_t reserved:31;
  } flags;
} __attribute__ ((packed));

#define ACPI_MADT_LSAPIC 7

struct acpi_table_lsapic
{
  acpi_table_entry_header header;
  uint8_t acpi_id;
  uint8_t id;
  uint8_t eid;
  uint8_t reserved[3];
  struct
  {
    uint32_t enabled:1;
    uint32_t reserved:31;
  } flags;
} __attribute__ ((packed));

/*
 * Map an ACPI table into virtual memory
 */
static struct acpi_table_header *
acpi_map_table (int mem, unsigned long addr, char *signature)
{
  /* mmap header to determine table size */
  struct acpi_table_header *table = NULL;
  unsigned long offset = PAGE_OFFSET (addr);
  uint8_t *mapped = mmap (NULL,
			  sizeof (struct acpi_table_header) + offset,
			  PROT_READ,
			  MAP_PRIVATE,
			  mem,
			  (unsigned long) addr - offset);
  table = (struct acpi_table_header *) (mapped != MAP_FAILED
					? mapped + offset
					: NULL);
  if (table)
    {
      if (memcmp (table->signature, signature, sizeof (table->signature)))
	{
	  munmap ((char *) table - offset,
		  sizeof (struct acpi_table_header) + offset);
	  return NULL;
	}
      {
	/* re-mmap entire table */
	unsigned long size = table->length;
	munmap ((uint8_t *) table - offset,
		sizeof (struct acpi_table_header) + offset);
	mapped = mmap (NULL, size + offset, PROT_READ, MAP_PRIVATE, mem,
		       (unsigned long) addr - offset);
	table = (struct acpi_table_header *) (mapped != MAP_FAILED
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
acpi_unmap_table (struct acpi_table_header * table)
{
  if (table)
    {
      unsigned long offset = PAGE_OFFSET (table);
      munmap ((uint8_t *) table - offset, table->length + offset);
    }
}

int
acpi_parse_lsapic (acpi_table_entry_header *p)
{
  struct acpi_table_lsapic *lsapic = (struct acpi_table_lsapic *) p;

  return lsapic->flags.enabled;
}

static int
acpi_parse_madt (struct acpi_table_madt *madt)
{
  acpi_table_entry_header *p, *end;
  int n_cpu = 0;

  p = (acpi_table_entry_header *) (madt + 1);
  end = (acpi_table_entry_header *) ((char *) madt + madt->header.length);

  while (p < end)
    {
      if (p->type == ACPI_MADT_LSAPIC)
	n_cpu += acpi_parse_lsapic (p);

      p = (acpi_table_entry_header *) ((char *) p + p->length);
    }

  return n_cpu;
}

static int
acpi_parse_rsdp (int mem_fd, struct acpi20_table_rsdp *rsdp)
{
  int n_cpu = 0;
  int i;
  struct acpi_table_xsdt *xsdt = 0;
  int tables;

  if (rsdp->xsdt_address)
    xsdt = (struct acpi_table_xsdt *) acpi_map_table (mem_fd, rsdp->xsdt_address,
						      ACPI_XSDT_SIG);
  if (xsdt)
    {
      tables = (xsdt->header.length - sizeof (struct acpi_table_header)) / 8;
      for (i = 0; i < tables; i++)
	{
	  struct acpi_table_header *dt
	    = acpi_map_table (mem_fd, xsdt->entry[i], ACPI_MADT_SIG);
	  if (dt)
	    n_cpu += acpi_parse_madt ((struct acpi_table_madt *) dt);
	  acpi_unmap_table (dt);
	}
      acpi_unmap_table ((struct acpi_table_header *) xsdt);
    }
  return n_cpu;
}

int ia64DetectSMP(hd_data_t *hd_data)
{
  int n_cpu = 0, mem_fd, systab_fd;
  struct acpi20_table_rsdp rsdp;
  uint8_t *mapped;
  unsigned long addr = 0, offset;
  int ok = 0;
  str_list_t *sl;
  const char *rsd_klog = "ACPI 2.0=";
  const char *rsd_systab = "ACPI20=";
  char *s;

  mem_fd = open("/dev/mem", O_RDONLY);
  if(mem_fd == -1) return -1;

  systab_fd = open("/proc/efi/systab", O_RDONLY);
  if (systab_fd != -1)
    {
      char buffer[512];
      int n_read = read(systab_fd, buffer, sizeof(buffer) - 1);
      close(systab_fd);
      if (n_read > 0)
	{
	  buffer[n_read] = 0;
	  if ((s = strstr(buffer, rsd_systab)) != NULL &&
	      sscanf(s + strlen(rsd_systab), "%lx", &addr) == 1)
	    goto found_it;
	}
    }

  if(!hd_data->klog) read_klog(hd_data);

  for(sl = hd_data->klog; sl; sl = sl->next) {
    if((s = strstr(sl->str, rsd_klog))) {
      if(sscanf(s + strlen(rsd_klog), "%lx", &addr) == 1) {
      found_it:
	offset= PAGE_OFFSET (addr);
        mapped = mmap(NULL, sizeof rsdp + offset, PROT_READ, MAP_PRIVATE,
		      mem_fd, (unsigned long) addr - offset);
	if(mapped != MAP_FAILED) {
	  ADD2LOG("seek to 0x%lx\n", addr);
          memcpy(&rsdp, mapped + offset, sizeof rsdp);
	  munmap(mapped, sizeof rsdp + offset);
	  ok = 1;
        }
        break;
      }
    }
  }

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

/** @} */

