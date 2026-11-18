#include "app_common.h"
#include "web_portal_html.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_log.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "APP_TRANSFER";

#define AP_SSID_NAME      "InkSprout-Kids"
#define AP_IP_STR         "192.168.4.1"
#define NET_CFG_MAGIC     0x494E4B32UL  /* "INK2" */

#define STA_CONNECTED_BIT BIT0
#define STA_FAIL_BIT      BIT1

app_net_cfg_t g_net_cfg;

static bool               s_wifi_stack_inited = false;
static EventGroupHandle_t s_wifi_ev = NULL;
static volatile int       s_sta_retries = 0;
static char               s_sta_ip_str[24] = "";
static httpd_handle_t     s_httpd = NULL;
static volatile bool      s_dns_running = false;
static volatile int       s_ap_clients = 0;
static volatile int       s_rx_count = 0;
static volatile int       s_cfg_updates = 0;
static bool               s_focus_ai_tab = true;
static char               s_last_rx_name[96] = "等待手机配网或传书...";

void app_get_device_mac_str(char *out18)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out18, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

const char *app_ai_provider_label(uint8_t prov)
{
    switch (prov) {
    case AI_PROV_XIAOZHI:  return "小智AI(语音)";
    case AI_PROV_DEEPSEEK: return "DeepSeek";
    case AI_PROV_QWEN:     return "通义千问";
    case AI_PROV_DOUBAO:   return "豆包大模型";
    case AI_PROV_SILICON:  return "硅基流动";
    default:               return "自定义接口";
    }
}

static void generate_uuid_v4(char *out40)
{
    uint32_t r0 = esp_random(), r1 = esp_random(), r2 = esp_random(), r3 = esp_random();
    snprintf(out40, 40, "%08lx-%04lx-4%03lx-%04lx-%04lx%08lx",
             (unsigned long)r0,
             (unsigned long)((r1 >> 16) & 0xFFFF),
             (unsigned long)(r1 & 0x0FFF),
             (unsigned long)(((r2 >> 16) & 0x3FFF) | 0x8000),
             (unsigned long)(r2 & 0xFFFF),
             (unsigned long)r3);
}

void app_net_cfg_load(void)
{
    memset(&g_net_cfg, 0, sizeof(g_net_cfg));
    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(g_net_cfg);
        nvs_get_blob(h, "net_cfg_v1", &g_net_cfg, &sz);
        nvs_close(h);
    }
    if (g_net_cfg.magic != NET_CFG_MAGIC) {
        memset(&g_net_cfg, 0, sizeof(g_net_cfg));
        g_net_cfg.magic = NET_CFG_MAGIC;
        strcpy(g_net_cfg.child_name, "宝贝");
        g_net_cfg.age_mode = 0;
        g_net_cfg.ai_provider = AI_PROV_XIAOZHI;
        strcpy(g_net_cfg.ai_url, "https://api.tenclass.net/xiaozhi/ota/");
        strcpy(g_net_cfg.ai_model, "xiaozhi-v1");
        strcpy(g_net_cfg.tts_url, "https://api.siliconflow.cn/v1/audio/speech");
        strcpy(g_net_cfg.tts_model, "FunAudioLLM/CosyVoice2-0.5B");
        strcpy(g_net_cfg.tts_voice, "FunAudioLLM/CosyVoice2-0.5B:anna");
        generate_uuid_v4(g_net_cfg.xiaozhi_uuid);
        app_net_cfg_save();
    } else if (g_net_cfg.xiaozhi_uuid[0] == '\0') {
        generate_uuid_v4(g_net_cfg.xiaozhi_uuid);
        app_net_cfg_save();
    }
}

void app_net_cfg_save(void)
{
    g_net_cfg.magic = NET_CFG_MAGIC;
    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "net_cfg_v1", &g_net_cfg, sizeof(g_net_cfg));
        nvs_commit(h);
        nvs_close(h);
    }
}

bool app_wifi_has_credentials(void)
{
    return g_net_cfg.wifi_ssid[0] != '\0';
}

