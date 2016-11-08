/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_common.h"
#include "gpio.h"
#include "i2c_master.h"
#include "oled.h"
#include "icon.h"
#include "cJSON.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"
#include "espressif/esp_softap.h"

//#define server_ip "192.168.101.142"
//#define server_port 9669


#define DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID 			"122475" //model ID
#define debug_printf
#undef  debug_printf

#define DEFAULT_LAN_PORT 	12476

GPIO_ConfigTypeDef smart_set_pin ={
    .GPIO_Pin        = GPIO_Pin_3,               /**< GPIO pin */
    .GPIO_Mode       = GPIO_Mode_Input,          /**< GPIO mode */
    .GPIO_Pullup     = GPIO_PullUp_EN,           /**< GPIO pullup */
    .GPIO_IntrType   = GPIO_PIN_INTR_DISABLE//GPIO_PIN_INTR_NEGEDGE     /**< GPIO interrupt type */
};

GPIO_ConfigTypeDef led_pin ={
    //GPIO15-->R_LED;GPIO13-->B_LED;GPIO12-->G_LED
    .GPIO_Pin        = GPIO_Pin_2,//GPIO_Pin_15 | GPIO_Pin_13 | GPIO_Pin_12,               //< GPIO pin 
    .GPIO_Mode       = GPIO_Mode_Output,          //< GPIO mode 
    .GPIO_Pullup     = GPIO_PullUp_DIS,           //< GPIO pullup 
    .GPIO_IntrType   = GPIO_PIN_INTR_DISABLE      //< GPIO interrupt type 
};
/**/

LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8  lan_buf[200];
uint16 lan_buf_len,clear_rest;
uint8  udp_sent_cnt = 0;

const airkiss_config_t akconf =
{
	(airkiss_memset_fn)&memset,
	(airkiss_memcpy_fn)&memcpy,
	(airkiss_memcmp_fn)&memcmp,
	0,
};

uint8_t dis_flag = 1;
uint8_t init_flag = 1;

typedef struct 
{
	int32_t day;
	int32_t night;
	int32_t min;
	int32_t max;
	int32_t morn;
	int32_t eve;

}xTemperature_t;
#define STA_SIZE			(16)
typedef struct
{
	int32_t id;
	int8_t wmain[STA_SIZE];
	int8_t wdes[STA_SIZE];
	int8_t wicon[STA_SIZE];
}xWeather_t;

typedef struct
{
	int32_t dt;					/* µ±«∞ ±º‰ */
	int32_t pressure;
	int32_t humidity;
	int32_t speed;
	int32_t deg;
	int32_t clouds;
	xTemperature_t temperature;
	xWeather_t weather;
}xWeatherList_t;

#define BUFFER_SIZE						(1024)

