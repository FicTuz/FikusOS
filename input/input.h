#ifndef INPUT_H
#define INPUT_H

void input_init(void);
char getchar(void);
char getchar_echo(unsigned char color);
void read_line(char *buf, int max, unsigned char color);
int  read_string(char *buf, int max, unsigned char color);

#endif