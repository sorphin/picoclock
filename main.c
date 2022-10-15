//﻿#include "LCD_Test.h"   //Examples
#include "stdio.h"
#include "pico/stdlib.h"
#include "stdlib.h"
#include "pico/time.h"
#include "pico/util/datetime.h"
#include "hardware/adc.h"
#include "hardware/rtc.h"
#include "hardware/gpio.h"
#include <math.h>
#include <float.h>
#include "pico/types.h"
#include "pico/bootrom/sf_table.h"

#include "lcd.h"
#include "QMI8658.h"

#include "lib/Fonts/fonts.h"
#include "img/Font34.h"
//#include "img/hanamin.h"
#include "img/Font30.h"
#include "img/earth.h"
#include "img/bega.h"
//#include "img/sand.h"
//#include "img/maple.h"
#include "img/usa32.h"
#include "img/cn32.h"
#include "img/ger32.h"
#include "img/tr32.h"
#include "img/usa16.h"
#include "img/cn16.h"
#include "img/ger16.h"
#include "img/tr16.h"


#define mcpy(d,s,sz) for(int i=0;i<sz;i++){d[i]=s[i];}
#define THEMES 4

bool Paint_ext=false;


// Start on Friday 5th of June 2020 15:45:00
datetime_t t = {
  .year  = 2022,
  .month = 10,
  .day   = 13,
  .dotw  = 4, // 0 is Sunday, so 5 is Friday
  .hour  = 7,
  .min   = 12,
  .sec   = 0
};

#define CNFONT Font30
#define DRAW_GFX_FIRST false //1 == text floating above clock
#define to_rad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)
#define to_deg(angleInRadians) ((angleInRadians) * 180.0 / M_PI)
#define HOURGLASSBORDER 200 // minimum rise/fall of acc_x
#define HOURGLASS 600  // rise/fall of acc_x border till switch (cw/ccw)
#define BUTTONGLASSC 300
#define BUTTONGLASS 1400
#define SCRSAV 60

#define BRIGHTD 20
#define FLAGS_DELAY 4

#define ACCX 60
#define ACCY 40

#define BATX 70
#define BATY 30
#define BATYS 4
#define TFONT Font20
#define TFW 14
#define DATIX 2
#define DATIY 86
#define HXST 64
#define HYST 136


void sincosf(float,float*,float*);

typedef enum CMode {
  CM_None = 0,
  CM_Config = 1,
  CM_Editpos = 2,
  CM_Changepos = 3,
  CM_Changetheme = 4
} CMode;

typedef struct ColorTheme_t{
  uint16_t alpha;
  uint16_t col_h;
  uint16_t col_m;
  uint16_t col_s;
  uint16_t col_cs;
  uint16_t col_cs5;
  uint16_t col_dotw;
  uint16_t col_date;
  uint16_t col_time;
} ColorTheme_t;

#define USA_Old_Glory_Red 0xB0C8 //0xB31942
#define USA_Old_Glory_Blue 0x098C //0x0A3161
//#define USA_Old_Glory_Red  0xC8B0 //0xB31942
//#define USA_Old_Glory_Blue 0x8C09 //0x0A3161

#define CN_Red 0xE8E4 //0xee1c25
#define CN_Gold 0xFFE0 //0xffff00
//#define CN_Red 0xE4E8 //0xee1c25
//#define CN_Gold 0xE0FF //0xffff00

//#define GER_Gold 0x60FE
//#define GER_Red 0x00F8
#define GER_Gold 0xFE60
#define GER_Red 0xF800

//#define TR_Red 0x00F8
#define TR_Red 0xF800

#define THEMES 4
//#define FLAGS_MAX 4

CMode cmode = CM_None;

uint8_t theme_pos = 0;
const uint8_t* flags[] = {cn32,usa32,ger32,tr32};
const uint8_t* stars[] = {cn16,usa16,ger16,tr16};
uint16_t alpha[] = {BLACK,BLACK};
const char* backgrounds[] = {earth,earth,bega,bega};



