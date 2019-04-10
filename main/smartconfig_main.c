/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#define GIOVANNI 1
#define BLINK_GPIO 15
#define BUTTON_IO 0
//#define LED_BUILTIN 27


//#define BUTTON_IO1			BUTTON_IO 
//18
/* ONLY INPUT PIN */
#define BUTTON_IO1			18
#define BUTTON_IO2         	19
#define BUTTON_IO3         	21
#define LIMIT_SWITCH    	22
#define WATER_LEVEL         BUTTON_IO //23

/* OUTPUT PIN */
#define WATER_LEVEL_LED     27
#define LIMIT_SWITCH_LED    12
#define BUTTON1_LED         25
#define BUTTON2_LED         26
#define BUTTON3_LED         14

#define FRIZ_LIEVE			(4)  //secondi
#define FRIZ_MEDIA			(6)
#define FRIZ_MOLTO			(8)
#define GAS_COMPLETO		(1000)



#define ESP_INTR_FLAG_DEFAULT 0
#define PIN_SDA 5
#define PIN_SCL 4


#include "u8g2_esp32_hal.h"
#include "icons.h"
#include "hx711.h"

#include <string.h>
#include <stdlib.h>

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/types.h> /* getpid() */
#include <unistd.h> /* getpid() */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOSConfig.h" 
#include "freertos/semphr.h"

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"





//spiffs example
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
//from tutorial master spiffs
#include "esp_vfs.h"
#include "esp_vfs_fat.h"


// da esp32-webserver-master
#include "lwip/err.h"
#include "cJSON.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

void smartconfig_example_task(void * parm);
void nuovo_task(void *pvParameter);
int read_spiffs_value(int variabile);
int write_spiffs_value(int variabile,uint32_t valore);
void oled_scrivi(char *stringa,u8g2_uint_t x, u8g2_uint_t y,uint8_t noClear);
uint32_t get_bottiglie_rimanenti(uint32_t gasconsumato,char *str_lieve,char *str_media,char *str_molto,int strsize);


static void generate_json();
static void http_server(void *pvParameters);
static void peso_task();
static void ioTask();

int write_credential();

    
#define WIFI_SSID "AndroidAP"
#define WIFI_PASS "marsala25"



#define delay(ms) (vTaskDelay(ms/portTICK_RATE_MS))

char ipStr[18];
char* json_unformatted;
const static char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_index_hml[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "  <style type=\"text/css\">\n"
      "    html, body, iframe { margin: 0; padding: 0; height: 100%; }\n"
      "    iframe { display: block; width: 100%; border: none; }\n"
      "  </style>\n"
      "<title>HELLO ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Hello World, from ESP32!</h1>\n"
      "</body>\n"
      "</html>\n";




wifi_config_t info_wifi_config= {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };


TaskHandle_t xHandle = NULL;

esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
	// initialize the u8g2 hal
	u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
	
	// initialize the u8g2 library
	u8g2_t u8g2;
	
	
#define INTERRUPTED_BY_SIGNAL (errno == EINTR || errno == ECHILD)


#ifndef MAXLINE
#define MAXLINE 1024
#endif




#define GAS_RIMANENTE	1
#define DA_DEFINIRE		2

static uint32_t conta_livellogas=0;
static int dadefinire=0;


static uint8_t presenzabombola=0;
SemaphoreHandle_t xSemaphore = NULL;

static xQueueHandle gpio_evt_queue = NULL;

// interrupt service routine, called when the button is pressed
void IRAM_ATTR gasbottle_isr_handler(void* arg) {
	
    // notify the button task
	//xSemaphoreGiveFromISR(xSemaphore, NULL);
	
	 uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);

}

// task that will react to button clicks
void bottlegas_task(void* arg) 
{
uint32_t io_num;
	
	// infinite loop
	while(1)
	{
		
		vTaskDelay(100 / portTICK_PERIOD_MS);
		printf("\n bottlegas_task \n");
		
		if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
		{
            
            
            printf("GPIO[%d] intr, val:%d\n", io_num,presenzabombola);
			oled_scrivi("ERR GAS",2,17,0);
			gpio_set_level(LIMIT_SWITCH_LED, presenzabombola);
        }
         presenzabombola=gpio_get_level(io_num);
        
		
	}
}