int32_t vGetRawData(int8_t *pbuf,const int32_t maxsiz)
{
	int32_t temp=0;
	int32_t recbytes=0;
	int32_t buf_index=0;
	int8_t *c=NULL;
	int8_t buffer[128];
	int8_t rec_count;//÷ÿ ‘¥Œ ˝
	struct sockaddr_in remote_ip;
	const int32_t server_port=80;
	const static int8_t *serverurl="api.openweathermap.org";
	const int8_t request[256]="GET /data/2.5/forecast/daily?q=Shenzhen&mode=json&units=metric&cnt=3&appid=d9176758eea113e3813f472f387439e2 HTTP/1.1\nHOST: api.openweathermap.org\nCache-Control: no-cache\n\n\n";
//    const int8_t request[256]="GET /data/2.5/forecast/daily?q=isachsen&mode=json&units=metric&cnt=3&appid=d9176758eea113e3813f472f387439e2 HTTP/1.1\nHOST: api.openweathermap.org\nCache-Control: no-cache\n\n\n";
	const struct hostent *pURL=gethostbyname(serverurl);
	const int socketfd=socket(AF_INET, SOCK_STREAM, 0);
	
 #ifdef debug_printf
    printf(" %s Run... \n", __func__);
 #endif

	
	if (socketfd<0)
	{
		printf("C > socket fail!\n");
		close(socketfd);
		return -1;
	}
	bzero(&remote_ip, sizeof(struct sockaddr_in));
	remote_ip.sin_family = AF_INET;
	remote_ip.sin_addr.s_addr = *((unsigned long*)pURL->h_addr_list[0]);//inet_addr(server_ip);
	remote_ip.sin_port = htons(server_port);
	if (0 != connect(socketfd, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr)))
	{
		close(socketfd);
		printf("C > connect fail!\n");
		vTaskDelay(1);
		return -1;
	}
	if (write(socketfd, request, strlen(request) + 1) < 0)
	{
		close(socketfd);
		printf("C > send fail\n");		
		vTaskDelay(1);
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	//memset(pbuf,0,maxsiz);
	vTaskDelay(10);
	//printf("-------------------Read-------------------\n");
	while ((recbytes = read(socketfd , buffer, sizeof(buffer)-1)) > 0)					/* Ω” ’∆ ºÕ∑ */
	{
		//buffer[recbytes] = 0;
		c=strstr(buffer,"{");
		if (NULL==c)
		{
			memset(buffer,0,sizeof(buffer));
			vTaskDelay(1);
			continue;
		}
		buf_index=recbytes-(c-buffer);
		memcpy(pbuf,c,recbytes-(c-buffer));
		
		vTaskDelay(1);
		//c-buffer;
		//printf("Start:%u,Offset:%u,%u\n",buffer,c,c-buffer);
		//printf("-------------------Index-------------------\n");
		//printf("Offset:%ld\n",(long)(c-buffer));
		//printf("%s\n", pbuf);
		break;
		//memset(buffer,0,sizeof(buffer));
	}
	if (recbytes<1)
	{
		close(socketfd);		
		vTaskDelay(1);
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	rec_count = 0;
	while ((recbytes = read(socketfd , buffer, sizeof(buffer)-1)) > 0)					/* Ω” ’ £œ¬≤ø∑÷ ˝æ› */
	{
		
		if (recbytes+buf_index>maxsiz)
			temp=maxsiz-buf_index;
		else
			temp=recbytes;
		//temp
		memcpy(pbuf+buf_index,buffer,temp);
		buf_index=buf_index+temp;
		if (temp<recbytes)
		{
			//printf("Full\n");
			break;																		/* ª∫≥Â«¯¬˙ */
		}
		//buffer[recbytes] = 0;
		//printf("%s\n", buffer);
		//c=strstr(buffer,"{");
		//if (NULL==c)
		//{
			//memset(buffer,0,sizeof(buffer));
			//vTaskDelay(100 / portTICK_RATE_MS);
		//	continue;
		//}
		//memset(buffer,0,sizeof(buffer));
		vTaskDelay(1);
		
		clear_rest = 0;
		if(rec_count > 50)	//÷ÿ ‘50¥Œ
			return -1;
		
		rec_count++;
	}
	temp=maxsiz;
	while (('}'!=pbuf[temp]) && (temp >= 0))
	{
		clear_rest = 0;
		pbuf[temp]='\0';
		--temp;		
		vTaskDelay(1);
	}
	/*printf("-------------------Read-------------------\n");
	printf("%s\n", pbuf);
	printf("-------------------ReadEnd----------------\n");*/
	close(socketfd);
	return 0;
}



int32_t vGetWeatherList(const int8_t *jsStr,xWeatherList_t *weatherList,const uint8_t dt)
{
	int32_t cpySiz=0;
	cJSON *jsTmp=NULL;			//¡Ÿ ±±‰¡ø
	cJSON *jsonRoot=cJSON_Parse(jsStr);
	cJSON *jsWeatherRoot=NULL;
	cJSON *jsWeatherList=NULL;
	cJSON *jsTemp=NULL;
	cJSON *jsWeather=NULL;
	
#ifdef debug_printf  
    printf(" %s Run... \n", __func__);
#endif
	
	/***************************************************************/
	if (cJSON_GetArraySize(jsonRoot)<5)
		return -1;
	/***************************************************************/
	jsWeatherRoot=cJSON_GetObjectItem(jsonRoot, "list");		/* ªÒ»°À˘”–ÃÏ∆¯–≈œ¢ */
	if (dt+1>cJSON_GetArraySize(jsWeatherRoot))
		return -1;
	jsWeatherList=cJSON_GetArrayItem(jsWeatherRoot,dt);			/* ªÒ»°µ⁄nÃÏÃÏ∆¯–≈œ¢ */
	jsTemp=cJSON_GetArrayItem(jsWeatherList,1);					/* Œ¬∂» ˝æ› */
	jsTmp=cJSON_GetArrayItem(jsWeatherList,4);					/* ÃÏ∆¯ ˝æ› */
	//jsWeather=cJSON_GetArrayItem(jsWeatherList,4);					/* ÃÏ∆¯ ˝æ› */
	jsWeather=cJSON_GetArrayItem(jsTmp,0);
	/***************************************************************/									/*  ±º‰ */
	weatherList->dt=cJSON_GetArrayItem(jsWeatherList,0)->valueint;
	weatherList->pressure=cJSON_GetArrayItem(jsWeatherList,2)->valueint;	/* ∆¯—π */
	weatherList->humidity=cJSON_GetArrayItem(jsWeatherList,3)->valueint;	/*  ™∂» */
	weatherList->speed=cJSON_GetArrayItem(jsWeatherList,5)->valueint;
	weatherList->deg=cJSON_GetArrayItem(jsWeatherList,6)->valueint;
	weatherList->clouds=cJSON_GetArrayItem(jsWeatherList,7)->valueint;
	//weatherList->rain=cJSON_GetArrayItem(jsWeatherList,3)->valueint;
	/**************************ÃÏ∆¯ ˝æ›*********************************/
	weatherList->weather.id=cJSON_GetObjectItem(jsWeather,"id")->valueint;
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"main")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wmain,cJSON_GetObjectItem(jsWeather,"main")->valuestring);
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"description")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wdes,cJSON_GetObjectItem(jsWeather,"description")->valuestring);
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"icon")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wicon,cJSON_GetObjectItem(jsWeather,"icon")->valuestring);
	/**************************Œ¬∂» ˝æ›*********************************/
	weatherList->temperature.day=cJSON_GetObjectItem(jsTemp,"day")->valueint;
	weatherList->temperature.night=cJSON_GetObjectItem(jsTemp,"night")->valueint;
	weatherList->temperature.min=cJSON_GetObjectItem(jsTemp,"min")->valueint;
	weatherList->temperature.max=cJSON_GetObjectItem(jsTemp,"max")->valueint;
	weatherList->temperature.morn=cJSON_GetObjectItem(jsTemp,"morn")->valueint;
	weatherList->temperature.eve=cJSON_GetObjectItem(jsTemp,"eve")->valueint;
	cJSON_Delete(jsWeatherRoot);
	cJSON_Delete(jsWeatherList);
	cJSON_Delete(jsWeather);
	cJSON_Delete(jsTemp);
	cJSON_Delete(jsonRoot);
	return 0;
}