ColorTheme_t colt1={BLACK,CN_Red,CN_Red,CN_Gold,CN_Red,CN_Gold,LGRAY,WHITE,WHITE};
ColorTheme_t colt2={BLACK,USA_Old_Glory_Red,USA_Old_Glory_Blue,WHITE,USA_Old_Glory_Red,WHITE,NBLACK,WHITE,WHITE};
ColorTheme_t colt3={BLACK,GER_Red,0x0001,GER_Gold,GER_Red,GER_Gold,WHITE,WHITE,WHITE};
ColorTheme_t colt4={BLACK,TR_Red,TR_Red,WHITE,WHITE,TR_Red,WHITE,WHITE,WHITE};

//ColorTheme_t* colt[2] = [&colt1,&colt2];
ColorTheme_t* colt[THEMES];

char timebuffer[16] = {0};
char* ptimebuffer=timebuffer;
bool h24=true;

char datetime_buf[256];
char *datetime_str = &datetime_buf[0];
char* dt_date;
char* dt_time;

//ky-040
#define CCLK 16
#define CDT 17
#define CSW 19

//one button /
#define CBUT 22

bool analog_seconds=false;
bool fire=false;
bool ceasefire=false;
bool tcw=false;
bool tccw=false;
bool clk,dt,sw,oclk,odt,osw;
int gc=0;
char gch;
char gbuf[2] = {'c','d'};
uint32_t stime;
uint8_t tseco;
int hourglass=HOURGLASS;
int hgb;
int hgx;
int hgy;
int buttonglass=BUTTONGLASS;
int screensaver=SCRSAV;

int flagsdelay = FLAGS_DELAY;
int blink_counter = 0;
bool bmode = false;

float tsin[360];
float tcos[360];
float tfsin[600];
float tfcos[600];
bool is_sleeping;

uint8_t editpos=0;
bool edittime=false;
bool changetime=false;
char dbuf[8];
float temperature = -99.99f;
uint16_t dcol = WHITE;
uint16_t editcol = YELLOW;
uint16_t changecol = ORANGE;
uint16_t acol=WHITE;
uint16_t colors[7] = {WHITE,WHITE,WHITE,WHITE,WHITE,WHITE,WHITE};
uint16_t dcolors[7];

float mag[3];
bool draw_gfx_first = DRAW_GFX_FIRST;
bool usb_loading = false;

uint16_t result;
const float conversion_factor = 3.3f / (1 << 12) * 2;

float acc[3], gyro[3];
unsigned int tim_count = 0;

uint16_t cn_chars=0;
char ftst[128*4] = {0};

char* week_usa[7] = {"Sun\0","Mon\0","Tue\0","Wed\0","Thu\0","Fri\0","Sat\0"};
char* week_cn[7] = {"星期日\0","星期一\0","星期二\0","星期三\0","星期四\0","星期五\0","星期六\0"};
char* week_ger[7] = {"Son\0","Mon\0","Die\0","Mit\0","Don\0","Fre\0","Sam\0"};
char* week_tr[7] = {"PAZ\0","PZT\0","SAL\0","CAR\0","PER\0","CUM\0","CMT\0"};
char** week[THEMES] = {week_cn,week_usa,week_ger,week_tr};
 // dummy month0