uint32_t get_bottiglie_rimanenti(uint32_t gasconsumato,char *str_lieve,char *str_media,char *str_molto,int strsize)
{
  uint32_t  gasrimanente=0, num_lieve=0, num_media=0,num_molto=0;
  
   gasrimanente=GAS_COMPLETO-gasconsumato;
   
   
   num_lieve=(uint32_t)gasrimanente/FRIZ_LIEVE;
   num_media=(uint32_t)gasrimanente/FRIZ_MEDIA;
   num_molto=(uint32_t)gasrimanente/FRIZ_MOLTO;
   //printf("\n\n gasrimanente=%d\ngasconsumato%d\nnum_lieve=%d\nnum_media=%d\nnum_molto=%d\n\n",gasrimanente,gasconsumato,num_lieve,num_media,num_molto);
 
   snprintf(str_lieve,strsize, "%d", num_lieve);
   snprintf(str_media,strsize, "%d", num_media);
   snprintf(str_molto,strsize, "%d", num_molto);
   
  return gasrimanente;

}
/**/

void oled_scrivi(char *stringa,u8g2_uint_t x, u8g2_uint_t y,uint8_t group)
{

 static uint8_t mygroup;
 

  if((group==0) || (mygroup!=group))
  {
	u8g2_ClearBuffer(&u8g2);
	mygroup=group;
  }
	
	u8g2_SetFont(&u8g2, u8g2_font_timR14_tf);
	u8g2_DrawStr(&u8g2, x,y,stringa);
	u8g2_SendBuffer(&u8g2);


}

/*

read a whole buffer, for performance, and then return one char at a time

*/
static ssize_t my_read (int fd, char *ptr)
{
  static int read_cnt = 0;
  static char *read_ptr;
  static char read_buf[MAXLINE];

  if (read_cnt <= 0)
  {
again:
    if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0)
    {
      if (INTERRUPTED_BY_SIGNAL)
      	goto again;
      return -1;
    }
    else
      if (read_cnt == 0)
    	return 0;
    read_ptr = read_buf;
  }
  read_cnt--;
  *ptr = *read_ptr++;
  return 1;
}


ssize_t readline (int fd, void *vptr, size_t maxlen)
{
  int n, rc;
  char c, *ptr;

  ptr = vptr;
  for (n=1; n<maxlen; n++)
  {
    if ( (rc = my_read(fd,&c)) == 1)
    {
      *ptr++ = c;
      if (c == '\n')
      {
      	n=n-1;
      	break;	/* newline is stored, like fgets() */
      }
    }
    else if (rc == 0)
    {
      if (n == 1)
      	return 0; /* EOF, no data read */
      else
      	break; /* EOF, some data was read */
    }
    else
      return -1; /* error, errno set by read() */
  }
  *ptr = 0; /* null terminate like fgets() */
  return n;
}


ssize_t Readline (int fd, void *ptr, size_t maxlen)
{
  ssize_t n;

  if ( (n = readline(fd, ptr, maxlen)) < 0)
    printf ("error - readline() failed");
  return n;
}
/************** from tutorial master spiffs *********************/
// max buffer length
#define LINE_MAX_CMD	50

// global variables
char actual_path[256];


void ls(char* path) {

	printf("\r\nListing folder %s\r\n", path);
	
	// open the specified folder
	DIR *dir;
	dir = opendir(path);
    if (!dir) {
        printf("Error opening folder\r\n");
        return;
    }
	
	// list the files and folders
	struct dirent *direntry;
	while ((direntry = readdir(dir)) != NULL) {
		
		// do not print the root folder (/spiffs)
		if(strcmp(direntry->d_name, "/spiffs") == 0) continue;
		
		if(direntry->d_type == DT_DIR) printf("DIR\t");
		else if(direntry->d_type == DT_REG) printf("FILE\t");
		else printf("???\t");
		printf("%s\r\n", direntry->d_name);
	}
	
	// close the folder
	closedir(dir);
}

void cat(char* filename) {

	printf("\r\nContent of the file %s\r\n", filename);
	
	// open the specified file
	char file_path[300];
	strcpy(file_path, actual_path);
	strcat(file_path, "/");
	strcat(file_path, filename);

	FILE *file;
	file = fopen(file_path, "r");
    if (!file) {
        printf("Error opening file %s\r\n", file_path);
        return;
    }
	
	
	// display the file content
	int filechar;
	while((filechar = fgetc(file)) != EOF)
		putchar(filechar);
	
	
	// close the folder
	fclose(file);
}

void cd(char* path) {

	printf("\r\nMoving to directory %s\r\n", path);
	
	// backup the actual path
	char previous_path[256];
	strcpy(previous_path, actual_path);
	
	// if the new path is ".." return to the previous
	if(strcmp(path, "..") == 0) {
		
		// locate the position of the last /
		char* pos = strrchr(actual_path, '/');
		if(pos != actual_path) pos[0] = '\0';
	}
	
	// if the new path starts with /, append to the root folder
	else if(path[0] == '/') {
	
		strcpy(actual_path, "/spiffs");
		if(strlen(path) > 1) strcat(actual_path, path);
	}
	
	// else add it to the actual path
	else {
		strcat(actual_path, "/");
		strcat(actual_path, path);
	}
	
	// verify that the new path exists
	DIR *dir;
	dir = opendir(actual_path);
    
	// if not, rever to the previous path
	if (!dir) {
        printf("Folder does not exists\r\n");
        strcpy(actual_path, previous_path);
		return;
    }
	
	closedir(dir);
}