/* === URL Decode & Filename Sanitizer === */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void url_decode_clean_filename(const char *src, char *dst, size_t dst_sz)
{
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < dst_sz; i++) {
        char ch = src[i];
        if (ch == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            ch = (char)((hex_val(src[i + 1]) << 4) | hex_val(src[i + 2]));
            i += 2;
        } else if (ch == '+') {
            ch = ' ';
        }
        /* Strip path separators or illegal FAT chars */
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
            continue;
        }
        dst[o++] = ch;
    }
    dst[o] = '\0';
    while (dst[0] == '.' || dst[0] == ' ') {
        memmove(dst, dst + 1, strlen(dst));
    }
    if (dst[0] == '\0') {
        strncpy(dst, "story.txt", dst_sz - 1);
    }
}

/* === JSON String Escape Helper === */
static void json_escape(const char *in, char *out, size_t out_sz)
{
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 3 < out_sz; i++) {
        if (in[i] == '"' || in[i] == '\\') out[o++] = '\\';
        if ((unsigned char)in[i] >= 0x20) out[o++] = in[i];
    }
    out[o] = '\0';
}

/* === HTTP Handlers === */
static esp_err_t http_root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, k_web_portal_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_list_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    uint32_t tot_kb = 0, free_kb = 0;
    bsp_storage_get_space_kb(&tot_kb, &free_kb);

    char hdr_json[256];
    snprintf(hdr_json, sizeof(hdr_json),
             "{\"medium\":\"%s\",\"total_kb\":%lu,\"free_kb\":%lu,\"files\":[",
             bsp_storage_get_medium_label(), (unsigned long)tot_kb, (unsigned long)free_kb);
    httpd_resp_sendstr_chunk(req, hdr_json);

    DIR *dir = opendir(bsp_storage_get_stories_dir());
    bool first_item = true;
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            size_t len = strlen(ent->d_name);
            bool is_txt = (len > 4 && strcasecmp(ent->d_name + len - 4, ".txt") == 0);
            bool is_wav = (len > 4 && strcasecmp(ent->d_name + len - 4, ".wav") == 0);
            if (!is_txt && !is_wav) continue;

            char fpath[320];
            snprintf(fpath, sizeof(fpath), "%s/%s", bsp_storage_get_stories_dir(), ent->d_name);
            struct stat st = {0};
            stat(fpath, &st);

            char esc[192], item_json[320];
            json_escape(ent->d_name, esc, sizeof(esc));
            snprintf(item_json, sizeof(item_json), "%s{\"name\":\"%s\",\"size\":%ld,\"wav\":%s}",
                     first_item ? "" : ",", esc, (long)st.st_size, is_wav ? "true" : "false");
            httpd_resp_sendstr_chunk(req, item_json);
            first_item = false;
        }
        closedir(dir);
    }
    httpd_resp_sendstr_chunk(req, "]}");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static bool extract_name_query(httpd_req_t *req, char *clean_out, size_t out_sz)
{
    char qbuf[256] = {0};
    char raw_name[192] = {0};
    if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) != ESP_OK) return false;
    if (httpd_query_key_value(qbuf, "name", raw_name, sizeof(raw_name)) != ESP_OK) return false;
    url_decode_clean_filename(raw_name, clean_out, out_sz);
    size_t l = strlen(clean_out);
    if (l <= 4) return false;
    bool ok_ext = (strcasecmp(clean_out + l - 4, ".txt") == 0) ||
                  (strcasecmp(clean_out + l - 4, ".wav") == 0);
    return ok_ext;
}

