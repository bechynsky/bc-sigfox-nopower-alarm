#include <application.h>

#define BATTERY_UPDATE_INTERVAL   (10 * 60 * 1000)
#define USB_POWER_UPDATE_INTERVAL (20 * 1000)
#define PC_COMMUNICATION_TIMEOUT  (1 * 60 * 1000)

// Serial port communication buffers
bc_fifo_t tx_fifo;
bc_fifo_t rx_fifo;
uint8_t tx_fifo_buffer[10];
uint8_t rx_fifo_buffer[10];

// LED instance
bc_led_t led;

// SIGFOX
bc_module_sigfox_t sigfox;
bool last_is_usb_powered = true;

// battery
float voltage;

// server ping status
// OK - server response is ok
// NO_RESPONSE - there is no response from server, no internet
typedef enum {OK = 0x6f, NO_RESPONSE = 0x65} t_ping_status;
t_ping_status last_ping_status = OK;

void send_sigfox_message(void)
{
    if (!bc_module_sigfox_is_ready(&sigfox))
    {
        return;
    }

    bc_led_set_mode(&led, BC_LED_MODE_ON);

    uint8_t buffer[3];

    // 0x79: y, 0x6E: n
    buffer[0] = last_is_usb_powered ? 0x79 : 0x6E;
    buffer[1] = last_ping_status;
    // we make whole number from voltage
    // voltage can be up to 3 V (2 x 1.5 V batery) so we substract 1 V to fit to 1 byte
    buffer[2] = (uint8_t)((voltage * 100) - 100);
    bc_module_sigfox_send_rf_frame(&sigfox, buffer, sizeof(buffer));

    bc_led_set_mode(&led, BC_LED_MODE_OFF);
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    bc_led_pulse(&led, 100);
    
    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        bc_module_battery_get_voltage(&voltage);
    }
}

void uart_handler(bc_uart_channel_t channel, bc_uart_event_t event, void *param)
{
    bc_led_pulse(&led, 100);
    
    if (event == BC_UART_EVENT_ASYNC_READ_DATA)
    {
        // Read data from FIFO by a single byte as you receive it
        uint8_t rx_data[5];
        size_t number_of_rx_bytes = bc_uart_async_read(BC_UART_UART2, rx_data, sizeof(rx_data));

        t_ping_status ping_status;

        if (number_of_rx_bytes != 1)
        {
            return;
        }

        // 0x6F: o, 0x65: e
        if(rx_data[0] == 0x6F) 
        {
            ping_status = OK;
        }
        
        if(rx_data[0] == 0x65) 
        {
            ping_status = NO_RESPONSE;
        }

        // acknowledge
        char buffer[4];
        sprintf(buffer, "%d\r\n", ping_status);
        bc_uart_async_write(BC_UART_UART2, buffer, strlen(buffer));

        if (ping_status == last_ping_status)
        {
            return;
        }

        last_ping_status = ping_status;
        send_sigfox_message();
    }
}


void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);
    
    // SIGFOX init
    bc_module_sigfox_init(&sigfox, BC_MODULE_SIGFOX_REVISION_R2);

    // Communication with PC
    bc_uart_init(BC_UART_UART2, BC_UART_BAUDRATE_115200, BC_UART_SETTING_8N1);
    bc_uart_set_event_handler(BC_UART_UART2, uart_handler, NULL);

    bc_fifo_init(&tx_fifo, tx_fifo_buffer, sizeof(tx_fifo_buffer));
    bc_fifo_init(&rx_fifo, rx_fifo_buffer, sizeof(rx_fifo_buffer));

    bc_uart_set_async_fifo(BC_UART_UART2, &tx_fifo, &rx_fifo);
    bc_uart_async_read_start(BC_UART_UART2, PC_COMMUNICATION_TIMEOUT);
}

void application_task(void)
{
    bc_led_pulse(&led, 100);
    
    // Check if board is connected to USB.
    // It checks USB data pins not USB power pins.
    // It doesn't work properly with USB charger. You need to change FTDI chip setup. 
    // It works with USB devices like PC, server, Wifi access point, USB hub, ...
    bool is_usb_powered  = bc_system_get_vbus_sense();
    bc_scheduler_plan_current_from_now(USB_POWER_UPDATE_INTERVAL);

    // check if power source changed since last check
    if (is_usb_powered == last_is_usb_powered)
    {
        return;
    }

    last_is_usb_powered = is_usb_powered;
    send_sigfox_message();
}