uint8_t last[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

char cn_buffer[32] = {0};

uint16_t to_rgb565(uint8_t r,uint8_t g,uint8_t b){
  r>>=3;
  g>>=2;
  b>>=3;
  return ((r<<11)+(g<<5)+b);
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
  return ((r<<11)+(g<<5)+b);
}

void to_rgb(uint16_t rgb, uint8_t* r, uint8_t* g, uint8_t* b){
  *r=((rgb>>11)&0x1f)<<3;
  *g=((rgb>>5)&0x3f)<<3;
  *b=(rgb&0x1f)<<3;
}

void set_colors(){
  for(int i=0;i<7;i++){
    colors[i]=dcolors[i];
  }
}

void set_dcolors(){
  dcolors[0]=colt[theme_pos]->col_dotw;
  dcolors[1]=colt[theme_pos]->col_date;
  dcolors[2]=colt[theme_pos]->col_date;
  dcolors[3]=colt[theme_pos]->col_date;
  dcolors[4]=colt[theme_pos]->col_time;
  dcolors[5]=colt[theme_pos]->col_time;
  dcolors[6]=colt[theme_pos]->col_time;
}

uint8_t find_cc(uint8_t a, uint8_t b, uint8_t c){
  uint fo=0;
  for(int i=0; i<cn_chars+1;i++){
    //printf("[%02x%02x%02x] %02x %02x %02x\n",a,b,c,ftst[fo],ftst[fo+1],ftst[fo+2]);
    if( (ftst[fo+0]==a) && (ftst[fo+1]==b) && (ftst[fo+2]==c) ){
      //printf("find_cc: %d %d\n",i,i+228);
      return i;
    }
    fo+=4;
  }
}

void convert_cs(char* source, char* target){
  uint32_t si=0, ti=0;
  while(source[si]){
    //printf("%02x %02x %02x\n",source[si],source[si+1],source[si+2]);
    target[ti]=find_cc(source[si],source[si+1],source[si+2]);
    //printf("%d [%d]\n",target[ti],target[ti]+228);
    si+=3;
    target[ti]+=(256-32);
    ++ti;
    target[ti]+='\n';
  }
  target[ti]=0;
}

void print_font_table(){
  uint8_t fts=0;
  uint8_t n=0;
  uint8_t nbytes=0;
  uint32_t ft[128];
  uint32_t sti=0;
  char ftc[5] = {0};
  uint8_t cbu[5];
  puts("TESTING...");
  printf("symcheck\n");
  char* pc;
  for(int i=0;i<7;i++){
    int c=0;
    pc = week_cn[i];
    while(pc[c]){
      n=pc[c];

      if((0b10000000&n)==0b00000000){nbytes=1;}
      if((0b11100000&n)==0b11000000){nbytes=2;}
      if((0b11110000&n)==0b11100000){nbytes=3;}
      if((0b11111000&n)==0b11110000){nbytes=4;}
      //printf("n=%02x (%02b) [%d]",n,(n&0b10000000),nbytes);
      switch(nbytes){
        case 1: ft[fts]=n;c+=1;break;
        case 2: ft[fts]=(pc[c+1]<<8)+(pc[c+0]);c+=2;break;
        case 3: ft[fts]=(pc[c+0]<<16)+(pc[c+1]<<8) +(pc[c+2]);c+=3;break;
        case 4: ft[fts]=(pc[c+0]<<24)+(pc[c+1]<<16)+(pc[c+2]<<8)+(pc[c+3]);c+=4;
      }
      //printf("ft=%d\n",ft[fts]);
      bool dupe=false;
      for(int j=0;j<fts;j++){
        if(ft[j]==ft[fts]){dupe=true;break;}
      }
      if(!dupe){++fts;}
    }
  }

  uint32_t i,k;
  uint32_t temp;

  n=fts;
  for(i = 0; i<n-1; i++) {
    for(k = 0; k<n-1-i; k++) {
      if(ft[k] > ft[k+1]) {
        temp = ft[k];
        ft[k] = ft[k+1];
        ft[k+1] = temp;
      }
    }
  }
  pc=(char*)&ft[0];
  sti=0;
  for(i=0;i<fts;i++){
    //printf("%02d : %d %02x %02x %02x %02x\n",i,ft[i],pc[0],pc[1],pc[2],pc[3]);
    ftc[0]=pc[2];
    ftc[1]=pc[1];
    ftc[2]=pc[0];
    ftc[3]=pc[3];
    printf("S: %02x %02x %02x %s\n",ftc[0],ftc[1],ftc[2],ftc);
    ftst[sti+0]=ftc[0];
    ftst[sti+1]=ftc[1];
    ftst[sti+2]=ftc[2];
    ftst[sti+3]='\n';
    pc+=4;
    sti+=4;
  }
  ftst[sti]=0;
  printf("CHARLIST:\n%s\n",ftst);
  cn_chars=fts;
}

void gpio_callback(uint gpio, uint32_t events) {
    if(events&GPIO_IRQ_EDGE_RISE){
      if(gpio==CSW){        osw=true;      }
      if(gpio==CCLK){        gch='c';      }
      if(gpio==CDT){        gch='d';      }
      gbuf[0]=gbuf[1];
      gbuf[1]=gch;
    }

    if(events&GPIO_IRQ_EDGE_FALL){
      if(gpio==CSW){        sw=true;      }
      if(gpio==CCLK){        gch='C';      }
      if(gpio==CDT) {        gch='D';      }
      gbuf[0]=gbuf[1];
      gbuf[1]=gch;
    }
    if(events&GPIO_IRQ_LEVEL_LOW && gpio==CBUT){
      buttonglass-=BUTTONGLASSC;
      if(buttonglass<=0){
        fire=true;
        buttonglass=BUTTONGLASS;
      }
    }
    if(gbuf[0]=='C'&&gbuf[1]=='D'){tcw=true;}
    if(gbuf[0]=='D'&&gbuf[1]=='C'){tccw=true;}
    if(sw){sw=false;fire=true;}
    if(osw){osw=false;ceasefire=true;}
}



void draw_gfx(){
  uint8_t x1,y1,xt,yt;
  uint8_t x0=120;
  uint8_t y0=120;

  lcd_line(BATX    ,BATY,   BATX+102, BATY, BLUE,1);//top
  lcd_line(BATX    ,BATY+BATYS, BATX+102, BATY+BATYS, BLUE,1);//bottom
  lcd_line(BATX    ,BATY,   BATX,     BATY+BATYS, BLUE,1);//left
  lcd_line(BATX+102,BATY,   BATX+102, BATY+BATYS, BLUE,1);//right
  uint16_t bat = (uint16_t)(((result * conversion_factor)/4.17)*100);
  //printf("bat :  %03d\n",bat);
  if(bat>100){bat=100;}
  lcd_line(BATX+1    ,BATY+1,   BATX+bat-1, BATY+1, WHITE, 1);//top
  lcd_line(BATX+1    ,BATY+2,   BATX+bat-1, BATY+2, WHITE, 1);//top
  lcd_line(BATX+3    ,BATY+3,   BATX+bat-1, BATY+3, WHITE, 1);//top
  if(!usb_loading){
    sprintf(dbuf,"  %02d%%",bat);
  }else{
    sprintf(dbuf,"LOADING",bat);
  }
  lcd_str(94, 12, dbuf, &Font12, WHITE, RED);
  //lcd_str(94, 12, dbuf, &Font34);

  if(t.sec==0||t.sec==30){
    temperature = QMI8658_readTemp();
  }
  //QMI8658_read_mag(mag);
  //printf("mag: %0.2f %0.2f %0.2f\n",mag[0],mag[1],mag[2]);
  //printf("acc_x   = %4.3fmg , acc_y  = %4.3fmg , acc_z  = %4.3fmg\r\n", acc[0], acc[1], acc[2]);
  //printf("gyro_x  = %4.3fdps, gyro_y = %4.3fdps, gyro_z = %4.3fdps\r\n", gyro[0], gyro[1], gyro[2]);
  //printf("tim_count = %d\r\n", tim_count);
  int xi,yi;
  for(uint16_t i=0;i<60;i++){
    xi = (int)(tcos[i*6]*120);
    yi = (int)(tsin[i*6]*120);
    x1 = (uint8_t)x0+xi;
    y1 = (uint8_t)y0+yi;
    if(!(i%5)){
      xi = (int)(tcos[i*6]*110);
      yi = (int)(tsin[i*6]*110);
      lcd_line((uint8_t)x0+xi,(uint8_t)y0+yi, x1, y1, colt[theme_pos]->col_cs5, 1);
    }else{
      xi = (int)(tcos[i*6]*115);
      yi = (int)(tsin[i*6]*115);
      lcd_line((uint8_t)x0+xi,(uint8_t)y0+yi, x1, y1, colt[theme_pos]->col_cs, 1);
    }
  }
  xi = (int)(tcos[t.min*6]*105);
  yi = (int)(tsin[t.min*6]*105);
  x1 = (uint8_t)x0+xi;
  y1 = (uint8_t)y0+yi;
  lcd_line(x0,y0, x1, y1, colt[theme_pos]->col_m, 3);

  int th=(int)t.hour;
  if(th>=12){th-=12;}
  th*=30;
  th+=(int)(t.min>>1);
  xi = (int)(tcos[th]*64);
  yi = (int)(tsin[th]*64);
  x1 = (uint8_t)x0+xi;
  y1 = (uint8_t)y0+yi;
  lcd_line(x0,y0, x1, y1, colt[theme_pos]->col_h, 5);

  if(tseco!=t.sec){
    tseco=t.sec;
    stime = time_us_32();
  }

  uint32_t st = time_us_32();
  st-=stime;
  st=st/100000;
  //printf("%d\n",st);
  if(!analog_seconds){
    // 'jump' seconds
    xi = (int)(tcos[t.sec*6]*114);
    yi = (int)(tsin[t.sec*6]*114);
    x1 = (uint8_t)x0+xi;
    y1 = (uint8_t)y0+yi;
    lcd_line(x0,y0, x1, y1, colt[theme_pos]->col_s, 1);
    lcd_blit((int)(x0-8+tcos[t.sec*6]*102),(int)(y0-8+tsin[t.sec*6]*102),16,16,colt[theme_pos]->alpha,stars[theme_pos]);
  }else{
    // 'analog' seconds
    xi = (int)(tfcos[t.sec*10+st]*114);
    yi = (int)(tfsin[t.sec*10+st]*114);
    x1 = (uint8_t)x0+xi;
    y1 = (uint8_t)y0+yi;
    lcd_line(x0,y0, x1, y1, colt[theme_pos]->col_s, 1);
    lcd_blit((int)(x0-8+tfcos[t.sec*10+st]*102),(int)(y0-8+tfsin[t.sec*10+st]*102),16,16,colt[theme_pos]->alpha,stars[theme_pos]);

  }

  lcd_blit(120-16,120-16,32,32,colt[theme_pos]->alpha, flags[theme_pos]); // center

}


void draw_text(){
  lcd_str(ACCX, ACCY    , "GYR_X = ", &Font12, WHITE,  CYAN);
  lcd_str(ACCX, ACCY+16 , "GYR_Y = ", &Font12, WHITE,  CYAN);
  lcd_str(ACCX, ACCY+32 , "GYR_Z = ", &Font12, WHITE, CYAN);
  lcd_str(ACCX, ACCY+114 ,"ACC_X = ", &Font12, WHITE, CYAN);
  lcd_str(ACCX, ACCY+128, "ACC_Y = ", &Font12, WHITE, CYAN);
  lcd_str(ACCX, ACCY+142, "ACC_Z = ", &Font12, WHITE, CYAN);
  lcd_float(130, ACCY    , acc[0], &Font12,  YELLOW,   WHITE);
  lcd_float(130, ACCY+16 , acc[1], &Font12,  YELLOW,   WHITE);
  lcd_float(130, ACCY+32 , acc[2], &Font12,  YELLOW,  WHITE);
  lcd_float(130, ACCY+114 , gyro[0], &Font12,YELLOW, WHITE);
  lcd_float(130, ACCY+128, gyro[1], &Font12, YELLOW, WHITE);
  lcd_float(130, ACCY+142, gyro[2], &Font12, YELLOW, WHITE);
  lcd_str(70, 194, "TEMP = ", &Font12, WHITE, CYAN);
  lcd_float(120, 194, temperature, &Font12,  YELLOW, WHITE);
  lcd_str(50, 208, "BAT(V)=", &Font16, WHITE, ORANGE);
  lcd_float(130, 208, result * conversion_factor, &Font16, ORANGE, WHITE);

  if(!theme_pos){
    convert_cs(week[theme_pos][t.dotw],cn_buffer);
    lcd_strc(190, 72, cn_buffer, &CNFONT, colors[0], BLACK);
    //printf("cn_buffer: %s\n",cn_buffer);
  }else{
    lcd_str(20, 111, week[theme_pos][t.dotw], &TFONT, colors[0], BLACK);
  }

  sprintf(dbuf,"%02d",t.day);
  lcd_str(DATIX+3*TFW, DATIY, dbuf, &TFONT, colors[1], BLACK);
  lcd_str(DATIX+5*TFW, DATIY, ".", &TFONT, WHITE, BLACK);
  sprintf(dbuf,"%02d",t.month);
  lcd_str(DATIX+6*TFW, DATIY, dbuf, &TFONT, colors[2], BLACK);
  lcd_str(DATIX+8*TFW, DATIY, ".", &TFONT, WHITE, BLACK);
  sprintf(dbuf,"%04d",t.year);
  lcd_str(DATIX+9*TFW, DATIY, dbuf, &TFONT, colors[3], BLACK);

  sprintf(dbuf,"%02d",t.hour);
  lcd_str(HXST, HYST, dbuf, &TFONT, colors[4], BLACK);
  lcd_str(HXST+2*TFW, HYST, ":", &TFONT, WHITE, BLACK);
  sprintf(dbuf,"%02d",t.min);
  lcd_str(HXST+3*TFW, HYST, dbuf, &TFONT, colors[5], BLACK);
  lcd_str(HXST+5*TFW, HYST, ":", &TFONT, WHITE, BLACK);
  sprintf(dbuf,"%02d",t.sec);
  lcd_str(HXST+6*TFW, HYST, dbuf, &TFONT, colors[6], BLACK);
}




int main(void)
{
    stdio_init_all();
    puts("stdio init");
    lcd_init();
    lcd_set_brightness(30);
    puts("lcd init");
    uint8_t* b0 = malloc(LCD_SZ);
    uint32_t o = 0;
    lcd_setimg((uint16_t*)b0);

    colt[0]=&colt1;
    colt[1]=&colt2;
    colt[2]=&colt3;
    colt[3]=&colt4;

    bool o_clk;
    bool o_dt;
    bool o_sw;


    //uint32_t bm = 0b00000000000010110000000000000000;
    gpio_set_irq_enabled_with_callback(CCLK, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(CDT, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CSW, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_dir(CBUT,GPIO_IN);
    gpio_pull_up(CBUT);
    gpio_set_irq_enabled(CBUT, GPIO_IRQ_LEVEL_LOW, true);
    //gpio_pull_up(CBUT);
    rtc_init();
    rtc_set_datetime(&t);
    if(!(t.year%4)){last[2]=29;}else{last[2]=28;}

    set_dcolors();
    set_colors(); // are set from dcolors so set em first
    QMI8658_init();
    printf("QMI8658_init\r\n");

    for(uint16_t i=0;i<360;i++){
      float f = (float)i;
      sincosf(to_rad(f-90),&tsin[i],&tcos[i]);
    }
    float ff=0.0f;
    for(int i=0;i<600;++i){
      sincosf(to_rad(ff-90),&tfsin[i],&tfcos[i]);
      ff+=0.6f;
    }
    print_font_table();


    while(true){
      mcpy(b0,backgrounds[theme_pos],LCD_SZ);
      //lcd_blit(104,54,32,32, 0x0000,ger32);
      //lcd_blit(104,92,32,32, 0x0000,cn32);
      //lcd_blit(104,124,32,32,0x0000,usa32);
      //lcd_blit(104,164,32,32,0x0000,tr32);
      //lcd_display(b0);
      //o++;
      //if(o==THEMES){o=0;}
      //sleep_ms(1000);


      QMI8658_read_xyz(acc, gyro, &tim_count);
      //check if not moving
      if((gyro[0]>-10.0f&&gyro[0]<10.0f)&&(gyro[1]>-10.0f&&gyro[1]<10.0f)&&(gyro[2]>-10.0f&&gyro[2]<10.0f)){
        if(!is_sleeping && cmode==CM_None && !usb_loading){
          screensaver--;
          if(screensaver<=0){
            is_sleeping=true;
            screensaver=SCRSAV;
            lcd_set_brightness(0);
            lcd_sleepon();
          }
        }
      }else{
        if(is_sleeping){
          is_sleeping=false;
          lcd_set_brightness(BRIGHTD);
          lcd_sleepoff();
        }
      }

      if(is_sleeping){
        sleep_ms(1000);
        continue;
      }

      rtc_get_datetime(&t);
      result = adc_read();
      // (v>4.15)?true:false
      usb_loading = ((result * conversion_factor)>=4.15);
      //Paint_DrawImage(gImage_1inch3_1, 0, 0, 240, 240);
      //Paint_DrawImage(earth, 0, 0, 240, 240);
      //Paint_DrawImage(backgrounds[theme_pos], 0, 0, 240, 240);
      //lcd_blit(0,0,240,240,backgrounds[theme_pos]);
      //for(int i=0;i<(240*240*2);i++){          BlackImage[i]=backgrounds[theme_pos][i];        }
      //LCD_1IN28_Display(BlackImage);
      //blit(0,0,240,240,br,BLACK);

      if(fire){
        hgx = acc[0];
        hgy = acc[1];
        if(cmode==CM_None){
          puts("CM_Config");
          cmode=CM_Config;
        }else if(cmode==CM_Config){
          puts("CM_Changepos");
          cmode=CM_Changepos;
          colors[editpos]=editcol;
        }else if(cmode==CM_Changepos){
          puts("CM_Editpos");
          cmode=CM_Editpos;
          colors[editpos]=changecol;
        }else if(cmode==CM_Editpos){
          puts("CM_Changetheme");
          cmode=CM_Changetheme;
          colors[editpos]=dcolors[editpos];
        }else if(cmode==CM_Changetheme){
          puts("CM_None");
          cmode=CM_None;
        }

        fire=false;
      }
      if(cmode == CM_Config){
        float cx = acc[0]-hgx;
        float cy = acc[1]-hgy;
        float r;
        cx/=1000;
        cy/=1000;
        r = sqrt(cx*cx+cy*cy);
        printf("cxy: %0.3f %0.3f  r=%0.3f\n",cx,cy,r);
        cx/=r;
        cy/=r;
        printf("cxy: %0.3f %0.3f\n",cx,cy);
        //Paint_DrawLine(120,120, 120+cy*110, 120-cx*110, CYAN , 3, LINE_STYLE_DOTTED);
        lcd_line(120,120, 120+cy*110, 120-cx*110, CYAN, 4);


      }
      if(cmode==CM_Changepos || cmode==CM_Editpos){
        // wrist-control (arm==x-axis)
        int as = acc[0];
        as-=hgb;
        //printf("hgb: %d as: %d\n",hgb,as);
        if( as > HOURGLASSBORDER || as < -HOURGLASSBORDER ){
          int a = as;
          if(a<0){a*=-1;}
          hourglass -= a;
          //printf("hg: %d\n",hourglass);
          if( hourglass <=0 ){
            hourglass=HOURGLASS;
            if(as>0){tcw=true;}else{tccw=true;}
          }
        }
      }

      if(cmode==CM_Changepos){
        if(tcw){
          colors[editpos]=dcol;
          if(editpos==6){editpos=0;}else{++editpos;}
          colors[editpos]=editcol;
          tcw=false;
        }else if(tccw){
          colors[editpos]=dcol;
          if(editpos==0){editpos=6;}else{--editpos;}
          colors[editpos]=editcol;
          tccw=false;
        }
      }

      if(cmode==CM_Editpos){
        bool set=false;
        if(tcw){
          colors[editpos]=dcolors[editpos];
          switch(editpos){
            case 0: (t.dotw==6)?t.dotw=0:++t.dotw;break;
            case 1: (t.day==last[t.month])?t.day=1:++t.day;break;
            case 2: (t.month==12)?t.month=1:++t.month;break;
            case 3: (t.year==2099)?t.year=2022:++t.year;break;
            case 4: (t.hour==23)?t.hour=0:++t.hour;break;
            case 5: (t.min==59)?t.min=0:++t.min;break;
            case 6: (t.sec==59)?t.sec=0:++t.sec;break;
          }
          colors[editpos]=changecol;
          tcw=false;
          set=true;
        }
        if(tccw){
          colors[editpos]=dcolors[editpos];
          switch(editpos){
            case 0: (t.dotw==0)?t.dotw=6:--t.dotw;break;
            case 1: (t.day==1)?t.day=last[t.month]:--t.day;break;
            case 2: (t.month==1)?t.month=12:--t.month;break;
            case 3: (t.year==2099)?t.year=2022:--t.year;break;
            case 4: (t.hour==0)?t.hour=23:--t.hour;break;
            case 5: (t.min==0)?t.min=59:--t.min;break;
            case 6: (t.sec==0)?t.sec=59:--t.sec;break;
          }
          colors[editpos]=changecol;
          tccw=false;
          set=true;
        }
        if(set){
          rtc_set_datetime(&t);
          if(!(t.year%4)){last[2]=29;}else{last[2]=28;}
        }
      }
      if(cmode==CM_Changetheme){
        blink_counter++;
        if(blink_counter==5){bmode=!bmode;blink_counter=0;}
        //Paint_DrawCircle(120,120,20,bmode?RED:BLUE,2,0);
      }


      if(draw_gfx_first){
        draw_gfx();
        draw_text();
      }else{
        draw_text();
        draw_gfx();
      }
      lcd_display(b0);

#define THRS 12
#define THRLY 30
      int as = acc[1];  // y-axis
      as-=hgy;

      if((as>THRLY) && cmode==CM_Changetheme){
        printf("right\n");
        if(!flagsdelay){
          theme_pos++;
          if(theme_pos==THEMES){theme_pos=0;}
          flagsdelay=FLAGS_DELAY;
          set_dcolors();
          set_colors();
        }else{
          --flagsdelay;
        }
      }
      if((as<-THRLY) && cmode==CM_Changetheme){
        printf("left\n");
        if(!flagsdelay){
          if(theme_pos==0){theme_pos=THEMES;}
          theme_pos--;
          flagsdelay=FLAGS_DELAY;
          set_dcolors();
          set_colors();
        }else{
          --flagsdelay;
        }
      }
      if(gyro[1]>THRS){printf("up\n");} // shake up
      if(gyro[1]<-THRS){printf("down\n");} //shake down
      //printf("%d\n",st/100000);
      //printf("[%d] {as%d} fd=%d\n",theme_pos,as,flagsdelay);

      sleep_ms((analog_seconds)?1:100);

    }
    return 0;
}