static esp_err_t http_upload_post(httpd_req_t *req)
{
    char fname[128] = {0};
    if (!extract_name_query(req, fname, sizeof(fname))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid .txt/.wav filename");
        return ESP_FAIL;
    }

    mkdir(bsp_storage_get_stories_dir(), 0777);
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", bsp_storage_get_stories_dir(), fname);

    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Storage full or write error");
        return ESP_FAIL;
    }

    char *chunk = (char *)malloc(4096);
    if (!chunk) {
        fclose(fp);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    size_t remaining = req->content_len;
    bool write_ok = true;
    while (remaining > 0) {
        int to_read = (remaining > 4096) ? 4096 : (int)remaining;
        int r = httpd_req_recv(req, chunk, to_read);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            write_ok = false;
            break;
        }
        if (fwrite(chunk, 1, (size_t)r, fp) != (size_t)r) {
            write_ok = false;
            break;
        }
        remaining -= (size_t)r;
    }
    free(chunk);
    fclose(fp);

    if (!write_ok) {
        unlink(fullpath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Interrupted or disk full");
        return ESP_FAIL;
    }

    strncpy(s_last_rx_name, fname, sizeof(s_last_rx_name) - 1);
    s_rx_count++;
    bsp_audio_sfx(SFX_SUCCESS);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t http_delete_post(httpd_req_t *req)
{
    char fname[128] = {0};
    if (!extract_name_query(req, fname, sizeof(fname))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad filename");
        return ESP_FAIL;
    }
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", bsp_storage_get_stories_dir(), fname);
    bsp_audio_stop_all();
    unlink(fullpath);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t http_play_post(httpd_req_t *req)
{
    char fname[128] = {0};
    if (!extract_name_query(req, fname, sizeof(fname))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad filename");
        return ESP_FAIL;
    }
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", bsp_storage_get_stories_dir(), fname);
    bsp_audio_play_wav(fullpath);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t http_captive_redirect(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP_STR "/");
    return httpd_resp_send(req, NULL, 0);
}

/* === Lightweight Captive Portal UDP DNS Task (Port 53 -> 192.168.4.1) === */
static void captive_dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        s_dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    struct timeval tv = { .tv_sec = 0, .tv_usec = 350000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        s_dns_running = false;
        vTaskDelete(NULL);
        return;
    }

    uint8_t rx[256];
    uint8_t tx[300];
    while (s_dns_running) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int n = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&client, &clen);
        if (n < 17 || n > 240) continue;
        if (rx[2] & 0x80) continue; /* Ignore responses */

        /* Find end of first QNAME + QTYPE(2) + QCLASS(2) */
        int pos = 12;
        while (pos < n && rx[pos] != 0) {
            pos += 1 + rx[pos];
        }
        pos += 5; /* zero byte + QTYPE + QCLASS */
        if (pos > n) continue;

        memcpy(tx, rx, pos);
        tx[2] = 0x81; tx[3] = 0x80; /* Standard query response, No error */
        tx[4] = 0x00; tx[5] = 0x01; /* QDCOUNT = 1 */
        tx[6] = 0x00; tx[7] = 0x01; /* ANCOUNT = 1 */
        tx[8] = tx[9] = tx[10] = tx[11] = 0;

        uint8_t ans[16] = {
            0xC0, 0x0C,             /* Name pointer to offset 12 */
            0x00, 0x01,             /* Type A */
            0x00, 0x01,             /* Class IN */
            0x00, 0x00, 0x00, 0x3C, /* TTL 60s */
            0x00, 0x04,             /* IPv4 length */
            192, 168, 4, 1
        };
        memcpy(tx + pos, ans, 16);
        sendto(sock, tx, pos + 16, 0, (struct sockaddr *)&client, clen);
    }

    close(sock);
    vTaskDelete(NULL);
}

static void wifi_unified_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_AP_STACONNECTED) {
            s_ap_clients++;
        } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
            if (s_ap_clients > 0) s_ap_clients--;
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_sta_ip_str[0] = '\0';
            if (s_wifi_ev) {
                if (s_sta_retries < 3) {
                    s_sta_retries++;
                    esp_wifi_connect();
                } else {
                    xEventGroupSetBits(s_wifi_ev, STA_FAIL_BIT);
                }
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_retries = 0;
        if (s_wifi_ev) xEventGroupSetBits(s_wifi_ev, STA_CONNECTED_BIT);
    }
}

static void ensure_wifi_stack_inited(void)
{
    if (s_wifi_stack_inited) return;
    s_wifi_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t ev_err = esp_event_loop_create_default();
    if (ev_err != ESP_OK && ev_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ev_err);
    }
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_unified_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_unified_event_handler, NULL, NULL);
    s_wifi_stack_inited = true;
}

bool app_wifi_connect_sta(int timeout_ms, char *ip_out, int ip_out_len)
{
    if (g_net_cfg.wifi_ssid[0] == '\0') return false;
    ensure_wifi_stack_inited();

    xEventGroupClearBits(s_wifi_ev, STA_CONNECTED_BIT | STA_FAIL_BIT);
    s_sta_retries = 0;
    s_sta_ip_str[0] = '\0';

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, g_net_cfg.wifi_ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, g_net_cfg.wifi_pass, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = (g_net_cfg.wifi_pass[0] != '\0') ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    /* If SoftAP Portal is running, stay in APSTA so parent's phone stays connected! */
    if (s_httpd == NULL) {
        esp_wifi_stop();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        esp_wifi_disconnect();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    }
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_ev, STA_CONNECTED_BIT | STA_FAIL_BIT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    if (bits & STA_CONNECTED_BIT) {
        if (ip_out && ip_out_len > 0) {
            strncpy(ip_out, s_sta_ip_str, (size_t)ip_out_len - 1);
            ip_out[ip_out_len - 1] = '\0';
        }
        return true;
    }
    return false;
}

