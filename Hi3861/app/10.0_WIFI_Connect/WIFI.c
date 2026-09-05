#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hos_types.h"
#include "lwip/api_shell.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "ohos_init.h"
#include "wifi_device.h"
#include "wifiiot_errno.h"

#define DEF_TIMEOUT 15
#define ONE_SECOND 1
#define SELECT_WLAN_PORT "wlan0"
#define SELECT_WIFI_SSID "TEMP"
#define SELECT_WIFI_PASSWORD "987654321"
#define SELECT_WIFI_SECURITY_TYPE WIFI_SEC_TYPE_PSK
#define WIFI_SCAN_RETRY_LIMIT 5
#define TASK_VERSION "TASK14_WIFI_V1"

static volatile int g_scanDone = 0;
static volatile int g_connected = 0;
static volatile int g_connectResult = -1;
static int g_ssidCount = 0;
static WifiEvent g_wifiEvent;

static void WifiStaTask(void *argument);

static void OnWifiScanStateChangedHandler(int state, int size)
{
    (void)state;
    g_ssidCount = size;
    g_scanDone = 1;
}

static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info)
{
    (void)info;
    if (state > 0) {
        g_connected = 1;
        g_connectResult = 0;
    } else {
        g_connected = 0;
        g_connectResult = -1;
    }
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    (void)info;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    (void)info;
}

static void OnHotspotStateChangedHandler(int state)
{
    (void)state;
}

static int WaitForScanDone(void)
{
    int timeout = DEF_TIMEOUT;

    while (!g_scanDone && timeout > 0) {
        sleep(ONE_SECOND);
        timeout--;
    }
    return g_scanDone ? 0 : -1;
}

static int WaitForConnection(void)
{
    int timeout = DEF_TIMEOUT;

    while (!g_connected && timeout > 0) {
        sleep(ONE_SECOND);
        timeout--;
    }
    return g_connected ? 0 : -1;
}

