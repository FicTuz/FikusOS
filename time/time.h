#ifndef TIME_H
#define TIME_H

struct rtc_time {
    unsigned char second;   /* 0..59  */
    unsigned char minute;   /* 0..59  */
    unsigned char hour;     /* 0..23  */
    unsigned char day;      /* 1..31  */
    unsigned char month;    /* 1..12  */
    unsigned int  year;     /* например 2026 */
};

/* Блокирующие задержки. Точность ~ 1 мс / ~ 1 мкс. */
void delay_ms(unsigned int ms);
void delay_us(unsigned int us);

/* Чтение текущего времени из аппаратных часов CMOS/RTC. */
void rtc_read(struct rtc_time *t);

/* Печать времени в формате YYYY-MM-DD HH:MM:SS\n */
void time_print(const struct rtc_time *t, unsigned char color);

#endif