// parse command
void parse_command(char* command) {
	
	// split the command and the arguments
	char* token;
	token = strtok(command, " ");
	
	if(!token) {
		
		printf("\r\nNo command provided!\r\n");
		return;
	}
	
	// LS command, list the content of the actual folder
	if(strcmp(token, "ls") == 0) {
	
		ls(actual_path);
	}
	
	// CAT command, display the content of a file
	else if(strcmp(token, "cat") == 0) {
	
		char* filename = strtok(NULL, " ");
		if(!filename) {
			
			printf("\r\nNo file specified!\r\n");
			return;
		}
		cat(filename);
	}
	
	// CD command, move to the specified directory
	else if(strcmp(token, "cd") == 0) {
	
		char* path = strtok(NULL, " ");
		if(!path) {
			
			printf("\r\nNo directory specified!\r\n");
			return;
		}
		cd(path);
	}
	
	else if (strcmp(token, "rm") == 0)
	{
	  remove("/spiffs/hello.txt");
	  remove("/spiffs/valori.txt");
	  printf("\r\nhello.txt e valori.txt rimossi!!\r\n");
	}
	
	// UNKNOWN command
	else printf("\r\nUnknown command!\r\n");
}


// print the command prompt, including the actual path
void print_prompt() {
	
	printf("\r\nesp32 (");
	printf(actual_path);
	printf(") > ");
	fflush(stdout);
}


// main task
void main_task(void *pvParameter) {

	// buffer to store the command	
	char line[LINE_MAX_CMD];
	int line_pos = 0;

	print_prompt();
	
	// read the command from stdin
	while(1) {
	
		int c = getchar();
		
		// nothing to read, give to RTOS the control
		if(c < 0) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;
		}
		if(c == '\r') continue;
		if(c == '\n') {
		
			// terminate the string and parse the command
			line[line_pos] = '\0';
			parse_command(line);
			
			// reset the buffer and print the prompt
			line_pos = 0;
			print_prompt();
		}
		else {
			putchar(c);
			line[line_pos] = c;
			line_pos++;
			
			// buffer full!
			if(line_pos == LINE_MAX) {
				
				printf("\nCommand buffer full!\n");
				
				// reset the buffer and print the prompt
				line_pos = 0;
				print_prompt();
			}
		}
	}	
}



/***********************************/



static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	static char taskname[20];
	static int reconnect=0;
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	

	
    switch(event->event_id) {
    
    
    case SYSTEM_EVENT_STA_START:
			ESP_LOGI(taskname, "event->event_id SYSTEM_EVENT_STA_START %d",event->event_id);
			ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "Giovanni"));   
			esp_wifi_connect();

			xTaskCreate(nuovo_task, "nuovo_task", 4096, NULL, 5, NULL);

    break;
    case SYSTEM_EVENT_STA_GOT_IP:
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
			ESP_LOGI(taskname, "event->event_id SYSTEM_EVENT_STA_GOT_IP %d",event->event_id);

	break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        
        	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        	esp_wifi_connect();
            ESP_LOGI(taskname, "event->event_id SYSTEM_EVENT_STA_DISCONNECTED %d",event->event_id);
            reconnect++;
            if(reconnect>10)
            {
            	ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "espressif"));   
            	xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 2, NULL);
            }

        break;
     case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(taskname, "event->event_id SYSTEM_EVENT_STA_CONNECTED %d",event->event_id);
    		reconnect=0;
        break;
    default:
   			 ESP_LOGI(taskname, "event->event_id DEFAULT %d",event->event_id);
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{

	char taskname[20];
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    
    
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    
    ESP_LOGI(taskname, "initialise_wifi");
    ESP_LOGI(taskname, "SSDI:%s", info_wifi_config.sta.ssid);
    ESP_LOGI(taskname, "PASSWORD:%s", info_wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &info_wifi_config));   
    ESP_ERROR_CHECK( esp_wifi_start() );
    // start the main task
        
}
/*static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}*/