static int FindTargetWifi(WifiScanInfo *scanResult, unsigned int resultSize)
{
    unsigned int i;

    for (i = 0; i < resultSize; i++) {
        printf("[%s][SCAN] ssid=%s rssi=%d\r\n",
            TASK_VERSION, scanResult[i].ssid, scanResult[i].rssi);
        if (strcmp(scanResult[i].ssid, SELECT_WIFI_SSID) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int ConnectTargetWifi(const WifiScanInfo *target)
{
    WifiDeviceConfig config = {0};
    int networkId = 0;
    int ret;

    strncpy(config.ssid, target->ssid, sizeof(config.ssid) - 1);
    strncpy(config.preSharedKey, SELECT_WIFI_PASSWORD,
        sizeof(config.preSharedKey) - 1);
    config.securityType = SELECT_WIFI_SECURITY_TYPE;

    ret = AddDeviceConfig(&config, &networkId);
    if (ret != WIFI_SUCCESS) {
        printf("[%s][ERROR] AddDeviceConfig=%d\r\n", TASK_VERSION, ret);
        return -1;
    }

    g_connected = 0;
    g_connectResult = -1;
    ret = ConnectTo(networkId);
    if (ret != WIFI_SUCCESS) {
        printf("[%s][ERROR] ConnectTo=%d\r\n", TASK_VERSION, ret);
        return -1;
    }

    if (WaitForConnection() != 0) {
        printf("[%s][ERROR] connect timeout, result=%d\r\n",
            TASK_VERSION, g_connectResult);
        return -1;
    }

    printf("[%s][WIFI] connected to %s\r\n", TASK_VERSION, config.ssid);
    return 0;
}

static int StartDhcp(void)
{
    struct netif *netif = netifapi_netif_find(SELECT_WLAN_PORT);
    int timeout = DEF_TIMEOUT;

    if (netif == NULL) {
        printf("[%s][ERROR] netif %s not found\r\n",
            TASK_VERSION, SELECT_WLAN_PORT);
        return -1;
    }

    if (dhcp_start(netif) != ERR_OK) {
        printf("[%s][ERROR] dhcp_start failed\r\n", TASK_VERSION);
        return -1;
    }

    while (!dhcp_is_bound(netif) && timeout > 0) {
        sleep(ONE_SECOND);
        timeout--;
    }
    if (!dhcp_is_bound(netif)) {
        printf("[%s][ERROR] DHCP timeout\r\n", TASK_VERSION);
        return -1;
    }

    printf("[%s][DHCP] bound successfully\r\n", TASK_VERSION);
    netifapi_netif_common(netif, dhcp_clients_info_show, NULL);
    return 0;
}

static void WifiStaTask(void *argument)
{
    WifiScanInfo *scanResult = NULL;
    unsigned int resultSize;
    int targetIndex = -1;
    int retry;
    int ret;

    (void)argument;
    printf("[%s] task started\r\n", TASK_VERSION);

    g_wifiEvent.OnWifiScanStateChanged = OnWifiScanStateChangedHandler;
    g_wifiEvent.OnWifiConnectionChanged = OnWifiConnectionChangedHandler;
    g_wifiEvent.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEvent.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEvent.OnHotspotStateChanged = OnHotspotStateChangedHandler;

    ret = RegisterWifiEvent(&g_wifiEvent);
    if (ret != WIFI_SUCCESS) {
        printf("[%s][ERROR] RegisterWifiEvent=%d\r\n", TASK_VERSION, ret);
        return;
    }

    ret = EnableWifi();
    if (ret != WIFI_SUCCESS) {
        printf("[%s][ERROR] EnableWifi=%d\r\n", TASK_VERSION, ret);
        return;
    }
    while (IsWifiActive() == 0) {
        sleep(ONE_SECOND);
    }
    printf("[%s][WIFI] station enabled\r\n", TASK_VERSION);

    scanResult = malloc(sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);
    if (scanResult == NULL) {
        printf("[%s][ERROR] scan buffer allocation failed\r\n", TASK_VERSION);
        return;
    }

    for (retry = 0; retry < WIFI_SCAN_RETRY_LIMIT && targetIndex < 0; retry++) {
        g_scanDone = 0;
        g_ssidCount = 0;
        ret = Scan();
        if (ret != WIFI_SUCCESS || WaitForScanDone() != 0) {
            printf("[%s][WARN] scan attempt %d failed\r\n",
                TASK_VERSION, retry + 1);
            continue;
        }

        resultSize = WIFI_SCAN_HOTSPOT_LIMIT;
        ret = GetScanInfoList(scanResult, &resultSize);
        if (ret != WIFI_SUCCESS) {
            printf("[%s][WARN] GetScanInfoList=%d\r\n", TASK_VERSION, ret);
            continue;
        }
        printf("[%s][SCAN] callback_count=%d result_count=%u\r\n",
            TASK_VERSION, g_ssidCount, resultSize);
        targetIndex = FindTargetWifi(scanResult, resultSize);
    }

    if (targetIndex < 0) {
        printf("[%s][ERROR] target SSID %s not found\r\n",
            TASK_VERSION, SELECT_WIFI_SSID);
        free(scanResult);
        return;
    }

    ret = ConnectTargetWifi(&scanResult[targetIndex]);
    free(scanResult);
    if (ret != 0) {
        return;
    }
    if (StartDhcp() != 0) {
        return;
    }

    printf("[%s][READY] WiFi connection test passed\r\n", TASK_VERSION);
    while (1) {
        sleep(ONE_SECOND);
    }
}

static void Wifi(void)
{
    osThreadAttr_t attr = {0};

    attr.name = "WifiStaTask";
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;
    if (osThreadNew(WifiStaTask, NULL, &attr) == NULL) {
        printf("[%s][ERROR] create WifiStaTask failed\r\n", TASK_VERSION);
    }
}

APP_FEATURE_INIT(Wifi);
