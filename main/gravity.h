#include <esp_wifi.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "cmd_nvs.h"
#include <stdlib.h>
#include <stdint.h>

/* Command usage strings */
extern const char USAGE_BEACON[];
extern const char USAGE_TARGET_SSIDS[];
extern const char USAGE_PROBE[];
extern const char USAGE_SNIFF[];
extern const char USAGE_DEAUTH[];
extern const char USAGE_MANA[];
extern const char USAGE_STALK[];
extern const char USAGE_AP_DOS[];
extern const char USAGE_AP_CLONE[];
extern const char USAGE_SCAN[];
extern const char USAGE_HOP[];
extern const char USAGE_SET[];
extern const char USAGE_GET[];
extern const char USAGE_VIEW[];
extern const char USAGE_SELECT[];
extern const char USAGE_CLEAR[];
extern const char USAGE_HANDSHAKE[];
extern const char USAGE_COMMANDS[];

/* Command specifications */
int cmd_beacon(int argc, char **argv);
int cmd_probe(int argc, char **argv);
int cmd_sniff(int argc, char **argv);
int cmd_deauth(int argc, char **argv);
int cmd_mana(int argc, char **argv);
int cmd_stalk(int argc, char **argv);
int cmd_ap_dos(int argc, char **argv);
int cmd_ap_clone(int argc, char **argv);
int cmd_scan(int argc, char **argv);
int cmd_set(int argc, char **argv);
int cmd_get(int argc, char **argv);
int cmd_view(int argc, char **argv);
int cmd_select(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_handshake(int argc, char **argv);
int cmd_target_ssids(int argc, char **argv);
int cmd_commands(int argc, char **argv);
int cmd_hop(int argc, char **argv);
int mac_bytes_to_string(uint8_t *bMac, char *strMac);
int mac_string_to_bytes(char *strMac, uint8_t *bMac);

enum PROBE_RESPONSE_AUTH_TYPE {
    AUTH_TYPE_NONE,
    AUTH_TYPE_WEP,
    AUTH_TYPE_WPA
};

enum HopStatus {
    HOP_STATUS_OFF,
    HOP_STATUS_ON,
    HOP_STATUS_DEFAULT
};

/* Moving attack_status and hop_defaults off the heap */
bool *attack_status;
bool *hop_defaults;
int *hop_millis_defaults;


uint8_t PRIVACY_OFF_BYTES[] = {0x01, 0x11};
uint8_t PRIVACY_ON_BYTES[] = {0x11, 0x11};

struct NetworkList {
    uint8_t bMac[6];
    char strMac[18];
    char **ssids;
    int ssidCount;
};
typedef struct NetworkList NetworkList;
static NetworkList *networkList = NULL;
static int networkCount = 0;
static enum PROBE_RESPONSE_AUTH_TYPE mana_auth = AUTH_TYPE_NONE;

static bool WIFI_INITIALISED = false;
static const char *TAG = "GRAVITY";
static const char *MANA_TAG = "mana@GRAVITY";
static const char *HOP_TAG = "hop@GRAVITY";

char scan_filter_ssid[33] = "\0";
uint8_t scan_filter_ssid_bssid[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

extern int PROBE_SSID_OFFSET;
extern int PROBE_SRCADDR_OFFSET;
extern int PROBE_DESTADDR_OFFSET;
extern int PROBE_BSSID_OFFSET;
extern int PROBE_SEQNUM_OFFSET;

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

#define CMD_COUNT 18
esp_console_cmd_t commands[CMD_COUNT] = {
    {
        .command = "beacon",
        .hint = USAGE_BEACON,
        .help = "A beacon spam attack continously transmits forged beacon frames. RICKROLL will simulate eight APs named after popular song lyrics. RANDOM will generate COUNT random SSIDs between SSID_LEN_MIN and SSID_LEN_MAX in length. If COUNT is not specified DEFAULT_SSID_COUNT is used. USER will generate SSIDs as specified in target-ssids. INIFINITE will continuously broadcast random APs until it is stopped.",
        .func = cmd_beacon
    }, {
        .command = "target-ssids",
        .hint = USAGE_TARGET_SSIDS,
        .help = "ssid-targets may be specified for several attacks, including BEACON and PROBE.",
        .func = cmd_target_ssids
    }, {
        .command = "probe",
        .hint = USAGE_PROBE,
        .help = "A probe flood attack continually transmits probe requests, imposing continual load on target APs.",
        .func = cmd_probe
    }, {
        .command = "sniff",
        .hint = USAGE_SNIFF,
        .help = "Gravity operates in promiscuous (monitor) mode at all times. This mode displays relevant information about interesting packets as they are received.",
        .func = cmd_sniff
    }, {
        .command = "deauth",
        .hint = USAGE_DEAUTH,
        .help = "Arguments:   <millis>: Time to wait between packets.  FRAME | DEVICE | SPOOF : Deauth packet is sent with the AP's source address, the device's source address, or the device's MAC is changed to match AP.  STA: Send deauthentication packets to selected stations.   BROADCAST: Send broadcast deauthentication packets.   OFF: Disable a running deauthentication attack.   No argument: Return the current status of the module.   Deauthentication frames are intended to be issued by an AP to instruct connected STAs to disconnect before the AP makes a change that could affect the connection. This obviously makes it trivial to observe a 4-way handshake and obtain key material, and as a consequence of this many - perhaps even the majority of - wireless devices will disregard a broadcast deauthentication packet. This attack will be much more effective if specific stations are selected as targets. Success will be greater still if you adopt the MAC of the Access Point you are attempting to deauthenticate stations from.",
        .func = cmd_deauth
    } , {
        .command = "mana",
        .hint = USAGE_MANA,
        .help = "Call without arguments to obtain the current status of the module.  Including the verbose keyword will enable or disable verbose logging as the attack progresses.  Default authentication type is NONE.  The Mana attack is a way to 'trick' stations into connecting to a rogue access point. With Mana enabled the AP will respond to all directed probe requests, impersonating any SSID a STA is searching for. If the STA expects any of these SSIDs to have open (i.e. no) authentication the STA will then establish a connection with the AP. The only criterion for vulnerability is that the station has at least one open/unsecured SSID saved in its WiFi history.",
        .func = cmd_mana
    }, {
        .command = "stalk",
        .hint = USAGE_STALK,
        .help = "Displays signal strength information for selected wireless devices, allowing their location to be rapidly determined.  Where selected, this attack will include both stations and access points in its tracking - while it's unlikely you'll ever need to track STAs and APs simultaneously, it's far from impossible. While selecting multiple devices will improve your accuracy and reliability, it also increases the likelihood that selected devices won't all remain in proximity with each other.",
        .func = cmd_stalk
    }, {
        .command = "ap-dos",
        .hint = USAGE_AP_DOS,
        .help = "If no argument is provided returns the current state of this module. This attack targets selected SSIDs.  This attack attempts to interrupt all communication and disconnect all stations from selected access points. When a frame addressed to a target AP is observed a deauthentication packet is created as a reply to the sender, specifying the target AP as the sender. TODO: If we can identify STAs that are associated with target APs send directed deauth frames to them. Perhaps this could be 'DOS Mk. II'",
        .func = cmd_ap_dos
    }, {
        .command = "ap-clone",
        .hint = USAGE_AP_CLONE,
        .help = "If no argument is provided returns the current state of the module. To start the attack issue the ap_clone command with the target AP's MAC address as a parameter.  Clone the specified AP and attempt to coerce STAs to connect to the cloned AP instead of the authentic one. The success of this attack is dependent upon being able to generate a more powerful signal than the genuine AP. The attack will set ESP32C6's MAC and SSID to match the AP's, run a deauth attack until there are no associated STAs or a predetermined time has passed, then disable the deauth attack and hope that STAs connect to the rogue AP.",
        .func = cmd_ap_clone
    }, {
        .command = "scan",
        .hint = USAGE_SCAN,
        .help = "No argument returns scan status.   ON: Initiate a continuous scan for 802.11 APs and STAs.   Specifying a value for <ssid> will capture only frames from that SSID.  Scan wireless frequencies to identify access points and stations in range. Most modules in this application require one or more target APs and/or STAs so you will run these commands frequently. The scan types commence an open-ended analysis of received packets, and will continue updating until they are stopped. To assist in identifying contemporary devices these scan types also capture a timestamp of when the device was last seen.",
        .func = cmd_scan
    }, {
        .command = "hop",
        .hint = USAGE_HOP,
        .help = "Enable or disable channel hopping, and set the frequency of hops. The KILL option terminates the event loop.",
        .func = cmd_hop
    }, {
        .command = "set",
        .hint = USAGE_SET,
        .help = "Set a variety of variables that affect various components of the application. Usage: set <variable> <value>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   MAC_RAND: Whether to change the device's MAC after each packet (default: ON)   DEFAULT_SSID_COUNT: Number of SSIDs to generate if not specified   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_set
    }, {
        .command = "get",
        .hint = USAGE_GET,
        .help = "Get a variety of variables that affect various components of the application. Usage: get <variable>   <variable>   SSID_LEN_MIN: Minimum length of a generated SSID   SSID_LEN_MAX: Maximum length of a generated SSID   MAC_RAND: Whether to change the device's MAC after each packet (default: ON)   CHANNEL: Wireless channel   MAC: ASP32C6's MAC address   HOP_MILLIS: Milliseconds to stay on a channel before hopping to the next (0: Hopping disabled)   ATTACK_PKTS: Number of times to repeat an attack packet when launching an attack (0: Don't stop attacks based on packet count)   ATTACK_MILLIS: Milliseconds to run an attack for when it is launched (0: Don't stop attacks based on duration)   NB: If ATTACK_PKTS and ATTACK_MILLIS are both 0 attacks will not end automatically but will continue until terminated.",
        .func = cmd_get
    }, {
        .command = "view",
        .hint = USAGE_VIEW,
        .help = "VIEW is a fundamental command in this framework, with the typical workflow being Scan-View-Select-Attack. Multiple result sets can be viewed in a single command using, for example, VIEW STA AP.",
        .func = cmd_view
    }, {
        .command = "select",
        .hint = USAGE_SELECT,
        .help = "Select the specified element from the specified scan results. Usage: select ( AP | STA ) <elementId>.  Selects/deselects item <elementId> from the AP or STA list. Multiple items can be specified separated by spaces.",
        .func = cmd_select
    }, {
        .command = "clear",
        .hint = USAGE_CLEAR,
        .help = "Clear the specified list.",
        .func = cmd_clear
    }, {
        .command = "handshake",
        .hint = USAGE_HANDSHAKE,
        .help = "Toggle monitoring of 802.11 traffic for a 4-way handshake to obtain key material from. Usage: handshake",
        .func = cmd_handshake
    }, {
        .command = "commands",
        .hint = USAGE_COMMANDS,
        .help = "Display a BRIEF command summary",
        .func = cmd_commands
    }
};