void chang_valu(int32_t temp_valu,char *dis_str)
{

#ifdef debug_printf	  
    printf(" %s Run... \n", __func__);
#endif

    if(temp_valu > 0){            
        if(temp_valu < 10)
            sprintf(dis_str,"  %d^",temp_valu);
        else
            sprintf(dis_str," %d^",temp_valu);
    }
    else{       
//        temp_valu = -1 * temp_valu;
        
        if(temp_valu > -10)
            sprintf(dis_str," %d^",temp_valu);
        else
            sprintf(dis_str,"%d^",temp_valu);
    }    
}

int8_t get_icondata(char *ico_str)
{
    int8_t ico_dat = 0,ico_offset = 0;

#ifdef debug_printf	  
    printf(" %s Run... \n", __func__);
#endif
    
    ico_dat = (*ico_str - '0') * 10 + (*(ico_str + 1) - '0');
    switch(ico_dat)
    {
        case 1:     //ÃÏ«Á∫Õ‘¬¡¡
            if(*(ico_str + 2) == 'd')
                ico_offset = 3;    //Ã´—Ù
            else
                ico_offset = 7;    //‘¬¡¡             
        break;
        
        case 2:    //∞◊ÃÏ“ı∫ÕÕÌ…œ“ı
            if(*(ico_str + 2) == 'd')
                ico_offset = 1;    //∞◊ÃÏ“ı
            else
                ico_offset = 4;    //ÕÌ…œ“ı         
        break;
        
        case 3:    //‘∆
                ico_offset = 8;                        
        break;
        
        case 4:    //‘∆
                ico_offset = 8;    
        break;
        
        case 9:    //”Í
                ico_offset = 6;    
        break;
        
        case 10:    //”Í
                ico_offset = 6;    
        break;
        
        case 11:    //¥Ú¿◊
                ico_offset = 2;    
        break;
        
        case 13:    //—©
                ico_offset = 5;    
        break;
        
        default :ico_offset = 0;
        break;            
    }
    
    return ico_offset;
}


