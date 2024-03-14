__asm("jmp kmain");
#define VIDEO_BUF_PTR (0xb8000)
#define PIC1_PORT (0x20)
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
#define GDT_CS (0x8)
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80)
#define VIDEO_HEIGHT (25)
#define START_POS (0)

//for cur_time
#define CUR_TIME (0x70)
#define GET_TIME (0x71)

unsigned int cur_line = 0;
unsigned int global_line = 0;
unsigned int cur_pos = 0;

volatile unsigned int ticks_counter = 0;
int counter_of_symbs = 0;
char str_of_line[VIDEO_WIDTH];

char sys_time_h = 0;
char sys_time_m = 0;
char sys_time_s = 0;



char scancode_symb(int scancode)
{
	switch (scancode)
	{
		case 0x10:  return 'q';
		case 0x11:  return 'w';
		case 0x12:  return 'e';
		case 0x13:  return 'r';
		case 0x14:  return 't';
		case 0x15:  return 'y';
		case 0x16:  return 'u';
		case 0x17:  return 'i';
		case 0x18:  return 'o';
		case 0x19:  return 'p';
		case 0x1E:  return 'a';
		case 0x1F:  return 's';
		case 0x20:  return 'd';
		case 0x21:  return 'f';
		case 0x22:  return 'g';
		case 0x23:  return 'h';
		case 0x24:  return 'j';
		case 0x25:  return 'k';
		case 0x26:  return 'l';
		case 0x2C:  return 'z';
		case 0x2D:  return 'x';
		case 0x2E:  return 'c';
		case 0x2F:  return 'v';
		case 0x30:  return 'b';
		case 0x31:  return 'n';
		case 0x32:  return 'm';
		case 0x39:  return ' ';
		default: return '\0';
	}
}