void app_wifi_shutdown(void)
{
    if (!s_wifi_stack_inited || s_httpd != NULL) return;
    esp_wifi_disconnect();
    esp_wifi_stop();
}

/* === Provisioning & AI Config REST Endpoints === */
static esp_err_t http_config_get(httpd_req_t *req)
{
    char mac[18];
    app_get_device_mac_str(mac);

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "wifi_ssid", g_net_cfg.wifi_ssid);
    cJSON_AddStringToObject(j, "wifi_pass", g_net_cfg.wifi_pass);
    cJSON_AddStringToObject(j, "child_name", g_net_cfg.child_name);
    cJSON_AddNumberToObject(j, "age_mode", g_net_cfg.age_mode);
    cJSON_AddNumberToObject(j, "ai_provider", g_net_cfg.ai_provider);
    cJSON_AddStringToObject(j, "ai_url", g_net_cfg.ai_url);
    cJSON_AddStringToObject(j, "ai_key", g_net_cfg.ai_key);
    cJSON_AddStringToObject(j, "ai_model", g_net_cfg.ai_model);
    cJSON_AddNumberToObject(j, "tts_enabled", g_net_cfg.tts_enabled);
    cJSON_AddStringToObject(j, "tts_url", g_net_cfg.tts_url);
    cJSON_AddStringToObject(j, "tts_key", g_net_cfg.tts_key);
    cJSON_AddStringToObject(j, "tts_model", g_net_cfg.tts_model);
    cJSON_AddStringToObject(j, "tts_voice", g_net_cfg.tts_voice);
    cJSON_AddStringToObject(j, "mac", mac);
    cJSON_AddStringToObject(j, "sta_ip", s_sta_ip_str);
    cJSON_AddNumberToObject(j, "focus_ai", s_focus_ai_tab ? 1 : 0);

    char *out = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    esp_err_t ret = httpd_resp_sendstr(req, out ? out : "{}");
    if (out) free(out);
    return ret;
}

static void copy_json_str(cJSON *obj, const char *key, char *dst, size_t dst_sz)
{
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(it) && it->valuestring) {
        strncpy(dst, it->valuestring, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
    }
}

