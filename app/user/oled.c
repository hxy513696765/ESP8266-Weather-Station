#include "esp_common.h"
#include "oled.h"
#include "icon.h"
#include "i2c_master.h"

GPIO_ConfigTypeDef test_gpio = 
{
    .GPIO_Pin = RS_PIN | RST_PIN | CS_PIN ,
    .GPIO_Mode = GPIO_Mode_Output,
    .GPIO_Pullup = GPIO_PullUp_DIS,
    .GPIO_IntrType = GPIO_PIN_INTR_DISABLE,
};

void gpio_init(void)
{
    gpio_config(&test_gpio);
//    gpio_output_conf(0,GPIO_Pin_12,0,GPIO_Pin_12);
//    gpio_output_conf(0,GPIO_Pin_15,0,GPIO_Pin_15);
    RS_L;
    CS_L;
}
/**********************************************
// IIC Write Command
**********************************************/
void Write_IIC_Command(uint8 IIC_Command)
{
    RS_L;
    CS_L;
    i2c_master_writeByte(IIC_Command);
    CS_H;
}
/**********************************************
// IIC Write Data
**********************************************/
void Write_IIC_Data(uint8 IIC_Data)
{
    RS_H;
    CS_L;
    i2c_master_writeByte(IIC_Data);
    CS_H;
}
void Initial_LCD(void)
{
    i2c_master_gpio_init();
    printf("IIC Init OK \n");
    
    gpio_init();
    
    RST_L;          //硬件复位低电平复位。
    vTaskDelay(100);    //延时50uS
    RST_H;          //拉高复位引脚电平LCD正常工作。
    
    Write_IIC_Command(0x76);    //软件复位
    Write_IIC_Command(0x3A);    //开启液晶晶振  
    Write_IIC_Command(0x3C); // Set display oFF 

    // SET power
    Write_IIC_Command(0x2F); // set power vc vr vf


    // VA_set
    Write_IIC_Command(0x33);  // BIAS
    Write_IIC_Command(0xb1); // SET CT
    Write_IIC_Command(0xdF); //VOP=12.5V，效果最佳

    // SET FR
    Write_IIC_Command(0xb2);
    Write_IIC_Command(0x90); //Write_IIC_Command(0xE3);
    Write_IIC_Command(0x01); //Write_IIC_Command(0x01); 

    // SET duty
    Write_IIC_Command(0xa4); // SET duty
    Write_IIC_Command(0x90); // SET duty
    //***************************
    Write_IIC_Command(0x68); // SHL ADC EON REV =0 
    
//    delay_ms(10);
    Write_IIC_Command(0x88);    //IST Command entry ,It need 4 times entry
    Write_IIC_Command(0x88);
    Write_IIC_Command(0x88);
    Write_IIC_Command(0x88);
    Write_IIC_Command(0x60);    //设置液晶的显示方向
//    delay_ms(10);
    Write_IIC_Command(0xe3);
    
    Clear_lcd();
}

void Clear_lcd(void)
{
    uint8_t i,j;
    uint32_t dis_count = 0;

    for(i = 0;i < 64;i++)
    {
        Write_IIC_Command(0x10 | (i >> 4)); // H_byte AY ADD
        Write_IIC_Command(i & 0x0F); // L_byte AY ADD
        Write_IIC_Command(0xc0); // SET AX Add
        
        for(j = 0;j < 16;j++)
        {
            Write_IIC_Data(0x00);
            dis_count++;
        }
    }
    Write_IIC_Command(0x3d); // Set display on     
}

/******************************************
// picture
******************************************/
void Picture_show(uint8_t x,uint8_t y,uint8_t with,uint8_t high,const uint8_t DisplayData[])
{
    uint8_t i,j;
    uint32_t dis_count = 0;
    x = x % 16;
    y = y % 64;
    with = with / 8;
    high = high % 65;
//    for(i = 0;i < 64 - y;i++)
    for(i = 0;i < high;i++)
    {
        Write_IIC_Command(0x10 | ((i + y) >> 4)); // H_byte AY ADD
        Write_IIC_Command((i + y) & 0x0F); // L_byte AY ADD
        Write_IIC_Command(0xc0 + x); // SET AX Add
        
//        for(j = 0;j < 16 - x;j++)       
        for(j = 0;j < with;j++)
        {
            Write_IIC_Data(*(DisplayData + dis_count));
            dis_count++;
        }
    }
    Write_IIC_Command(0x3d); // Set display on  
}

void LCD_print(uint8_t x,uint8_t y,char *p_string)
{
    uint8_t loop = 0,scan_loop=0,font_index = 0;
    uint8_t y_loop;//,x_offset = 0,y_offset = 0;
//    x_offset = x;
//    y_offset = y;
    while(*(p_string + loop))
    {
        for(scan_loop = 0;scan_loop < FONT_NUM;scan_loop++)
        {
            if(*(p_string + loop) == word_string[scan_loop].string)
            {
                font_index = scan_loop;
                break;
            }
        }
        
        for(y_loop = 0;y_loop < 16;y_loop++)
        {
            Write_IIC_Command(0x10 | ((y_loop + y) >> 4)); // H_byte AY ADD
            Write_IIC_Command((y_loop + y) & 0x0F); // L_byte AY ADD
            Write_IIC_Command(0xc0 + x + loop); // SET AX Add
            
//            Write_IIC_Data(0xFF);
            Write_IIC_Data(*(word_string[font_index].Font_tab + y_loop));
        }      
         
        loop++;
    }
    
    Write_IIC_Command(0x3d); // Set display on     
}

void Big_print(uint8_t x,uint8_t y,char *p_string)
{
    uint8_t loop = 0,scan_loop=0,font_index = 0;
    uint8_t y_loop,x_offset;//,x_offset = 0,y_offset = 0;
//    x_offset = x;
//    y_offset = y;
    while(*(p_string + loop))
    {
        for(scan_loop = 0;scan_loop < BIG_NUM;scan_loop++)
        {
            if(*(p_string + loop) == Big_font_tab[scan_loop].string)
            {
                font_index = scan_loop;
                break;
            }
        }
        
        for(y_loop = 0;y_loop < 32;y_loop++)
        {
            Write_IIC_Command(0x10 | ((y_loop + y) >> 4)); // H_byte AY ADD
            Write_IIC_Command((y_loop + y) & 0x0F); // L_byte AY ADD
            for(x_offset = 0;x_offset < 2;x_offset++)
            {                
                Write_IIC_Command(0xc0 + x + loop*2 + x_offset); // SET AX Add
                Write_IIC_Data(*(Big_font_tab[font_index].Font_tab + x_offset + y_loop*2));
            }

        }      
         
        loop++;
    }
    
    Write_IIC_Command(0x3d); // Set display on     
}