#define DT_CNT				(3)				//nÃÏ ˝æ›
void vTaskNetwork(void *pvParameters)
{
	int32_t i,tempvalu = 0;
	xWeatherList_t weatherList[DT_CNT];
	int8_t *pbuf=NULL;
	pbuf=(int8_t *)zalloc(BUFFER_SIZE+1);
    char *dis_str = "   ^";
    char *ico_str;
    char *main_weath = "        ";
    int  ico_dat;
    int  ico_offset = 0;
    uint8_t LCD_clear = 1;
	
    uint8_t dis_count = 0;
    char *dis_cont= "        ";
    uint8_t loop = 0;
	uint8_t dis_cnt,err_cnt,net_errcnt;
	uint8 pwm_tim,blink_loop;
	gpio16_output_set(0);

//	printf("\n%s Running...\n",__func__);

	
	if (NULL==pbuf)
	{
        LCD_print(0,0, "ARM Zalloc ERROR");
        
		while(1)
		{
			Clear_lcd(); 
			LCD_print(0,50,"ARM Zalloc ERROR");
			vTaskDelay(5000);			
            system_restart();
		}
	}
	// ß∞‹¥¶¿Ì	
	
    while (1)
	{
		net_errcnt=1;		  
#ifdef debug_printf
    	printf("\n %s Run... \n", __func__);
#endif
		
		clear_rest = 0;
		
		//÷ª”–WiFi¡¨Ω”¡À≤≈ƒ‹Ω¯–– ˝æ›◊•»°
		if(wifi_station_get_connect_status() == STATION_GOT_IP)
		{
			init_flag = 0;
			err_cnt = 0;
			memset(pbuf,'\0',BUFFER_SIZE+1);
			//printf("%s\n",__func__);
			if (0!=vGetRawData(pbuf,BUFFER_SIZE))
			{				
				net_errcnt=0;
	 			Clear_lcd(); 
				//Big_print(7,0," 	");
				//Picture_show(5,0,48,48,lcd_show);
				//Õ¯“≥ªÒ»° ˝æ›≥ˆ¥Ì
				Picture_show(0,0,128,33,net_erro);
				LCD_print(3,10,"===");
				LCD_print(9,10,"=/=");
				LCD_print(0,34,"Network Timeout ");
				LCD_print(0,50,"Please Checking.");
				
				printf("GetRawData Error\n");
				continue;
			}
			for (i=0;i<DT_CNT;++i)
			{
				if (0!=vGetWeatherList(pbuf,weatherList+i,i))
				{
					net_errcnt=0;
					continue;
				}
			}
			
#if 0
			printf("\n-------------------Vals-------------------\n");
			printf("Dt:%d\n",weatherList[0].dt);
			printf("Speed:%d\n",weatherList[0].speed);
			printf("Deg:%d\n",weatherList[0].deg);
			printf("Clouds:%d\n",weatherList[0].clouds);
			printf("Pressure:%d\n",weatherList[0].pressure);
			printf("Humidity:%d\n",weatherList[0].humidity);
			
			printf("TempDay:%d\n",weatherList[0].temperature.day);
			printf("TempNight:%d\n",weatherList[0].temperature.night);
			printf("TempMin:%d\n",weatherList[0].temperature.min);
			printf("TempMax:%d\n",weatherList[0].temperature.max);
			printf("TempMorn:%d\n",weatherList[0].temperature.morn);
			printf("TempEve:%d\n",weatherList[0].temperature.eve);
			
			printf("WeatherId:%d\n",weatherList[0].weather.id);
			printf("WeatherMain:%s\n",weatherList[0].weather.wmain);
			printf("WeatherDes:%s\n",weatherList[0].weather.wdes);
			printf("WeatherIcon:%s\n",weatherList[0].weather.wicon);
			
						   
#endif
	
#if 0
			ico_dat = (*ico_str - '0') * 10 + (*(ico_str + 1) - '0');
	  
			if(tempvalu > 0){			 
				if(tempvalu < 10)
					sprintf(dis_str,"  %d^",tempvalu);
				else
					sprintf(dis_str," %d^",tempvalu);
			}
			else{		
				tempvalu = -1 * tempvalu;
				
				if(tempvalu < 10)
					sprintf(dis_str," %d^",tempvalu);
				else
					sprintf(dis_str,"%d^",tempvalu);
			}
#endif        

			if(net_errcnt == 1)
			{
				//LCD_clear = LCD_clear % 4;			

				if(LCD_clear == 1)
				{
					dis_flag = 1;
					ico_str = weatherList[0].weather.wicon;
					
					if(*(ico_str + 2) == 'd')	//≈–∂œ «∞◊ÃÏªπ «ÕÌ…œ
						tempvalu = weatherList[0].temperature.day;
					else
						tempvalu = weatherList[0].temperature.night;
				
					chang_valu(tempvalu,dis_str);
					Clear_lcd();
					
					Big_print(7,0,dis_str);
					Big_print(14,0,"C");
					
					ico_offset = get_icondata(ico_str); 	   
					Picture_show(1,8,48,48,lcd_show + ico_offset* 288);
					
					sprintf(main_weath,"%s",weatherList[0].weather.wmain);
					LCD_print(9,32,main_weath); 					

					for(dis_cnt = 0;dis_cnt<32;dis_cnt++)
					{
						clear_rest = 0;					
						dis_count = dis_count % 4;
						dis_cont[0] = '[';
						dis_cont[1] = ']';
						dis_cont[6] = '{';
						dis_cont[7] = '}';
						for(loop = 2;loop < 6;loop++)
						{
							*(dis_cont + loop) = '-';
						}
						
						*(dis_cont + dis_count+2) = '>';			  
						*(dis_cont + 5 - dis_count) = '<';
					
					
						LCD_print(8,48,dis_cont);
						
						dis_count++;
						
						pwm_tim++;
						pwm_tim = pwm_tim % 16;
						if(pwm_tim <= 5)
							gpio16_output_set(0);
						else
							gpio16_output_set(1);
					
						vTaskDelay(40);
					
					}

					LCD_clear = 0;
				}
				else
				{	 
					dis_flag = 0;
					err_cnt = 0;
					ico_str = weatherList[1].weather.wicon;
					
					if(*(ico_str + 2) == 'd')	//≈–∂œ «∞◊ÃÏªπ «ÕÌ…œ
						tempvalu = weatherList[1].temperature.day;
					else
						tempvalu = weatherList[1].temperature.night;
					
					chang_valu(tempvalu,dis_str);
					Clear_lcd();
					
		//			  LCD_print(0,0,"ab");	  //œ‘ æ°∞√˜°±
		//			  LCD_print(0,16,"ef");   //œ‘ æ°∞ÃÏ°±		  
					
					ico_offset = get_icondata(ico_str); 	   
					Picture_show(1,0,48,48,lcd_show + ico_offset* 288);   
					
					sprintf(dis_str,"%sC",dis_str);
					LCD_print(1,48,dis_str);   //œ‘ æŒ¬∂»÷µ  
					
					ico_str = weatherList[2].weather.wicon;
					
					if(*(ico_str + 2) == 'd')	//≈–∂œ «∞◊ÃÏªπ «ÕÌ…œ
						tempvalu = weatherList[2].temperature.day;
					else
						tempvalu = weatherList[2].temperature.night;
					
					chang_valu(tempvalu,dis_str);
					
		//			  LCD_print(8,0,"cd");	  //œ‘ æ°∞√˜°±
		//			  LCD_print(8,16,"ef");   //œ‘ æ°∞ÃÏ°±		  
					
					ico_offset = get_icondata(ico_str); 	   
					Picture_show(9,0,48,48,lcd_show + ico_offset* 288);   
					
					sprintf(dis_str,"%sC",dis_str);
					
					LCD_print(9,48,dis_str);   //œ‘ æŒ¬∂»÷µ

					for(blink_loop = 0;blink_loop < 20;blink_loop++)
					{
						clear_rest = 0;
						pwm_tim++;
						pwm_tim = pwm_tim % 9;
						if(pwm_tim <= 3)
							gpio16_output_set(0);
						else
							gpio16_output_set(1);
						
						vTaskDelay(25);
					}

					
					LCD_clear = 1;
				}
				
				//LCD_clear++;		

			}
		}
		else
		{			
			clear_rest = 0;
			if((err_cnt == 240) && (init_flag == 0))
			{				
				//ƒ£øÈ»Áπ˚30S√ª”–¡¨Ω”µΩ¬∑”…æÕΩ¯––≥¨ ±≥ˆ¥Ìœ‘ æ
	 			Clear_lcd(); 
				//Big_print(7,0," 	");
				//Picture_show(5,0,48,48,lcd_show);
				Picture_show(0,0,128,33,net_erro);
				LCD_print(3,10,"=/=");
				LCD_print(9,10,"===");
				LCD_print(0,34,"Network Error   ");
				LCD_print(0,50,"Not Connected...");	
				err_cnt = 80;
			}
			if(init_flag == 0)
				err_cnt++;

			pwm_tim++;
			pwm_tim = pwm_tim % 3;
			if(pwm_tim <= 1)
				gpio16_output_set(0);
			else
				gpio16_output_set(1);
			
			vTaskDelay(15);
		}
		
		vTaskDelay(1);
    }
	free(pbuf);	
    vTaskDelete(NULL);				
    system_restart();
}


LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
	uint16 i;
	airkiss_lan_ret_t ret;
	
	if ((udp_sent_cnt++) >30) {
		udp_sent_cnt = 0;
		os_timer_disarm(&ssdp_time_serv);//s
		//return;
	}

	ssdp_udp.remote_port = DEFAULT_LAN_PORT;
	ssdp_udp.remote_ip[0] = 255;
	ssdp_udp.remote_ip[1] = 255;
	ssdp_udp.remote_ip[2] = 255;
	ssdp_udp.remote_ip[3] = 255;
	lan_buf_len = sizeof(lan_buf);
	ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
		DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
	if (ret != AIRKISS_LAN_PAKE_READY) {
		os_printf("Pack lan packet error!");
		return;
	}
	
	ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
	if (ret != 0) {
		os_printf("UDP send error!");
	}
	os_printf("Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
	uint16 i;
	remot_info* pcon_info = NULL;
		
	airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
	airkiss_lan_ret_t packret;
	
	switch (ret){
	case AIRKISS_LAN_SSDP_REQ:
		espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
		os_printf("remote ip: %d.%d.%d.%d \r\n",pcon_info->remote_ip[0],pcon_info->remote_ip[1],
			                                    pcon_info->remote_ip[2],pcon_info->remote_ip[3]);
		os_printf("remote port: %d \r\n",pcon_info->remote_port);
      
        pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
		memcpy(pssdpudpconn.proto.udp->remote_ip,pcon_info->remote_ip,4);
		ssdp_udp.remote_port = DEFAULT_LAN_PORT;
		
		lan_buf_len = sizeof(lan_buf);
		packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
			DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
		
		if (packret != AIRKISS_LAN_PAKE_READY) {
			os_printf("Pack lan packet error!");
			return;
		}

		os_printf("\r\n\r\n");
		for (i=0; i<lan_buf_len; i++)
			os_printf("%c",lan_buf[i]);
		os_printf("\r\n\r\n");
		
		packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
		if (packret != 0) {
			os_printf("LAN UDP Send err!");
		}
		
		break;
	default:
		os_printf("Pack is not ssdq req!%d\r\n",ret);
		break;
	}
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
	ssdp_udp.local_port = DEFAULT_LAN_PORT;
	pssdpudpconn.type = ESPCONN_UDP;
	pssdpudpconn.proto.udp = &(ssdp_udp);
	espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
	espconn_create(&pssdpudpconn);

	os_timer_disarm(&ssdp_time_serv);
	os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
	os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{	  
    printf(" %s Run... \n", __func__);
    switch(status) {
        case SC_STATUS_WAIT:
            printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            printf("SC_STATUS_GETTING_SSID_PSWD\n");
            sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;
	
	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
                uint8 phone_ip[4] = {0};

                memcpy(phone_ip, (uint8*)pdata, 4);
                printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
            } else {
            	//SC_TYPE_AIRKISS - support airkiss v2.0
				airkiss_start_discover();
			}
            smartconfig_stop();
            break;
    }
	
}

