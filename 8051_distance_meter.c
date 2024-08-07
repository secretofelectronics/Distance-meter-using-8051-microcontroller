#include <reg51.h>
#include <math.h>

void i2c_start(void);
void i2c_stop(void);
void i2c_ACK(void);
void i2c_write(unsigned char);
void i2c_DevWrite(unsigned char);
void lcd_send_cmd(unsigned char);
void lcd_send_data(unsigned char);
void lcd_send_str(unsigned char *);
void lcd_slave(unsigned char);
void delay_ms(unsigned int);
void float_to_string(float, char*);

unsigned char slave1 = 0x4E;
unsigned char slave_add;

sbit scl = P2^1;
sbit sda = P2^0;

#define sound_velocity 34300  	/* sound velocity in cm per second */
#define period_in_us 1e-6
#define Clock_period 1.085 * period_in_us	/* period for clock cycle of 8051 */

sbit Trigger_pin = P2^2;        	/* Trigger pin */
sbit Echo_pin = P2^3;		/* Echo pin */

/* I2C Functions */
void i2c_start(void) {
    sda = 1;
    scl = 1;
    sda = 0;
    scl = 0;  // Ensure SCL is low before starting the next condition
}

void i2c_stop(void) {
    scl = 0;
    sda = 0;
    scl = 1;
    sda = 1;
}

void i2c_ACK(void) {
    scl = 0;
    sda = 1;
    scl = 1;
    while(sda);
}

void i2c_write(unsigned char dat) {
    unsigned char i;
    for(i = 0; i < 8; i++) {
        scl = 0;
        sda = (dat & (0x80 >> i)) ? 1 : 0;
        scl = 1;
    }
}

void lcd_slave(unsigned char slave) {
    slave_add = slave;
}

/* LCD Functions */
void lcd_send_cmd(unsigned char cmd) {
    unsigned char cmd_l, cmd_u;
    cmd_l = (cmd << 4) & 0xf0;
    cmd_u = (cmd & 0xf0);
    
    i2c_start();
    i2c_write(slave_add);
    i2c_ACK();
    i2c_write(cmd_u | 0x0C);
    i2c_ACK();
    delay_ms(1);
    i2c_write(cmd_u | 0x08);
    i2c_ACK();
    delay_ms(10);
    i2c_write(cmd_l | 0x0C);
    i2c_ACK();
    delay_ms(1);
    i2c_write(cmd_l | 0x08);
    i2c_ACK();
    delay_ms(10);
    i2c_stop();
}

void lcd_send_data(unsigned char dataw) {
    unsigned char dataw_l, dataw_u;
    dataw_l = (dataw << 4) & 0xf0;
    dataw_u = (dataw & 0xf0);
    
    i2c_start();
    i2c_write(slave_add);
    i2c_ACK();
    i2c_write(dataw_u | 0x0D);
    i2c_ACK();
    delay_ms(1);
    i2c_write(dataw_u | 0x09);
    i2c_ACK();
    delay_ms(10);
    i2c_write(dataw_l | 0x0D);
    i2c_ACK();
    delay_ms(1);
    i2c_write(dataw_l | 0x09);
    i2c_ACK();
    delay_ms(10);
    i2c_stop();
}

void lcd_send_str(unsigned char *p) {
    while(*p != '\0') {
        lcd_send_data(*p++);
    }
}

void delay_ms(unsigned int n) {
    unsigned int m;
    for(; n > 0; n--) {
        for(m = 121; m > 0; m--);
    }
}

void lcd_init() {
    lcd_send_cmd(0x02);  // Return home
    lcd_send_cmd(0x28);  // 4-bit mode
    lcd_send_cmd(0x0C);  // Display On, cursor off
    lcd_send_cmd(0x06);  // Increment Cursor (shift cursor to right)
    lcd_send_cmd(0x01);  // Clear display
}

/* Timer Functions */
void Delay_us() {
    TL0 = 0xF5;
    TH0 = 0xFF;
    TR0 = 1;
    while (TF0 == 0);
    TR0 = 0;
    TF0 = 0;
}

void init_timer() {
    TMOD = 0x01;  /* initialize Timer */
    TF0 = 0;
    TR0 = 0;
}

void send_trigger_pulse() {
    Trigger_pin = 1;           	/* pull trigger pin HIGH */
    Delay_us();               	/* provide 10uS Delay */
    Trigger_pin = 0;          	/* pull trigger pin LOW */
}

/* Convert float to string */
void float_to_string(float value, char *buffer) {
    unsigned int int_part = (unsigned int)value;
    unsigned int frac_part = (unsigned int)((value - int_part) * 100); // Get 2 decimal places
    int i = 0, j;

    // Convert integer part to string
    if (int_part == 0) {
        buffer[i++] = '0';
    } else {
        unsigned int temp = int_part;
        char temp_buffer[5];
        int k = 0;
        while (temp > 0) {
            temp_buffer[k++] = (temp % 10) + '0';
            temp /= 10;
        }
        // Reverse the string
        for (j = k - 1; j >= 0; j--) {
            buffer[i++] = temp_buffer[j];
        }
    }
    
    buffer[i++] = '.';
    buffer[i++] = (frac_part / 10) + '0';
    buffer[i++] = (frac_part % 10) + '0';
    buffer[i] = '\0'; // Null-terminate the string
}

void main() {
    float distance_measurement;
    unsigned int timer_count;
    char distance_str[16];
    
    init_timer();			/* Initialize Timer */
    lcd_slave(slave1);		/* Set LCD I2C Address */
    lcd_init();			/* Initialize LCD */
    
    while (1) {		
        send_trigger_pulse();			/* send trigger pulse of 10us */
    
        while (!Echo_pin);           		/* Waiting for Echo */
        TR0 = 1;                    		/* Timer Starts */
        while (Echo_pin && !TF0);    		/* Waiting for Echo goes LOW */
        TR0 = 0;                    		/* Stop the timer */
	  
        /* calculate distance using timer */
        timer_count = TL0 | (TH0 << 8);	/* read timer register for time count */
        distance_measurement = timer_count * Clock_period * sound_velocity / 2.0;  /* find distance (in cm) */
        
        lcd_send_cmd(0x80); // Move cursor to the beginning of the first line
        lcd_send_str("Distance: ");
        
        float_to_string(distance_measurement, distance_str);
				lcd_send_cmd(0xC0);
        lcd_send_str(distance_str);
				lcd_send_cmd(0xC8);
        lcd_send_str("cm");
        delay_ms(1000); // Delay before next measurement
    }
}