static esp_err_t http_config_post(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON size");
        return ESP_FAIL;
    }
    char *buf = (char *)calloc(1, req->content_len + 1);
    if (!buf) return ESP_FAIL;
    int got = httpd_req_recv(req, buf, req->content_len);
    if (got <= 0) { free(buf); return ESP_FAIL; }

    cJSON *j = cJSON_Parse(buf);
    free(buf);
    if (!j) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    copy_json_str(j, "wifi_ssid", g_net_cfg.wifi_ssid, sizeof(g_net_cfg.wifi_ssid));
    copy_json_str(j, "wifi_pass", g_net_cfg.wifi_pass, sizeof(g_net_cfg.wifi_pass));
    copy_json_str(j, "child_name", g_net_cfg.child_name, sizeof(g_net_cfg.child_name));
    copy_json_str(j, "ai_url", g_net_cfg.ai_url, sizeof(g_net_cfg.ai_url));
    copy_json_str(j, "ai_key", g_net_cfg.ai_key, sizeof(g_net_cfg.ai_key));
    copy_json_str(j, "ai_model", g_net_cfg.ai_model, sizeof(g_net_cfg.ai_model));
    copy_json_str(j, "tts_url", g_net_cfg.tts_url, sizeof(g_net_cfg.tts_url));
    copy_json_str(j, "tts_key", g_net_cfg.tts_key, sizeof(g_net_cfg.tts_key));
    copy_json_str(j, "tts_model", g_net_cfg.tts_model, sizeof(g_net_cfg.tts_model));
    copy_json_str(j, "tts_voice", g_net_cfg.tts_voice, sizeof(g_net_cfg.tts_voice));

    cJSON *age = cJSON_GetObjectItem(j, "age_mode");
    if (cJSON_IsNumber(age)) g_net_cfg.age_mode = (uint8_t)(age->valueint ? 1 : 0);
    cJSON *prv = cJSON_GetObjectItem(j, "ai_provider");
    if (cJSON_IsNumber(prv)) g_net_cfg.ai_provider = (uint8_t)prv->valueint;
    cJSON *ten = cJSON_GetObjectItem(j, "tts_enabled");
    if (cJSON_IsNumber(ten)) g_net_cfg.tts_enabled = (uint8_t)(ten->valueint ? 1 : 0);
    cJSON_Delete(j);

    app_net_cfg_save();
    snprintf(s_last_rx_name, sizeof(s_last_rx_name), "已存配网：%s · %s",
             g_net_cfg.wifi_ssid[0] ? g_net_cfg.wifi_ssid : "未填Wi-Fi",
             app_ai_provider_label(g_net_cfg.ai_provider));
    s_cfg_updates++;
    bsp_audio_sfx(SFX_SUCCESS);

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t http_wifi_scan_get(httpd_req_t *req)
{
    wifi_scan_config_t sc = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 50,
        .scan_time.active.max = 110,
    };
    esp_wifi_disconnect();
    esp_wifi_scan_start(&sc, true);

    uint16_t ap_num = 18;
    wifi_ap_record_t *recs = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
    if (recs) esp_wifi_scan_get_ap_records(&ap_num, recs);
    else ap_num = 0;

    cJSON *arr = cJSON_CreateArray();
    for (uint16_t i = 0; i < ap_num; i++) {
        const char *ssid = (const char *)recs[i].ssid;
        if (!ssid[0]) continue;
        bool dup = false;
        for (uint16_t k = 0; k < i; k++) {
            if (strcmp((const char *)recs[k].ssid, ssid) == 0) { dup = true; break; }
        }
        if (dup) continue;
        cJSON *it = cJSON_CreateObject();
        cJSON_AddStringToObject(it, "ssid", ssid);
        cJSON_AddNumberToObject(it, "rssi", recs[i].rssi);
        cJSON_AddItemToArray(arr, it);
    }
    if (recs) free(recs);

    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    esp_err_t ret = httpd_resp_sendstr(req, out ? out : "[]");
    if (out) free(out);
    return ret;
}

static esp_err_t http_wifi_test_post(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 512) return ESP_FAIL;
    char buf[516] = {0};
    if (httpd_req_recv(req, buf, req->content_len) <= 0) return ESP_FAIL;
    cJSON *j = cJSON_Parse(buf);
    if (!j) return ESP_FAIL;

    copy_json_str(j, "wifi_ssid", g_net_cfg.wifi_ssid, sizeof(g_net_cfg.wifi_ssid));
    copy_json_str(j, "wifi_pass", g_net_cfg.wifi_pass, sizeof(g_net_cfg.wifi_pass));
    cJSON_Delete(j);
    app_net_cfg_save();

    char ip[24] = {0};
    bool ok = app_wifi_connect_sta(9500, ip, sizeof(ip));
    char resp[192];
    if (ok) {
        snprintf(s_last_rx_name, sizeof(s_last_rx_name), "路由器已连：%s (%s)", g_net_cfg.wifi_ssid, ip);
        s_cfg_updates++;
        bsp_audio_sfx(SFX_SUCCESS);
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"ip\":\"%s\"}", ip);
    } else {
        bsp_audio_sfx(SFX_WRONG);
        snprintf(resp, sizeof(resp), "{\"ok\":false,\"msg\":\"连接失败，请确认是2.4GHz Wi-Fi且密码正确\"}");
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_sendstr(req, resp);
}

