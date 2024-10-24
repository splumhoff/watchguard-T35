/* exported serial functions */

int lcm_panel_init(void);
int lcm_serial_tstc (void);
int lcm_serial_getc (void);
void lcm_serial_putc (const char c);
void lcm_serial_puts (const char *s, int len);

int lcd_puts (int line, char * text);
int lcd_clear (void);
