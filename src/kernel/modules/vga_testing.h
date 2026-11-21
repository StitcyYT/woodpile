#define VGA ((unsigned short*)0xB8000)
#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
void set_cursor(unsigned short pos);
void printk(const char *s);