void ICACHE_FLASH_ATTR
smartconfig_task(void *pvParameters)
{	  
    printf(" %s Run... \n", __func__);
	
    smartconfig_start(smartconfig_done);
//	Clear_lcd(); 
//    LCD_print(0,26,"--SMARTCONFIG--");   
//	printf(" SMARTCONFIG \n");
    vTaskDelete(NULL);
}

//¥À»ŒŒÒŒ™WiFi∞¥º¸ºÏ≤‚£¨smartconfig∞¥º¸≥§∞¥20“‘…œæÕª·÷ÿ–¬∏¥ŒªΩ¯»ÎµΩsmartconfigƒ£ Ω
void 
key_setwifi(void *pvParameters)
{
    uint32 get_keyval;
	uint8 loop_count,led_count;
   
    while(1)
    {
		  
//    	printf(" %s Run... \n", __func__);
		
        get_keyval = gpio_input_get();
//        printf(" %00000008x \n",get_keyval);
        if((get_keyval & 0x00000008) == 0) 
        {             
            vTaskDelay(10); 
            loop_count++;
        }
        else
        {
            loop_count = 0;
        }
        
        if((loop_count > 40) || (clear_rest > 500))
        {
            loop_count = 0;
			if(clear_rest)
            	printf(" clear_rest Rest System..\n");
			else
            	printf(" loop_count Rest System..\n");
            system_restart();
        }
		


		
//	printf(" Net 0x%02x \n",wifi_station_get_connect_status());
//        printf(" RUN...\n");
        
        vTaskDelay(10);
		led_count++;
		led_count = led_count % 31;
		if(led_count == 10)
		{
			clear_rest++;
			GPIO_OUTPUT(GPIO_Pin_2,0);
		}
		else
        	GPIO_OUTPUT(GPIO_Pin_2,1);
    }
    vTaskDelete(NULL);
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;
  
    printf(" %s Run... \n", __func__);

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

/**
*******************************************************************************
 * @brief       ËæìÂÖ•ÂàùÂßãÂåñÂáΩÊï∞
 * @param       [in/out]  void
 * @return      void
 * @note        None
 *******************************************************************************
 */
static void drv_Input_Init( void )
{      
    printf(" %s Run... \n", __func__);
	
    gpio_config(&smart_set_pin);  
	gpio16_output_conf();
    gpio_config(&led_pin);    
 
    printf(" gpio_config Run... \n");   
    printf(" %s Run... \n", __func__);
    
}


/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_init(void)
{
//    struct station_info *wifi_inf;

	init_flag = 0 ;
	char hostname[]="W-Station";

    uart_init_new();				//≥ı ºªØ¥Æø⁄
    wifi_set_opmode(STATION_MODE);  
	//–ﬁ∏ƒhostname√˚◊÷
	wifi_station_set_hostname(hostname);
    
    drv_Input_Init();      			//≥ı ºªØ GPIO
    printf("SDK version:%s\n", system_get_sdk_version());

    Initial_LCD();
    printf("Initial_LCD OK \n");

	//»Áπ˚ø™ª˙”–ºÏ≤‚µΩ¡¨Ω”µΩGPIO4…œµƒ∞¥º¸∞¥œ¬æÕΩ¯»ÎµΩSmartconfigƒ£ Ω
    if((gpio_input_get() & 0x00000008) == 0)
    {
		init_flag = 1;
     /* need to set opmode before you set config */
        wifi_set_opmode(STATION_MODE);
	 	Clear_lcd(); 		
		
		LCD_print(0,26,"________________");	
		Picture_show(0,0,128,33,net_erro+528);		
			
		LCD_print(0,43,"________________");
		LCD_print(0,42,"> Smart Config <");
		
		printf(" SMARTCONFIG \n");
		while(((gpio_input_get() & 0x00000008) == 0))
		{
			vTaskDelay(10);
		}

		//ø…“‘≤‚ ‘“ªœ¬Œ¢–≈µƒarikiss
        //smartconfig_set_type(SC_TYPE_ESPTOUCH);
		//ÃÌº”smartconfig»ŒŒÒ
        xTaskCreate(smartconfig_task, "smartconfig_task", 256, NULL, 2, NULL);
    }
	else
	{
		Picture_show(0,0,128,33,net_erro+528);
		
		LCD_print(0,34,"WiFi Connection ");		
		LCD_print(0,50,"Please Wait.....");
	}

	//ÃÌº”key_setwifi∞¥º¸ºÏ≤‚»ŒŒÒ”√¿¥ºÏ≤‚ «∑Ò«–ªªµΩsmartconfigƒ£ Ω
	xTaskCreate(key_setwifi , "key_setwifi", 256,  NULL, 2, NULL);
	//ÃÌº”vTaskNetwork»ŒŒÒ‘≠¿¥ªÒ»°ÃÏ∆¯ ˝æ›∫Õœ‘ æÃÏ∆¯ ˝æ›µΩÂÂÂÂOLED
	xTaskCreate(vTaskNetwork, "tskNetwork" , 2256, NULL, 2, NULL);
  	
    
}

