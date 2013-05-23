char * version   = "5.0.1";
char * osName    = "x86_64-3.2.0-32-generic-linux-gnu";
char * ntop_author    = "Luca Deri <deri@ntop.org>";
char * configureDate = "Apr 29 2013  1:56:46";
#define buildDateIs __DATE__ " " __TIME__
char * buildDate    = buildDateIs;
char * configure_parameters   = "--prefix=/home/acer/ntop1 --no-create --no-recursion";
char * host_system_type   = "x86_64-unknown-linux-gnu";
char * target_system_type   = "x86_64-unknown-linux-gnu";
char * compiler_cppflags   = "gcc -E -DLINUX -I/usr/local/include -I/opt/local/include";
char * compiler_cflags   = "gcc -g -O2 -I/usr/local/include -I/opt/local/include -Wshadow -Wpointer-arith -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fPIC -DPIC -I/usr/include/python2.7 -I/usr/include/python2.7 -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -g -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security -DHAVE_CONFIG_H";
char * include_path    = "-I/usr/include/python2.7 -fno-strict-aliasing -DNDEBUG -fwrapv -O2 -Wall -Wstrict-prototypes -g -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security";
char * system_libs    = "-L/usr/local/lib -L/opt/local/lib -lcrypt -lc -lssl -lcrypto -lrrd_th -lpcap -lgdbm -lz -lpthread -ldl -lutil -lm -lpython2.7 -lGeoIP";
char * install_path   = "/home/acer/ntop1";
char * locale_dir   = "/usr/lib/locale";
char * distro   = "Ubuntu";
char * release   = "12.04";
char * force_runtime   = "";

/* Memory Debug: */
#ifdef MEMORY_DEBUG
  char * memoryDebug = " ";
#endif /* MEMORY_DEBUG */