void nuovo_task(void *pvParameter)
{
  // char pcWriteBuffer[1024] = "";
	uint32_t ticktime=xTaskGetTickCount();
	uint32_t timestamp;
	static uint8_t contatore=0;
	char taskname[20];
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
	memset(ipStr,0,sizeof(ipStr));
	
	// wait for connection
	printf("nuovo_task(%d): waiting for connection to the wifi network... \n",ticktime);
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, true,false, portMAX_DELAY);
	ESP_LOGI(taskname,"connected!\n");
	
	// print the local IP address
	tcpip_adapter_ip_info_t ip_info;
	
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	strcpy(ipStr,ip4addr_ntoa(&ip_info.ip));
	ESP_LOGI(taskname,"IP Address:  %s\n", ipStr);
	ESP_LOGI(taskname,"Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
	ESP_LOGI(taskname,"Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));
	oled_scrivi("CONNESSO",2,17,0);
	
	xTaskCreate(&generate_json, "json", 2048, NULL, 5, NULL);
	xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);

	//xTaskCreate(&peso_task, "peso_task", 2048, NULL, 2, NULL);
	//tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "SUCAMILLA");
	
	while(1) {
	
		//vTaskDelay(10000 / portTICK_RATE_MS);
		//vTaskGetRunTimeStats(( char *)pcWriteBuffer);
         //printf("NUOVO TASK:\n%s\n",pcWriteBuffer);     
         timestamp=esp_log_timestamp();
         ticktime=xTaskGetTickCount();
                  
         vTaskDelay(5000 / portTICK_PERIOD_MS);
		  if (contatore>0)
		  {
			/* Blink off (output low) */
			
			gpio_set_level(BLINK_GPIO, 1);
			contatore--;
			
		  }
		  else
		  {
			gpio_set_level(BLINK_GPIO, 0);
			contatore++;
		  }
         
         
	}
}