static inline unsigned char inb(unsigned short port) // read from port
{
    unsigned char data;
    asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outb(unsigned short port, unsigned char data) // input chr
{
    asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

inline void outw(unsigned short port, unsigned short data) //input number
{
	asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void out_str(int color, const char* ptr, unsigned int strnum)
{
    unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
    video_buf += 80 * 2 * strnum;
    while (*ptr)
    {
        video_buf[0] = (unsigned char)*ptr; // Символ (код)
        video_buf[1] = color; // Цвет символа и фона
        video_buf += 2;
        ptr++;
    }
}

void out_char(int color, char symbol, unsigned int cur_line, unsigned int cur_pos)
{
    unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
    video_buf += (cur_line * 80 + cur_pos) * 2; 
    video_buf[0] = (unsigned char)symbol; 
    video_buf[1] = color; 
}

void cursor_moveto(unsigned int cur_line, unsigned int cur_pos)
{
    unsigned short new_pos = (cur_line * VIDEO_WIDTH) + cur_pos;
    outb(CURSOR_PORT, 0x0F);
    outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
    outb(CURSOR_PORT, 0x0E);
    outb(CURSOR_PORT + 1, (unsigned char)((new_pos >> 8) & 0xFF));
}

void sys_time()
{
    outb(0x70, 0x00);
    sys_time_s = inb(0x71);

    outb(0x70, 0x02);
    sys_time_m = inb(0x71);

    outb(0x70, 0x04);
    sys_time_h = inb(0x71);
}


//interruprions
struct idt_entry
{
    unsigned short base_lo; // Младшие биты адреса обработчика
    unsigned short segm_sel; // Селектор сегмента кода
    unsigned char always0; // Этот байт всегда 0
    unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы - этоконстанты - IDT_TYPE...
    unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено

// Структура, адрес которой передается как аргумент команды lidt
struct idt_ptr
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp; // Описатель таблицы для команды lidt

void default_intr_handler()
{
    asm("pusha");
    asm("popa; leave; iret");
}

typedef void (*intr_handler)();

void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
	unsigned int hndlr_addr = (unsigned int) hndlr;
	g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

void intr_init()
{
    int i;
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    for (i = 0; i < idt_count; i++)
        intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start()
{
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    g_idtp.base = (unsigned int)(&g_idt[0]);
    g_idtp.limit = (sizeof(struct idt_entry) * idt_count) - 1;
    asm("lidt %0" : : "m" (g_idtp));
}

void intr_enable()
{
    asm("sti");
}

void intr_disable()
{
    asm("cli");
}

void timer_ticks()
{
	asm("pusha");

	ticks_counter++;

	outb(PIC1_PORT, 0x20);
	asm("popa; leave; iret");
}

void timer_ticks_init()
{
	intr_reg_handler(8, GDT_CS, 0x80 | IDT_TYPE_INTR, timer_ticks);
	outb(PIC1_PORT + 1, 0xFF ^ 0x01); 
}




//strings functions
int custom_strlen(const char* s) 
{
    int result = 0;
    while (*s != '\0')
    {
        s++;
        result++;
    }
    return result;
}

void custom_strcat(char* destination, const char* source)
{
    while (*destination) {
        destination++;
    }
    while (*source) {
        *destination = *source;
        destination++;
        source++;
    }
    *destination = '\0';
}

int custom_strcmp(const char* str1, const char* str2) 
{
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

void custom_strcpy(char* destination, const char* source) 
{
    while (*source) {
        *destination = *source;
        destination++;
        source++;
    }
    *destination = '\0';
}



//manipulations with string
int count_digits(int num) 
{
    int count = 0;
    while (num != 0) {
        num /= 10;
        count++;
    }
    return count;
}

void int_to_str(unsigned int num, char* str) 
{
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int i = 0;
    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    str[i] = '\0';

    // Переворачиваем строку, чтобы получить правильный порядок цифр
    int length = i;
    for (int j = 0; j < length / 2; j++) {
        char temp = str[j];
        str[j] = str[length - j - 1];
        str[length - j - 1] = temp;
    }
}

int str_to_int(char str) 
{
    int result = 0;
    while (str) {
        if (str >= '0' && str <= '9') {
            result = result * 10 + (str - '0');
            str++;
        } else {
            break;
        }
    }

    return result;
}



//work with Kboard

void keyb_handler();
void check_command();

void keyb_init()
{
    intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
    outb(PIC1_PORT + 1, 0xFF ^ 0x02); 
}

void clear()
{
	 unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
	 for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
	 {
	 	*(video_buf + i * 2) = '\0';
	 }
	 cur_line = 0; cur_pos = 0;
	 cursor_moveto(cur_line, cur_pos);
	 counter_of_symbs = 0;
	 return;
}

void remove_prev_char() 
{
    if (cur_pos > 0) {
        unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
        video_buf += VIDEO_WIDTH * 2 * cur_line + 2 * (cur_pos - 1);
        video_buf[0] = 0;
        counter_of_symbs --;
        str_of_line[counter_of_symbs] = '\0';
        cursor_moveto(cur_line, --cur_pos);
    }
}

void reset_input_buf() 
{
	cur_pos = 0;
	for (int i = 0; i< VIDEO_WIDTH; i++)
		str_of_line[i] = '\0';
	counter_of_symbs = 0;
	cursor_moveto(cur_line, cur_pos);
}

void move_cursor_up() 
{
    if (cur_line > 0) {
        cursor_moveto(--cur_line, cur_pos);
    }
}

void move_cursor_down() 
{
    if (cur_line < VIDEO_WIDTH) {
        cursor_moveto(++cur_line, cur_pos);
    }
}

void move_cursor_left() 
{
    if (cur_pos > 0) {
        cursor_moveto(cur_line, --cur_pos);
    }
}

void move_cursor_right() 
{
    if (cur_pos < VIDEO_WIDTH) {
        cursor_moveto(cur_line, ++cur_pos);
    }
}

void info() 
{
    out_str(0x0F, "Author: Zakharchenko Maksim, 5151003/20001", ++cur_line);global_line++;
    out_str(0x0F, "Development Tools: FASM, GCC, QEMU", ++cur_line);global_line++;
    out_str(0x0F, " ", ++cur_line);global_line++;
}

void help() 
{
    out_str(0x0F, "Commands:", ++cur_line);global_line++;
    out_str(0x0F, "- clear", ++cur_line);global_line++;
    out_str(0x0F, "- ticks", ++cur_line);global_line++;
    out_str(0x0F, "- info", ++cur_line);global_line++;
    out_str(0x0F, "- loadtime", ++cur_line);global_line++;
    out_str(0x0F, "- curtime", ++cur_line);global_line++;
    out_str(0x0F, "- uptime", ++cur_line);global_line++;
    out_str(0x0F, "- shutdown", ++cur_line);global_line++;
    out_str(0x0F, " ", ++cur_line);global_line++;
}

void ticks()
{
    out_str(0x0F, "Ticks: ", ++cur_line); global_line++;
    char ticks_str[20];
    int_to_str(ticks_counter, ticks_str); 
    out_str(0x0F, ticks_str, ++cur_line); global_line++;
    out_str(0x0F, " ", ++cur_line); global_line++;
}

void curtime() 
{
    char time[9];

    outb(CUR_TIME, 0x04); // hours
    time[0] = ((inb(GET_TIME) & 0xF0) >> 4) + '0';
    time[1] = (inb(GET_TIME) & 0x0F) + '0';

    outb(CUR_TIME, 0x02); // mins
    time[2] = ':';
    time[3] = ((inb(GET_TIME) & 0xF0) >> 4) + '0';
    time[4] = (inb(GET_TIME) & 0x0F) + '0';

    outb(CUR_TIME, 0x00); // sec
    time[5] = ':';
    time[6] = ((inb(GET_TIME) & 0xF0) >> 4) + '0';
    time[7] = (inb(GET_TIME) & 0x0F) + '0';
    time[8] = '\0';


    out_str(0x0F, time, ++cur_line);global_line++;
}


void loadtime() 
{
    char time[9];

    time[0] = ((sys_time_h & 0xF0) >> 4) + '0';
    time[1] = (sys_time_h & 0x0F) + '0';
    time[2] = ':';
    time[3] = ((sys_time_m & 0xF0) >> 4) + '0';
    time[4] = (sys_time_m & 0x0F) + '0';
    time[5] = ':';
    time[6] = ((sys_time_s & 0xF0) >> 4) + '0';
    time[7] = (sys_time_s & 0x0F) + '0';
    time[8] = '\0';

    // Output the start time to the screen
    out_str(0x0F, "OS started at: ", ++cur_line); global_line++;
    out_str(0x0F, time, ++cur_line); global_line++;
}

void uptime()
{
	outb(CUR_TIME, 0);
    int seconds_at_start = inb(GET_TIME);
    outb(CUR_TIME, 2);
    int mins_at_start = inb(GET_TIME);
    outb(CUR_TIME, 4);
    int hours_at_start = inb(GET_TIME);

    int hours_now = (sys_time_h / 16) * 10 + sys_time_h % 16 + 3;
    int mins_now = (sys_time_m / 16) * 10 + sys_time_m % 16;
    int seconds_now = (sys_time_s / 16) * 10 + sys_time_s % 16;
    
    hours_at_start = (hours_at_start / 16) * 10 + hours_at_start % 16 + 3;
    mins_at_start = (mins_at_start / 16) * 10 + mins_at_start % 16;
    seconds_at_start = (seconds_at_start / 16) * 10 + seconds_at_start % 16;

    int now_minus_start_h, now_minus_start_m, now_minus_start_s;
    if (mins_at_start < mins_now)
        now_minus_start_m = (60 - mins_now) + mins_at_start;
    else
        now_minus_start_m = mins_at_start - mins_now;
    if (seconds_at_start < seconds_now) {
        now_minus_start_m--;
        now_minus_start_s = (60 - seconds_now) + seconds_at_start;
    }
    else
        now_minus_start_s = seconds_at_start - seconds_now;


    char uptime_msg[50];
    int index = 0;
    out_str(0x0F, "Uptime: ", ++cur_line); global_line++;
    if (now_minus_start_m > 0)
    {
        if (now_minus_start_m == 1)
        {
            uptime_msg[index++] = '1';
            uptime_msg[index++] = ' ';
            uptime_msg[index++] = 'm';
            uptime_msg[index++] = 'i';
            uptime_msg[index++] = 'n';
            uptime_msg[index++] = 'u';
            uptime_msg[index++] = 't';
            uptime_msg[index++] = 'e';
            uptime_msg[index++] = ',';
            uptime_msg[index++] = ' ';
        }
        else
        {
            uptime_msg[index++] = (now_minus_start_m / 10) + '0';
            uptime_msg[index++] = (now_minus_start_m % 10) + '0';
            uptime_msg[index++] = ' ';
            uptime_msg[index++] = 'm';
            uptime_msg[index++] = 'i';
            uptime_msg[index++] = 'n';
            uptime_msg[index++] = 'u';
            uptime_msg[index++] = 't';
            uptime_msg[index++] = 'e';
            uptime_msg[index++] = 's';
            uptime_msg[index++] = ',';
            uptime_msg[index++] = ' ';
        }
    }
    if (now_minus_start_s > 0)
    {
        if (now_minus_start_s == 1)
        {
            uptime_msg[index++] = '1';
            uptime_msg[index++] = ' ';
            uptime_msg[index++] = 's';
            uptime_msg[index++] = 'e';
            uptime_msg[index++] = 'c';
            uptime_msg[index++] = 'o';
            uptime_msg[index++] = 'n';
            uptime_msg[index++] = 'd';
            uptime_msg[index++] = '\0';
        }
        else
        {
            uptime_msg[index++] = (now_minus_start_s / 10) + '0';
            uptime_msg[index++] = (now_minus_start_s % 10) + '0';
            uptime_msg[index++] = ' ';
            uptime_msg[index++] = 's';
            uptime_msg[index++] = 'e';
            uptime_msg[index++] = 'c';
            uptime_msg[index++] = 'o';
            uptime_msg[index++] = 'n';
            uptime_msg[index++] = 'd';
            uptime_msg[index++] = 's';
            uptime_msg[index++] = '\0';
        }
    }
    out_str(0x0F, uptime_msg, ++cur_line); global_line++;
}

/*void read_cpuid()
{
	int a[3];
	char* res;
	__asm__("mov $0x0, %ax\n\t");
	__asm__("cpuid\n\t");
	__asm__("mov %%bx, %0\n\t":"=r" (a[0]));
	__asm__("mov %%dx, %0\n\t":"=r" (a[1]));
	__asm__("mov %%cx, %0\n\t":"=r" (a[2]));
	int_to_str(a, res);
	out_str(0x0F, res, ++cur_line);
}*/

inline void shutdown()
{
	outw(0x604, 0x2000);
}

void check_command() 
{
    if (custom_strcmp(str_of_line, "info") == 0) info();
	else if (custom_strcmp(str_of_line, "help") == 0) help();
	else if (custom_strcmp(str_of_line, "clear") == 0) clear();
	else if (custom_strcmp(str_of_line, "ticks") == 0) ticks();
	else if (custom_strcmp(str_of_line, "curtime") == 0) curtime();
	else if (custom_strcmp(str_of_line, "loadtime") == 0) loadtime();
	else if (custom_strcmp(str_of_line, "uptime") == 0) uptime();
	//else if (custom_strcmp(str_of_line, "cpuid") == 0) read_cpuid();
	else if (custom_strcmp(str_of_line, "shutdown") == 0) shutdown();
	else out_str(0x0F, "Uncorect command, print help :  ", ++cur_line);global_line++;
	out_str(0x0F, " ", ++cur_line);global_line++;
}

void on_key(unsigned char scan_code) 
{
    if (scan_code == 0x0E) {
        // Handle backspace key
        remove_prev_char();
    }
    else if (scan_code == 0x1C) {
        // Handle enter key
        check_command();
        reset_input_buf();
    }
    else if (scan_code == 0x48) {
        // Handle up arrow key
        move_cursor_up();
    }
    else if (scan_code == 0x50) {
        // Handle down arrow key
        move_cursor_down();
    }
    else if (scan_code == 0x4B) {
        // Handle left arrow key
        move_cursor_left();
    }
    else if (scan_code == 0x4D) {
        // Handle right arrow key
        move_cursor_right();
    }
    else if (scan_code == 0x47) {
        // Handle Home
        cursor_moveto(cur_line, 0);
    }
    else if (scan_code == 0x4F) {
        // Handle End
        cursor_moveto(cur_line, counter_of_symbs);
    }
    char other_symbol=scancode_symb(scan_code);
	if (other_symbol != '\0') {
		out_char(0x0F, other_symbol, cur_line, cur_pos);
		cursor_moveto(cur_line, ++cur_pos);
		str_of_line[counter_of_symbs] = other_symbol;
		counter_of_symbs++;
		return;
	}
    return;
}

void keyb_process_keys()
{
    // Проверка что буфер PS/2 клавиатуры не пуст (младший битприсутствует)
    if (inb(0x64) & 0x01)
    {
        unsigned char scan_code;
        unsigned char state;
        scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
        if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
            on_key(scan_code);
    }
}

void keyb_handler()
{
	asm("pusha");
	// Обработка поступивших данных
	keyb_process_keys();
	// Отправка контроллеру 8259 нотификации о том, что прерывание обработано
	outb(PIC1_PORT, 0x20);
	asm("popa; leave; iret");
}

extern "C" int kmain()
{
	sys_time();
	timer_ticks_init();
	intr_disable();
	keyb_init();
    intr_start();
    intr_enable();
    const char* hello = "Welcome to HelloWorldOS (gcc edition)!";global_line++;
    const char* help_coma = "Write help, to see all list of commands.";global_line++;
	out_str(0x0F, hello, cur_line++);
	out_str(0x0F, help_coma, cur_line++);
	cur_pos = 0;

	cursor_moveto(cur_line, cur_pos);

	while (1)
	{
		outb(PIC1_PORT + 1, 0xFF ^ 0x02);
		outb(PIC1_PORT + 1, 0xFF ^ 0x01);
		asm("hlt");
	}
	return 0;
}