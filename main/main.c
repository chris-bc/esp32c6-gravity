/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "main.h"

static const char* TAG = "GRAVITY";
#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

int cmd_beacon(int argc, char **argv) {
    /* rickroll | random | user | off | status */
    /* Initially the 'TARGET MAC' argument is unsupported, the attack only supports broadcast beacon frames */
    /* argc must be 1 or 2 - no arguments, or rickroll/random/user/off */
    if (argc < 1 || argc > 3) {
        ESP_LOGE(TAG, "Invalid arguments specified. Expected 0 or 1, received %d.", argc - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 1) {
        ESP_LOGI(TAG, "Beacon Status: %s", attack_status[ATTACK_BEACON]?"Running":"Not Running");
        return ESP_OK;
    }

    /* Handle arguments to beacon */
    int ret = ESP_OK;
    int ssidCount = DEFAULT_SSID_COUNT;
    if (!strcasecmp(argv[1], "rickroll")) {
        ret = beacon_start(ATTACK_BEACON_RICKROLL, 0);
    } else if (!strcasecmp(argv[1], "random")) {
        if (SSID_LEN_MIN == 0) {
            SSID_LEN_MIN = 8;
        }
        if (SSID_LEN_MAX == 0) {
            SSID_LEN_MAX = 32;
        }
        if (argc == 3) {
            ssidCount = atoi(argv[2]);
            if (ssidCount == 0) {
                ssidCount = DEFAULT_SSID_COUNT;
            }
        }
        ret = beacon_start(ATTACK_BEACON_RANDOM, ssidCount);
    } else if (!strcasecmp(argv[1], "user")) {
        ret = beacon_start(ATTACK_BEACON_USER, 0);
    } else if (!strcasecmp(argv[1], "off")) {
        ret = beacon_stop();
    } else {
        ESP_LOGE(TAG, "Invalid argument provided to BEACON: \"%s\"", argv[1]);
        return ESP_ERR_INVALID_ARG;
    }

    // Update attack_status[ATTACK_BEACON] appropriately
    if (ret == ESP_OK) {
        if (!strcasecmp(argv[1], "off")) {
            attack_status[ATTACK_BEACON] = false;
        } else {
            attack_status[ATTACK_BEACON] = true;
        }
    }
    return ret;
}

int cmd_target_ssids(int argc, char **argv) {
    // Must have no args (return current value) or two (add/remove SSID)
    if ((argc != 1 && argc != 3) || (argc == 1 && user_ssid_count == 0)) {
        if (user_ssid_count == 0) {
            ESP_LOGI(TAG, "targt-ssids has no elements. user_ssids is at %p", user_ssids);
        } else {
            ESP_LOGE(TAG, "target-ssids must have either no arguments, to return its current value, or two arguments: ADD/REMOVE <ssid>");
        }
        return ESP_ERR_INVALID_ARG;
    }
    char temp[40];
    if (argc == 1) {
        char *strSsids = malloc(sizeof(char) * user_ssid_count * 32);
        if (strSsids == NULL) {
            ESP_LOGE(TAG, "Unable to allocate memory to display user SSIDs");
            return ESP_ERR_NO_MEM;
        }
        printf("Serialising target SSIDs");
        strcpy(strSsids, user_ssids[0]);
        printf("Before serialisation loop returned value is \"%s\"\n", strSsids);
        for (int i = 1; i < user_ssid_count; ++i) {
            sprintf(temp, " , %s", user_ssids[i]);
            strcat(strSsids, temp);
            printf("At the end of iteration %d retVal is \"%s\"\n",i, strSsids);
        }
        printf("Selected SSIDs: %s\n", strSsids);
    } else if (!strcasecmp(argv[1], "add")) {
        return addSsid(argv[2]);
    } else if (!strcasecmp(argv[1], "remove")) {
        return rmSsid(argv[2]);
    }

    return ESP_OK;
}

int cmd_probe(int argc, char **argv) {

    return ESP_OK;
}

int cmd_deauth(int argc, char **argv) {

    return ESP_OK;
}

int cmd_mana(int argc, char **argv) {

    return ESP_OK;
}

int cmd_stalk(int argc, char **argv) {

    return ESP_OK;
}

int cmd_ap_dos(int argc, char **argv) {

    return ESP_OK;
}

int cmd_ap_clone(int argc, char **argv) {

    return ESP_OK;
}

int cmd_scan(int argc, char **argv) {

    return ESP_OK;
}

int cmd_set(int argc, char **argv) {

    return ESP_OK;
}

int cmd_get(int argc, char **argv) {

    return ESP_OK;
}

int cmd_view(int argc, char **argv) {

    return ESP_OK;
}

int cmd_select(int argc, char **argv) {

    return ESP_OK;
}

int cmd_handshake(int argc, char **argv) {

    return ESP_OK;
}

int initialise_wifi() {
    /* Initialise WiFi if needed */
    if (!WIFI_INITIALISED) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        /* Init dummy AP to specify a channel and get WiFi hardware into a
           mode where we can send the actual fake beacon frames. */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        wifi_config_t ap_config = {
            .ap = {
                .ssid = "ManagementAP",
                .ssid_len = 12,
                .password = "management",
                .channel = 1,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .ssid_hidden = 0,
                .max_connection = 128,
                .beacon_interval = 5000
            }
        };

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        WIFI_INITIALISED = true;
    }
    return ESP_OK;
}

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static int register_gravity_commands() {
    esp_err_t err;
    for (int i=0; i < CMD_COUNT; ++i) {
        err = esp_console_cmd_register(&commands[i]);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Registered command \"%s\"...", commands[i].command);
            break;
        case ESP_ERR_NO_MEM:
            ESP_LOGE(TAG, "Out of memory registering command \"%s\"!", commands[i].command);
            return ESP_ERR_NO_MEM;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGW(TAG, "Invalid arguments provided during registration of \"%s\". Skipping...", commands[i].command);
            continue;
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    initialise_wifi();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_gravity_commands();
    /*register_nvs();*/

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