static void sc_callback(smartconfig_status_t status, void *pdata)
{

	char taskname[20];
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(taskname, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(taskname, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(taskname, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(taskname, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            memset(&info_wifi_config,0,sizeof(info_wifi_config));
            info_wifi_config = (wifi_config_t)*wifi_config;
            
            write_credential();
        
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            ESP_LOGI(taskname, "Connecting to SSDI:%s\n", wifi_config->sta.ssid);
            ESP_LOGI(taskname, "PASSWORD:%s", wifi_config->sta.password);
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(taskname, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(taskname, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    uint32_t ticktime;
    char taskname[20];
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
		

	// wait for connection
	
    
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        // wait for connection
        ticktime=xTaskGetTickCount();
		ESP_LOGI(taskname,"Main task(%d): waiting for connection to the wifi network... ",ticktime);
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(taskname, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(taskname, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}


int write_spiffs_value(int variabile,uint32_t valore)
{
	char taskname[20];
	
	int returnvalue=0,retscanf=0;


		strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
		ESP_LOGI(taskname, "Start write file valori.txt");
		
		FILE *fp = fopen("/spiffs/valori.txt", "r");
		
		
		if (fp == NULL) {
			ESP_LOGE(taskname, "Failed to open file for writing");
			fp = fopen("/spiffs/valori.txt", "w");
			fclose(fp);
			fp = fopen("/spiffs/valori.txt", "r");
			//return -3;
		}
		
		if(variabile != 0 ) 
		{	
			if (variabile == GAS_RIMANENTE)
			{
				retscanf=fscanf(fp, "LIVELLO_GAS=%u DADEFINIRE=%d", &conta_livellogas, &dadefinire);
				
				/*if(retscanf<=0)
				{*/
				if (ferror(fp)) 
				{
					fclose(fp);
					fp = fopen("/spiffs/valori.txt", "w");
        			perror("fscanf");
					returnvalue=-1;
					conta_livellogas=0;
					dadefinire=0;
					ESP_LOGE(taskname, "Failed to read file valori.txt retscanf=%d livellogas=%d",retscanf,conta_livellogas);        					
					fprintf(fp,"LIVELLO_GAS=%d DADEFINIRE=%d\nFINE\n", conta_livellogas, dadefinire);
				}
				else
				{
				  	ESP_LOGI(taskname, "Read value valori.txt %d",conta_livellogas);			
					/* valore è positivo allora aggiornalo altrimenti esegui solo la lettura*/
					if(valore>0)
					{
						fclose(fp);
						fp = fopen("/spiffs/valori.txt", "w+");
					    	
						ESP_LOGI(taskname, "Update value valori.txt");		
						conta_livellogas=valore;
						fprintf(fp,"LIVELLO_GAS=%d DADEFINIRE=%d\nFINE\n", conta_livellogas, dadefinire);

					}
				}
			}
		}
		else
			returnvalue=-2;

		
		
		
		
    	fclose(fp);
    	ESP_LOGI(taskname, "End write file valori.txt returnvalue=%d",returnvalue);
	
	// ritorna 0 in caso di scrittura positiva
	return returnvalue;
}


int write_credential()
{
 char taskname[20];
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	


		ESP_LOGI(taskname, "writing file");
    	FILE* f = fopen("/spiffs/hello.txt", "w+");
    	if (f == NULL) {
        	ESP_LOGE(taskname, "Failed to open file for writing");
        	return 3;
    	}
    	fprintf(f,"%s\n%s\n",info_wifi_config.sta.ssid,info_wifi_config.sta.password);

    	fclose(f);
    	ESP_LOGI(taskname, "File written");
    	
    	return 0;

}
int esempio_spiffs()
{
	char taskname[20];
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);

    // copiato da  spiffs example esp-idf
    ESP_LOGI(taskname, "Initializing SPIFFS");
    
    
    
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(taskname, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(taskname, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(taskname, "Failed to initialize SPIFFS (%d)", ret);
        }
        return 1;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(taskname, "Failed to get SPIFFS partition information");
        return 2;
    } else {
        ESP_LOGI(taskname, "Partition size: total: %d, used: %d", total, used);
    }
    
    
    ESP_LOGI(taskname, "Open and Reading file");
    int f = open("/spiffs/hello.txt",O_RDONLY);
    if (f == -1) {
        ESP_LOGE(taskname, "Failed to open file for reading");
        // Use POSIX and C standard library functions to work with files.
    	// First create a file.
    	
    	if(write_credential()!=0)
    	  return 3;
        //return;
    }
    else
    {
    	char buff[50];
    	int size=0;
	    memset(&info_wifi_config,0,sizeof(info_wifi_config));

    	memset(buff,0,sizeof(buff));
    	size=Readline (f, buff, 40);
    	memcpy(info_wifi_config.sta.ssid,buff,size);
    	ESP_LOGI(taskname, "size=%d", size);
    	memset(buff,0,sizeof(buff));
		size=Readline (f, buff, 40);
		ESP_LOGI(taskname, "size=%d", size);

		memcpy(info_wifi_config.sta.password,buff,size);
    	close(f);
    	ESP_LOGI(taskname, "ssid:%s\npassword:%s", info_wifi_config.sta.ssid,info_wifi_config.sta.password);
    
    }
    

    
    // copiato da 16_spiff  esp32-tutorial
    ESP_LOGI(taskname, "/spiffs start main_task linea di comando");
    // initial path
	strcpy(actual_path, "/spiffs");
	// start the loop task
	xTaskCreate(main_task, "main_task", 8192, NULL, 1, NULL);
	
	
	// start the loop task
	xTaskCreate(ioTask, "ioTask", 8192, NULL, 5, NULL);
	

    return 0;
}
/**************************** SERVER WEB e JSON *********************************************/
static void
http_server_netconn_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);

    // strncpy(_mBuffer, buf, buflen);

printf("buf: %c-%c-%c-%c-%c-%c dim =%d\n", buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buflen);
    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    printf("buffer = %s \n", buf);
    if (buflen>=5 &&
        buf[0]=='G' &&
        buf[1]=='E' &&
        buf[2]=='T' &&
        buf[3]==' ' &&
        buf[4]=='/' ) {
          printf("buf[5] = %c\n", buf[5]);
      /* Send the HTML header
             * subtract 1 from the size, since we dont send the \0 in the string
             * NETCONN_NOCOPY: our data is const static, so no need to copy it
       */

      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

      if(buf[5]=='r') {
      /* RESET conta gas*/
        if(write_spiffs_value(GAS_RIMANENTE,1) != 0)
			ESP_LOGE("http_server_netconn_serve","ERROR write_spiffs_value");
		
		oled_scrivi("SET GAS = 1",2,17,0);
        
        /* Send our HTML page */
        netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
      }
      else if(buf[5]=='j') {
    	  netconn_write(conn, json_unformatted, strlen(json_unformatted), NETCONN_NOCOPY);
      }
      else {
          netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
      }
    }

  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}

static void http_server(void *pvParameters)
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
       http_server_netconn_serve(newconn);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}


static void generate_json() {

	char buffer[30];
	struct timeval tv;

	time_t curtime;

	gettimeofday(&tv, NULL); 
	curtime=tv.tv_sec;

	strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
	printf("%s%ld\n",buffer,tv.tv_usec);



	cJSON *root, *info, *d,*a;
	root = cJSON_CreateObject();

	cJSON_AddItemToObject(root, "peso", d = cJSON_CreateObject());
	
	cJSON_AddStringToObject(d, "ip", "127.0.0.1");
	cJSON_AddStringToObject(d, "email", "giovi@yahoo.it");
	cJSON_AddNumberToObject(d, "valore",0);
	cJSON_AddNumberToObject(d, "temperature", 70.123);
	
	cJSON_AddItemToObject(root, "acqua", a = cJSON_CreateObject());

	cJSON_AddStringToObject(a, "name", "CMMC-ESP32-NANO");
	cJSON_AddStringToObject(a, "email", "giovi@yahoo.it");
	cJSON_AddNumberToObject(a, "valore", 10.100);
	cJSON_AddNumberToObject(a, "temperature", 70.123);

	cJSON_AddItemToObject(root, "info", info = cJSON_CreateObject());

	cJSON_AddStringToObject(info, "ssid", "dummy");
	cJSON_AddNumberToObject(info, "heap", esp_get_free_heap_size());
	cJSON_AddStringToObject(info, "sdk", esp_get_idf_version());
	cJSON_AddNumberToObject(info, "time", *buffer);

	while (1) {
		gettimeofday(&tv, NULL); 
		curtime=tv.tv_sec;
		strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
	
		cJSON_ReplaceItemInObject(info, "heap",
				cJSON_CreateNumber(esp_get_free_heap_size()));
		cJSON_ReplaceItemInObject(info, "time",
				cJSON_CreateNumber(*buffer));
		cJSON_ReplaceItemInObject(info, "sdk",
				cJSON_CreateString(esp_get_idf_version()));
		
		cJSON_ReplaceItemInObject(d, "ip",
				cJSON_CreateString(ipStr));			
		cJSON_ReplaceItemInObject(d, "valore",
				cJSON_CreateNumber(conta_livellogas));		

		json_unformatted = cJSON_PrintUnformatted(root);
/*		printf("[len = %d]  ", strlen(json_unformatted));

		for (int var = 0; var < strlen(json_unformatted); ++var) {
			putc(json_unformatted[var], stdout);
		}

		printf("\n");*/
		fflush(stdout);
		delay(2000);
		free(json_unformatted);
	}
}



static void peso_task()
{

	char taskname[20];
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
	
	hx711_begin(0);
	ESP_LOGI(taskname,"Before setting up the scale:");
  	ESP_LOGI(taskname,"read: \t\t");
  
	ESP_LOGI(taskname,"%ld\n",hx711_read(0)); // print a raw reading from the ADC
	
	ESP_LOGI(taskname,"read average: \t\t");
	ESP_LOGI(taskname, "%ld",hx711_read_average(20));// print the average of 20 readings from the ADC
	
	ESP_LOGI(taskname,"get value: \t\t");
	ESP_LOGI(taskname, "%f",hx711_get_value(5)); // print the average of 5 readings from the ADC minus the tare weight (not set yet)	
	
	ESP_LOGI(taskname,"get units: \t\t");
	ESP_LOGI(taskname, "%f",hx711_get_units(5)); // print the average of 5 readings from the ADC minus tare weight (not set) divided
						// by the hx711_SCALE parameter (not set yet)
	hx711_set_scale(2280.f);  // this value is obtained by calibrating the scale with known weights; see the README for details
	hx711_tare(0);			// reset the scale to 0
	
    ESP_LOGI(taskname,"After setting up the scale:");

  	ESP_LOGI(taskname,"read: \t\t");
	ESP_LOGI(taskname, "%ld",hx711_read()); // print a raw reading from the ADC
	ESP_LOGI(taskname, "%ld",hx711_read_average(20));// print the average of 20 readings from the ADC
	
	
	ESP_LOGI(taskname, "%f",hx711_get_value(5)); // print the average of 5 readings from the ADC minus the tare weight, set with tare()

	ESP_LOGI(taskname, "%f",hx711_get_units(5)); // print the average of 5 readings from the ADC minus tare weight, divided
						// by the hx711_SCALE parameter set with set_scale
	
	 while(1) {
     
   		
          ESP_LOGI(taskname,"one reading:\t");
          ESP_LOGI(taskname, "%f",hx711_get_units(1)); 
          ESP_LOGI(taskname,"\t| average:\t");
          ESP_LOGI(taskname, "%f",hx711_get_units(10)); 
          hx711_power_down();			        // put the ADC in sleep mode
          vTaskDelay(5000 / portTICK_RATE_MS);
          hx711_power_up();
   
         }



}
static void ioTask()
{
	char taskname[20];
	uint32_t ticktime=xTaskGetTickCount();
	uint32_t timestamp;
	static uint32_t local_conta_livellogas=0,flag=0;
	int retval=0;
	uint8_t livelloacqua=0;

	vTaskDelay(8000 / portTICK_PERIOD_MS);
	// create the binary semaphore
	//xSemaphore = xSemaphoreCreateBinary();
	//create a queue to handle gpio event from isr
   // gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
	gpio_pad_select_gpio(BUTTON_IO1);
    gpio_pad_select_gpio(BUTTON_IO2);
    gpio_pad_select_gpio(BUTTON_IO3);
    gpio_pad_select_gpio(LIMIT_SWITCH);
    gpio_pad_select_gpio(LIMIT_SWITCH_LED);
    gpio_pad_select_gpio(BUTTON1_LED);
    gpio_pad_select_gpio(BUTTON2_LED);
    gpio_pad_select_gpio(BUTTON3_LED);
    gpio_pad_select_gpio(WATER_LEVEL);
    gpio_pad_select_gpio(WATER_LEVEL_LED);

    /* Set the GPIO as a push/pull output */
   	gpio_set_direction(BUTTON_IO1, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_IO2, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_IO3, GPIO_MODE_INPUT);
    gpio_set_direction(LIMIT_SWITCH, GPIO_MODE_INPUT);
    // enable interrupt on falling (1->0) (0->1) edge for button pin
	//gpio_set_intr_type(LIMIT_SWITCH, GPIO_INTR_HIGH_LEVEL);
    
    gpio_set_direction(WATER_LEVEL, GPIO_MODE_INPUT);
    gpio_set_direction(LIMIT_SWITCH_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON1_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON2_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON3_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(WATER_LEVEL_LED, GPIO_MODE_OUTPUT);
    
    
    //start gas task
   // xTaskCreate(bottlegas_task, "bottlegas_task", 2048, NULL, 10, NULL);
    
    // install ISR service with default configuration
	//gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	
	// attach the interrupt service routine
	//gpio_isr_handler_add(LIMIT_SWITCH, gasbottle_isr_handler, (void*)LIMIT_SWITCH);
	
	if((retval=write_spiffs_value(GAS_RIMANENTE,0)) != 0)
			ESP_LOGE(taskname,"ERROR retval  %d", retval);
    local_conta_livellogas=conta_livellogas;
	
	while(1) 
	{
		timestamp=esp_log_timestamp();
		ticktime=xTaskGetTickCount();

		livelloacqua=gpio_get_level(WATER_LEVEL);
		
		presenzabombola=gpio_get_level(LIMIT_SWITCH);
			
		vTaskDelay(100 / portTICK_PERIOD_MS);
		gpio_set_level(WATER_LEVEL_LED,livelloacqua);
		gpio_set_level(LIMIT_SWITCH_LED, presenzabombola);


		/* se il livello acqua è corretto e la bombola è ben installata */
		if ((livelloacqua == 1) && (presenzabombola == 0))
		{
			
			if (gpio_get_level(BUTTON_IO3) == 0)
			{
			/* Blink off (output low) */
				gpio_set_level(BUTTON3_LED, 1);
				printf("(%s) BUTTON_IO3 pressed! \ntimestamp %d\nticktime %d\n\n",taskname,timestamp,ticktime);
				
				if(flag==0)
				{
					flag=1;
					conta_livellogas=conta_livellogas+FRIZ_MOLTO;
				}
				
				oled_scrivi("MOLTO FRIZ",2,17,0);
				//8 secondi
			
			}
			else if(gpio_get_level(BUTTON_IO2) == 0)
			{
			/* Blink off (output low) */
				gpio_set_level(BUTTON2_LED, 1);
				printf("(%s) BUTTON_2 pressed! \ntimestamp %d\nticktime %d\n\n",taskname,timestamp,ticktime);
				if(flag==0)
				{
					flag=1;
					conta_livellogas=conta_livellogas+FRIZ_MEDIA;
				}
				oled_scrivi("MEDIA FRIZ",2,17,0);
				//6 secondi
			}
			else if(gpio_get_level(BUTTON_IO1) == 0)
			{
			/* Blink off (output low) */
				gpio_set_level(BUTTON1_LED, 1);
				printf("(%s) BUTTON_IO1 pressed! \ntimestamp %d\nticktime %d\n\n",taskname,timestamp,ticktime);
				if(flag==0)
				{
					flag=1;
					conta_livellogas=conta_livellogas+FRIZ_LIEVE;
				}
				oled_scrivi("POCO FRIZ",2,17,0);
				//4 secondi
			}
			else
			{
				flag=0;
				
				char str_lieve[6],str_media[6],str_molto[6];
				char buf[20];
				memset(buf,0,sizeof(buf));
				memset(str_lieve,0,sizeof(str_lieve));
				memset(str_media,0,sizeof(str_media));
				memset(str_molto,0,sizeof(str_molto));

				get_bottiglie_rimanenti(conta_livellogas,str_lieve,str_media,str_molto,sizeof(str_molto)-1);
				
				memset(buf,0,sizeof(buf));		
				snprintf(buf, sizeof(buf)-1,"bottle Low=%s",str_lieve);
				oled_scrivi(buf,2,17,1);
				memset(buf,0,sizeof(buf));		
				snprintf(buf, sizeof(buf)-1,"bottle Mid=%s",str_media);
				oled_scrivi(buf,2,34,1);
				memset(buf,0,sizeof(buf));		
				snprintf(buf, sizeof(buf)-1,"bottle High=%s",str_molto);
				oled_scrivi(buf,2,51,1);
				
				//ESP_LOGI(taskname,"Attesa Comando.....");
				vTaskDelay(200 / portTICK_PERIOD_MS);
				gpio_set_level(BUTTON1_LED, 0);
				gpio_set_level(BUTTON2_LED, 0);
				gpio_set_level(BUTTON3_LED, 0);
				//ESP_LOGI(taskname,"Condizione di Riposo");
				
				if(local_conta_livellogas != conta_livellogas)
				{
					ESP_LOGI(taskname,"Salvataggio conta_livellogas");
					oled_scrivi("SAVE livellogas",2,17,0);
					local_conta_livellogas=conta_livellogas;
					if((retval=write_spiffs_value(GAS_RIMANENTE,conta_livellogas)) != 0)
							ESP_LOGE(taskname,"ERROR retval  %d", retval);
				}

			}
			
		}
		else
		{
			if (livelloacqua == 0)
			{
				ESP_LOGE(taskname,"WATER_LEVEL=%d NOT PRESENT",livelloacqua);
			//	oled_scrivi("No Water level",2,17,0);
			}
			
			if(presenzabombola == 1)
			{
				ESP_LOGE(taskname,"GAS_WITCH=%d NOT OK",presenzabombola);
			//	oled_scrivi("Gas Assente",2,34,0);
			}
			//presenzabombola=0;
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}
	}
}
void app_main()
{
	int flag=0;
	char pcWriteBuffer[1024] = "";
	char taskname[20];
	
	strncpy(taskname,pcTaskGetTaskName(NULL),sizeof(taskname)-1);
	
	
   // uint32_t timestamp=0;

	
	/********* OLED ********/
	// initialize the u8g2 hal
	u8g2_esp32_hal.sda = PIN_SDA;
	u8g2_esp32_hal.scl = PIN_SCL;
	u8g2_esp32_hal_init(u8g2_esp32_hal);
	
	// initialize the u8g2 library
	u8g2_Setup_ssd1306_128x64_noname_f(
		&u8g2,
		U8G2_R0,
		u8g2_esp32_msg_i2c_cb,
		u8g2_esp32_msg_i2c_and_delay_cb);
	
	// set the display address
	u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
	
	// initialize the display
	u8g2_InitDisplay(&u8g2);
	
	// wake up the display
	u8g2_SetPowerSave(&u8g2, 0);
	

	oled_scrivi("Soda Smart",2,17,0);
	vTaskDelay(3000 / portTICK_RATE_MS);
	
	
	
	
	ESP_ERROR_CHECK( nvs_flash_init() );

	flag=esempio_spiffs();
 	ESP_LOGI(taskname, "esempio_spiffs return value %d",flag);
	
	
	/***********************/
	
	if(flag == 0)
	{
		initialise_wifi();  //   *************************************************************
		
		/*  ESEMPIO LED Set the GPIO as a push/pull output */
		gpio_pad_select_gpio(BLINK_GPIO);
		gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

		while(1) 
		{

		 //vTaskGetRunTimeStats(( char *)pcWriteBuffer); 		 
		 //printf("app_main:timestamp %d\n%s\n",esp_log_timestamp(),pcWriteBuffer);    
		  vTaskDelay(5000 / portTICK_RATE_MS);

		}      
    }
    else
    {
    	ESP_LOGE(taskname, "Exit for Error");
    }
    
}

/****
TABELLA CHIAMATE PROCESSI:

1) app_main --> main_task,ioTask, wifi
2) main_task: console dei comandi (ls, cat, rm,)
3) wifi --> nuovo_task, smartconfig_example_task (solo se non si connette per un numero N di volte)
4) nuovo_task --> (dopo la connessione al wifi) generate_json, http_server, peso_task
5) generate_json: genera il json e ne modifica il valore
6) http_server: server http
7) peso_task: legge il valore dal sensore di peso (componente hx711)
8) smartconfig_example_task: è chiamato solo in fase di acquisizione della SSID e password wifi e poi si autoelimina


6 chiamate xTaskCreate
********/