#include <stdio.h>
#include <mosquitto.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <cjson/cJSON.h>


#define MQTT_DEBUG  0
#define BUFFER_SIZE 64

struct mosquitto *mosq = NULL;
char *topic = NULL;
int rv;
int MQTT_PORT = 0;
int keepalive = 60;
bool clean_session = true;
char *CA_CERT = "../../../mqtt_broker/certs/ca.crt";
char *CLIENT_CRT = "../../../mqtt_broker/certs/client.crt";
char *CLIENT_KEY = "../../../mqtt_broker/certs/client.key";
char MQTT_BROKER[BUFFER_SIZE] = {0}; // IP address of the system on which broker is running
char *MQTT_TOPIC1 = "req_config_data";
char *MQTT_TOPIC2 = "config_data";
bool tls_flag = false;

#define MQTT_QOS 1
#define MQTT_RETAIN 0

char data[1024];
uint8_t flag = 0;

bool is_zero_initialized(const char *buffer, size_t size) 
{
    for (size_t i = 0; i < size; i++) 
    {
        if (buffer[i] != '\0') 
        {
            return false;
        }
    }

    if(!strcmp(buffer, "0.0.0.0"))
    {
        return true;
    }
    return true;
}

uint8_t u8GetIP() {
    int n;
    struct ifreq ifr;
    char interface_name[] = "wlxe4fac4521afb";
    //char interface_name[] = "enp1s0";
 
    n = socket(AF_INET, SOCK_DGRAM, 0);
    //Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    //Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name , interface_name , IFNAMSIZ - 1);
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);
    printf("IP Address of Interface %s - %s\n" , interface_name , inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr )->sin_addr));
    if(strstr(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr )->sin_addr), "0.0.0.0") != NULL) {
        return -1;
    }
    sprintf(MQTT_BROKER, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr )->sin_addr));
    printf("IP Address of broker is %s\n" , MQTT_BROKER);
    return 0;
}

void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {

#if MQTT_DEBUG
    if(message->payloadlen){
        printf("%s %.*s\n", message->topic, (int)message->payloadlen, (char *)message->payload);
        strncpy (data, (char *)message->payload, (int)message->payloadlen);
        flag = 1;
    } else {
        printf("%s (null)\n", message->topic);
    }
#endif
    printf("\r\nresponse: \r\n");
    cJSON *config_data = cJSON_Parse((char*)message->payload);

    if(config_data != NULL)
    {
        cJSON *fw_version_data = cJSON_GetObjectItem(config_data, "fw_version");
        if (fw_version_data != NULL)
        {
            printf("\r\nreceived FW Version: %02x\r\n", fw_version_data->valueint);
        }
        cJSON *serial_no_data = cJSON_GetObjectItem(config_data, "serial_number");
        if (serial_no_data != NULL)
        {
            printf("\r\nreceived Serial No: %s\r\n", serial_no_data->valuestring);
        }
        cJSON *usb_mode_data = cJSON_GetObjectItem(config_data, "usb_mode");
        if (usb_mode_data != NULL)
        {
            switch (usb_mode_data->valueint)
            {
            case 0:
                printf("\r\nreceived USB Mode: USB_DeviceStateDisable\r\n");
                break;

            case 1:
                printf("\r\nreceived USB Mode: USB_DeviceStateDownstream\r\n");
                break;

            case 2:
                printf("\r\nreceived USB Mode: USB_DeviceStateDiagnostics\r\n");
                break;

            case 3:
                printf("\r\nreceived USB Mode: USB_HostStateUpstream\r\n");
                break;

            default:
                printf("\r\nreceived USB Mode: UNKNOWN\r\n");
                break;
            }
        }

        cJSON_Delete(config_data);
    }
    else
    {
        //No response received
        printf("Invalid response received!\n");
    }
}

void display_menu() 
{
    // Display the header
    printf("MQTT Client Example\n");
    printf("-------------------\n");

    // Display the options
    printf("Options:\n");
    printf("1. Request Configuration data\n");
    printf("2. Exit\n");
}


int mqtt_connect()
{
    struct mosquitto *mosq;
    int rc;
    printf("IP is successfully\n");

    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);
    if(!mosq) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    if(tls_flag == true)
    {
        mosquitto_tls_set(mosq, CA_CERT, NULL, CLIENT_CRT, CLIENT_KEY, NULL);
    }

    mosquitto_message_callback_set(mosq, message_callback);

    printf("MQTT Broker is : %s\n", MQTT_BROKER);

    rc = mosquitto_connect(mosq, MQTT_BROKER, MQTT_PORT, 60);
    if(rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Error: Could not connect to MQTT broker. Return code: %d\n", rc);
        return 1;
    }

    mosquitto_subscribe(mosq, NULL, MQTT_TOPIC2, MQTT_QOS);

    mosquitto_loop_start(mosq);

}

