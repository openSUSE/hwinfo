void add_bus_name(unsigned, char *);
void add_class_name(unsigned, char *);
void add_sub_class_name(unsigned, unsigned, char *);
void add_vendor_name(unsigned, char *);
void add_device_name(unsigned, unsigned, unsigned, char *);
void add_sub_device_name(unsigned, unsigned, unsigned, unsigned, unsigned, char *);

unsigned device_class(unsigned, unsigned);
unsigned sub_device_class(unsigned, unsigned, unsigned, unsigned);