static bool hotspot_server_start(bool focus_ai)
{
    ensure_wifi_stack_inited();
    s_focus_ai_tab = focus_ai;
    s_ap_clients = 0;

    wifi_config_t wcfg = {
        .ap = {
            .ssid = AP_SSID_NAME,
            .ssid_len = strlen(AP_SSID_NAME),
            .channel = 6,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.max_uri_handlers = 12;
    hcfg.stack_size = 10240;
    hcfg.lru_purge_enable = true;
    if (httpd_start(&s_httpd, &hcfg) == ESP_OK) {
        const httpd_uri_t u_root   = { .uri = "/",              .method = HTTP_GET,  .handler = http_root_get      };
        const httpd_uri_t u_list   = { .uri = "/api/list",      .method = HTTP_GET,  .handler = http_list_get      };
        const httpd_uri_t u_upload = { .uri = "/api/upload",    .method = HTTP_POST, .handler = http_upload_post   };
        const httpd_uri_t u_delete = { .uri = "/api/delete",    .method = HTTP_POST, .handler = http_delete_post   };
        const httpd_uri_t u_play   = { .uri = "/api/play",      .method = HTTP_POST, .handler = http_play_post     };
        const httpd_uri_t u_cfg_g  = { .uri = "/api/config",    .method = HTTP_GET,  .handler = http_config_get    };
        const httpd_uri_t u_cfg_p  = { .uri = "/api/config",    .method = HTTP_POST, .handler = http_config_post   };
        const httpd_uri_t u_wscan  = { .uri = "/api/wifi/scan", .method = HTTP_GET,  .handler = http_wifi_scan_get };
        const httpd_uri_t u_wtest  = { .uri = "/api/wifi/test", .method = HTTP_POST, .handler = http_wifi_test_post };
        httpd_register_uri_handler(s_httpd, &u_root);
        httpd_register_uri_handler(s_httpd, &u_list);
        httpd_register_uri_handler(s_httpd, &u_upload);
        httpd_register_uri_handler(s_httpd, &u_delete);
        httpd_register_uri_handler(s_httpd, &u_play);
        httpd_register_uri_handler(s_httpd, &u_cfg_g);
        httpd_register_uri_handler(s_httpd, &u_cfg_p);
        httpd_register_uri_handler(s_httpd, &u_wscan);
        httpd_register_uri_handler(s_httpd, &u_wtest);
        httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, http_captive_redirect);
    }

    s_dns_running = true;
    xTaskCreate(captive_dns_task, "captive_dns", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "APSTA '%s' live at http://%s (focus_ai=%d)", AP_SSID_NAME, AP_IP_STR, focus_ai);
    return true;
}

static void hotspot_server_stop(void)
{
    bsp_audio_stop_all();
    s_dns_running = false;
    vTaskDelay(pdMS_TO_TICKS(420));
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
}

/* =========================================================================
 * MODE 1 UI: USB 电脑优盘直连传故事 (TinyUSB MSC)
 * ========================================================================= */
static void run_usb_drive_transfer(void)
{
    if (bsp_usb_msc_start() != ESP_OK) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("USB 电脑优盘传故事", g_prof.stars, app_eye_mins_left());
        gfx_draw_text_center(GFX_W / 2, 210, "未检测到可用存储分区，请检查存储设置。", FONT_24, GFX_BLACK);
        bsp_epd_smart_present(gfx_get_fb(), true);
        bsp_input_wait(3000);
        return;
    }

    bool last_conn = !bsp_usb_msc_is_host_connected();
    bool first = true;

    while (1) {
        bool conn = bsp_usb_msc_is_host_connected();
        if (first || conn != last_conn) {
            last_conn = conn;
            gfx_clear(GFX_WHITE);
            gfx_draw_top_bar("★ USB 电脑优盘传故事 (免驱拖拽)", g_prof.stars, app_eye_mins_left());

            gfx_round_rect(32, 58, GFX_W - 64, 362, 18, 3, GFX_BLACK);
            gfx_draw_art(155, 225, 165, ART_LAPTOP_USB, GFX_BLACK);

            char med[96];
            snprintf(med, sizeof(med), "当前优盘介质：%s", bsp_storage_get_medium_label());
            gfx_draw_text(285, 84, med, FONT_24, GFX_BLACK);

            if (conn) {
                gfx_fill_round_rect(285, 126, 455, 48, 12, GFX_BLACK);
                gfx_draw_text(305, 138, "✓ 已连接电脑！优盘名：INKSPROUT", FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(285, 126, 455, 48, 12, 2, GFX_BLACK);
                gfx_draw_text(305, 138, "○ 请用 Type-C 数据线连接电脑 USB 口...", FONT_24, GFX_BLACK);
            }

            gfx_draw_paragraph(285, 194, 455, 195, 10,
                "电脑传书三步曲：\n"
                "1. 在电脑打开新弹出的优盘，进入 stories 文件夹\n"
                "2. 拖入 UTF-8 .txt 故事或 16kHz .wav 有声音频\n"
                "3. 先在电脑『弹出优盘』，再按本机滚轮完成同步！\n"
                "(Mac 也可运行 tools/send_story.py 自动配音传书)",
                FONT_16, GFX_BLACK, NULL);

            gfx_draw_bottom_hints("电脑拖拽传文件中", "按下完成并断开优盘", "安全退出优盘模式");
            bsp_epd_smart_present(gfx_get_fb(), first);
            first = false;
        }

        bsp_key_t k = bsp_input_wait(600);
        if (k == KEY_OK || k == KEY_BACK) {
            break;
        }
    }

    bsp_usb_msc_stop();
    int total_now = bsp_storage_count_stories();
    bsp_audio_sfx(SFX_SUCCESS);

    gfx_clear(GFX_WHITE);
    gfx_draw_top_bar("★ USB 传书同步完成", g_prof.stars, app_eye_mins_left());
    gfx_round_rect(140, 130, 520, 210, 18, 3, GFX_BLACK);
    gfx_draw_text_center(GFX_W / 2, 175, "✓ 优盘已安全收回至故事机！", FONT_24, GFX_BLACK);
    char sum[96];
    snprintf(sum, sizeof(sum), "当前书架共有 %d 个外置绘本/有声文件", total_now);
    gfx_draw_text_center(GFX_W / 2, 228, sum, FONT_24, GFX_BLACK);
    gfx_draw_text_center(GFX_W / 2, 282, "按下滚轮返回", FONT_16, GFX_BLACK);
    bsp_epd_smart_present(gfx_get_fb(), false);
    bsp_input_wait(2500);
}

/* =========================================================================
 * MODE 2 UI: 家庭路由器配网 + AI 设置 + 手机扫码传书 (APSTA Dual QR + Web Studio)
 * ========================================================================= */
void app_wifi_provision_run(bool focus_ai_tab)
{
    s_rx_count = 0;
    s_cfg_updates = 0;
    if (g_net_cfg.wifi_ssid[0]) {
        snprintf(s_last_rx_name, sizeof(s_last_rx_name), "已配Wi-Fi：%s · %s",
                 g_net_cfg.wifi_ssid, app_ai_provider_label(g_net_cfg.ai_provider));
    } else {
        strcpy(s_last_rx_name, "等待手机扫码配网或上传故事...");
    }
    hotspot_server_start(focus_ai_tab);

    int last_clients = -1;
    int last_rx = -1;
    int last_cfg = -1;
    bool first = true;

    while (1) {
        if (first || s_ap_clients != last_clients || s_rx_count != last_rx || s_cfg_updates != last_cfg) {
            last_clients = s_ap_clients;
            last_rx = s_rx_count;
            last_cfg = s_cfg_updates;

            uint32_t tot_kb = 0, free_kb = 0;
            bsp_storage_get_space_kb(&tot_kb, &free_kb);

            gfx_clear(GFX_WHITE);
            gfx_draw_top_bar(focus_ai_tab ? "★ 手机扫码配置家庭 Wi-Fi 与 AI 故事" : "★ 无线配网与扫码传书工坊",
                             g_prof.stars, app_eye_mins_left());

            /* Left Card: Step 1 Scan to Join Wi-Fi */
            gfx_round_rect(22, 52, 366, 322, 16, 2, GFX_BLACK);
            gfx_draw_text_center(205, 64, "第一步：手机扫码连本机热点", FONT_24, GFX_BLACK);
            gfx_draw_qrcode(118, 98, 5, "WIFI:T:nopass;S:" AP_SSID_NAME ";;");
            gfx_draw_text_center(205, 284, "热点名称：" AP_SSID_NAME " (免密码)", FONT_16, GFX_BLACK);
            char cli_s[64];
            snprintf(cli_s, sizeof(cli_s), "当前已连手机/电脑：%d 台", s_ap_clients);
            gfx_draw_text_center(205, 312, cli_s, FONT_16, GFX_BLACK);

            /* Right Card: Step 2 Scan or Open http://192.168.4.1 */
            gfx_round_rect(412, 52, 366, 322, 16, 2, GFX_BLACK);
            gfx_draw_text_center(595, 64, "第二步：自动弹窗或扫此码设置", FONT_24, GFX_BLACK);
            gfx_draw_qrcode(508, 98, 5, "http://" AP_IP_STR);
            gfx_draw_text_center(595, 284, "管理页：http://" AP_IP_STR, FONT_16, GFX_BLACK);
            gfx_draw_text_center(595, 312, "配家庭Wi-Fi · 绑小智/大模型 · 传有声书", FONT_16, GFX_BLACK);

            /* Live Provisioning & Transfer Status Bar */
            gfx_fill_round_rect(22, 384, GFX_W - 44, 48, 10, GFX_GRAY_LIGHT);
            gfx_round_rect(22, 384, GFX_W - 44, 48, 10, 2, GFX_BLACK);
            char st_bar[180];
            snprintf(st_bar, sizeof(st_bar), "%s  |  已传 %d 篇 (剩余 %.1fMB)",
                     s_last_rx_name, s_rx_count, (float)free_kb / 1024.0f);
            gfx_draw_text_center(GFX_W / 2, 398, st_bar, FONT_16, GFX_BLACK);

            gfx_draw_bottom_hints("在手机网页完成保存", "配好按确认关闭热点", "关闭热点返回");
            bsp_epd_smart_present(gfx_get_fb(), first);
            first = false;
        }

        bsp_key_t k = bsp_input_wait(700);
        if (k == KEY_OK || k == KEY_BACK) {
            break;
        }
    }

    hotspot_server_stop();
    bsp_audio_sfx(SFX_CONFIRM);
}

/* =========================================================================
 * Entry Hub: 传书与配网工坊 (Choose USB Flash Drive vs Wi-Fi Provisioning & Studio)
 * ========================================================================= */
void app_transfer_hub_run(void)
{
    int sel = 0;
    bool first = true;

    while (1) {
        uint32_t tot_kb = 0, free_kb = 0;
        bsp_storage_get_space_kb(&tot_kb, &free_kb);
        int cnt = bsp_storage_count_stories();

        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 传书与配网工坊 (USB优盘 · 家庭Wi-Fi · 扫码传书)", g_prof.stars, app_eye_mins_left());

        const char *titles[2] = { "USB 电脑优盘传故事", "无线配网·AI与扫码传书" };
        const char *subs[2] = {
            "插上 Type-C 线秒变电脑 U 盘！免装驱动，直接把电脑里的 .txt 绘本和 .wav 音频拖进 stories 文件夹即可。",
            "手机扫码一站搞定：配置家庭 2.4GHz 路由器、绑定小智 AI 或 DeepSeek/千问大模型，并支持在线传书与录音！"
        };
        gfx_art_id_t arts[2] = { ART_LAPTOP_USB, ART_WIFI_PHONE };

        for (int i = 0; i < 2; i++) {
            int x = 36 + i * 374;
            bool focus = (i == sel);
            gfx_round_rect(x, 60, 354, 318, 18, focus ? 5 : 2, GFX_BLACK);
            if (focus) {
                gfx_fill_round_rect(x + 12, 72, 330, 48, 12, GFX_BLACK);
                gfx_draw_text_center(x + 177, 84, titles[i], FONT_24, GFX_WHITE);
            } else {
                gfx_draw_text_center(x + 177, 84, titles[i], FONT_24, GFX_BLACK);
            }
            gfx_draw_art(x + 177, 192, 135, arts[i], GFX_BLACK);
            gfx_draw_paragraph(x + 22, 260, 310, 108, 6, subs[i], FONT_16, GFX_BLACK, NULL);
        }

        char foot[180];
        snprintf(foot, sizeof(foot), "家庭Wi-Fi：%s  ·  存储：%s  ·  外置/AI绘本 %d 个 (余 %.1fMB)",
                 app_wifi_has_credentials() ? g_net_cfg.wifi_ssid : "待配置",
                 bsp_storage_get_medium_label(), cnt, (float)free_kb / 1024.0f);
        gfx_draw_text_center(GFX_W / 2, 396, foot, FONT_16, GFX_BLACK);

        gfx_draw_bottom_hints("滚轮选择模式", "按下进入", "返回上一页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV || k == KEY_NEXT) { sel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            if (sel == 0) run_usb_drive_transfer();
            else app_wifi_provision_run(false);
            first = true;
        }
    }
}