int get_port()
{
    char input[2];
    printf("Select MQTT Port: (1 or 2)\n");
    while(1)
    {
        printf("1. Secure MQTT Port (8883)\n");
        printf("2. Unsecure MQTT Port (1883)\n");
        if (scanf("%1s", input) != 1) { // Read a single character
            printf("Invalid input. Please try again.\n");
            while (getchar() != '\n'); // Clear the input buffer
            continue;
        }
        
        
        if (strcmp(input, "1") == 0 || strcmp(input, "2") == 0) {
            break; // Valid input, exit the loop
        } else {
            printf("Invalid input. Please try again.\n");
        }
    }

    if(input[0] = '1')
    {
        //Secure MQTT
        MQTT_PORT = 8883;
        tls_flag = true;
        if(mqtt_connect() == 1)
        {
            return 1;
        }
    }
    else if(input[0] == '2')
    {
        //Unsecure MQTT
        MQTT_PORT = 1883;
        tls_flag = false;
        if(mqtt_connect() == 1)
        {
            return 1;
        }
    }

}

int main(int argc, char *argv[]) {
    
    char input[2];

    //Get the MQTT Broker Address and Port
    printf("Auto Getting the MQTT Broker Address... \n");

    if(u8GetIP() == -1) 
    {
        printf("Invalid Interface name\n");
        //return 0;
    }
    while(1)
    {
        printf("Is this MQTT Broker Address Correct? : %s (y/n)\t", MQTT_BROKER);
        if (scanf("%1s", input) != 1) { // Read a single character
            printf("Invalid input. Please try again.\n");
            while (getchar() != '\n'); // Clear the input buffer
            continue;
        }
        // Convert input to lowercase
        input[0] = tolower(input[0]);
        
        if (strcmp(input, "y") == 0 || strcmp(input, "n") == 0) {
            break; // Valid input, exit the loop
        } else {
            printf("Invalid input. Please try again.\n");
        }
    }

    if(input[0] == 'y')
    {
        //Yes, success in auto getting the address
        if(get_port() == 1)
        {
            return 1;
        }

    }
    else if(input[0] == 'n')
    {
        while (1)
        {
            // Ask the user to manually enter the addr
            memset(MQTT_BROKER, 0x00, BUFFER_SIZE);
            printf("Enter Server Address: ");
            scanf("%63s", MQTT_BROKER);
            printf("Is this MQTT Broker Address Correct? : %s (y/n)\t", MQTT_BROKER);
            if (scanf("%1s", input) != 1)
            { // Read a single character
                printf("Invalid input. Please try again.\n");
                while (getchar() != '\n')
                    ; // Clear the input buffer
                continue;
            }
            // Convert input to lowercase
            input[0] = tolower(input[0]);

            if (strcmp(input, "y") == 0 || strcmp(input, "n") == 0)
            {
                break; // Valid input, exit the loop
            }
            else
            {
                printf("Invalid input. Please try again.\n");
            }
        }

        if (get_port() == 1)
        {
            return 1;
        }
    }
    
    display_menu();
    printf("Enter your choice: ");
    // Wait for messages to arrive
    while (1)
    {
        

        // Get user input
        int choice;
        
        scanf("%d", &choice);

        // Process the user choice
        switch (choice)
        {
        case 1:
            printf("Requesting Configuration data...\n");
            cJSON *req_config_data = cJSON_CreateObject();
            if(req_config_data != NULL)
            {
                cJSON_AddBoolToObject(req_config_data,"req_fw_version", true);
                cJSON_AddBoolToObject(req_config_data,"req_serial_no", true);
                cJSON_AddBoolToObject(req_config_data,"req_usb_mode", true);
            }
            char* payload = cJSON_Print(req_config_data);
            // Call function to request configuration data
            //printf("sending payload: %s with size: %lu", payload, strlen(payload));
            mosquitto_publish(mosq, NULL, MQTT_TOPIC1, strlen(payload), payload, MQTT_QOS, MQTT_RETAIN);
            flag = 0;
            cJSON_Delete(req_config_data);

            break;
        case 2:
            printf("Exiting...\n");
            // Call function to exit
            exit(0);
            break;
        default:
            printf("Invalid choice!\n");
            break;
        }
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}