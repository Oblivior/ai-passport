#include "pet_ble.h"
#include "pet_protocol.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <string.h>

/* AES-256-GCM, fresh connection challenge as AAD, independent directions,
 * 96-bit random nonce, monotonic per-connection request sequence. No custom
 * cipher, no plaintext fallback, no OS Bluetooth bonding dependency. */
#define FRAME_MAX (12 + 4 + PET_LINE_MAX + 16)
typedef struct { uint32_t generation; uint16_t length; uint8_t data[FRAME_MAX]; } request_t;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_requests;
static pet_ble_status_t s_status;
static uint8_t s_key[32], s_challenge[16], s_reply[FRAME_MAX];
static uint16_t s_reply_len, s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint32_t s_generation, s_sequence, s_reply_generation;
static unsigned s_failures;
static int64_t s_activity;
static bool s_started, s_pending;
static uint8_t s_addr_type;
static char s_name[16];
static pet_meet_t s_meet;
static int s_meet_error;
static uint32_t s_meet_revision, s_meet_advertised, s_meet_refresh_at;

/* 2b251000/1/2/3-8db0-4bdb-8b45-947717e4e6fa, little-endian NimBLE UUIDs. */
#define UUID(n) BLE_UUID128_INIT(0xfa,0xe6,0xe4,0x17,0x77,0x94,0x45,0x8b,0xdb,0x4b,0xb0,0x8d,n,0x10,0x25,0x2b)
static const ble_uuid128_t SERVICE = UUID(0), INFO = UUID(1), REQUEST = UUID(2), RESPONSE = UUID(3);
static int gap_event(struct ble_gap_event *event, void *arg);

static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

static int access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr;
    unsigned kind = (unsigned)(uintptr_t)arg;
    int rc = BLE_ATT_ERR_UNLIKELY;
    lock();
    if (conn != s_conn) { unlock(); return rc; }
    if (kind == 1 && ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t info[25] = {3};
        memcpy(info + 1, s_status.id, 8);
        memcpy(info + 9, s_challenge, 16);
        rc = os_mbuf_append(ctxt->om, info, sizeof(info)) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    } else if (kind == 3 && ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        rc = os_mbuf_append(ctxt->om, s_reply, s_reply_len) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    } else if (kind == 2 && ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        request_t request = {.generation = s_generation, .length = OS_MBUF_PKTLEN(ctxt->om)};
        if (s_pending) rc = BLE_ATT_ERR_INSUFFICIENT_RES;
        else if (request.length < 33 || request.length > FRAME_MAX) rc = BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        else if (os_mbuf_copydata(ctxt->om, 0, request.length, request.data) == 0 &&
                 xQueueSend(s_requests, &request, 0) == pdTRUE) {
            s_pending = true;
            s_reply_len = 0;
            rc = 0;
        }
    }
    unlock();
    return rc;
}

static const struct ble_gatt_svc_def SERVICES[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &SERVICE.u,
     .characteristics = (struct ble_gatt_chr_def[]) {
         {.uuid=&INFO.u, .access_cb=access, .arg=(void *)1, .flags=BLE_GATT_CHR_F_READ},
         {.uuid=&REQUEST.u, .access_cb=access, .arg=(void *)2, .flags=BLE_GATT_CHR_F_WRITE},
         {.uuid=&RESPONSE.u, .access_cb=access, .arg=(void *)3, .flags=BLE_GATT_CHR_F_READ},
         {0}}},
    {0}
};

static int advertise(void)
{
    uint8_t greeting[PET_MEET_WIRE_SIZE];
    lock();
    bool meeting = pet_meet_encode(&s_meet, greeting);
    uint32_t revision = s_meet_revision;
    unlock();
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&SERVICE;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    struct ble_hs_adv_fields scan = {0};
    scan.name = (const uint8_t *)s_name;
    scan.name_len = strlen(s_name);
    scan.name_is_complete = 1;
    if (meeting) { scan.mfg_data = greeting; scan.mfg_data_len = sizeof(greeting); }
    if (!rc) rc = ble_gap_adv_rsp_set_fields(&scan);
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = 800; /* 500-625 ms; no continuously fast advertising. */
    params.itvl_max = 1000;
    if (!rc) rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    lock(); s_status.error = rc; if (!rc) s_meet_advertised = revision; unlock();
    return rc;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields = {0};
        if (!ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) && fields.mfg_data) {
            lock();
            uint8_t before[PET_MEET_WIRE_SIZE] = {0}, after[PET_MEET_WIRE_SIZE] = {0};
            pet_meet_encode(&s_meet, before);
            pet_meet_receive(&s_meet, fields.mfg_data, fields.mfg_data_len, event->disc.rssi,
                (uint32_t)(esp_timer_get_time() / 1000));
            pet_meet_encode(&s_meet, after);
            if (memcmp(before, after, sizeof(before))) s_meet_revision++;
            unlock();
        }
    } else if (event->type == BLE_GAP_EVENT_CONNECT && event->connect.status == 0) {
        lock();
        s_conn = event->connect.conn_handle;
        s_generation++;
        s_sequence = s_failures = s_reply_len = 0;
        s_pending = false;
        esp_fill_random(s_challenge, sizeof(s_challenge));
        s_activity = esp_timer_get_time();
        s_status.connected = true;
        s_status.authenticated = false;
        if (s_meet.active) { s_meet.active = false; s_meet_error = BLE_HS_EBUSY; s_meet_revision++; }
        unlock();
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        lock();
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        s_generation++;
        s_reply_len = 0;
        s_pending = false;
        s_status.connected = s_status.authenticated = false;
        unlock();
        advertise();
    } else if (event->type == BLE_GAP_EVENT_ADV_COMPLETE ||
               (event->type == BLE_GAP_EVENT_CONNECT && event->connect.status)) advertise();
    return 0;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &s_addr_type);
    lock(); s_status.ready = rc == 0; s_status.error = rc; unlock();
    if (!rc) advertise();
}
static void on_reset(int reason)
{
    lock(); s_status.ready = s_status.connected = s_status.authenticated = false;
    s_status.error = reason; s_generation++; s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_meet.active = false; s_meet_error = reason; s_meet_revision++; unlock();
}
static void host_task(void *arg) { (void)arg; nimble_port_run(); nimble_port_freertos_deinit(); }

static bool start(void)
{
    if (s_started) return true;
    int rc = nimble_port_init();
    if (rc) { lock(); s_status.error = rc; unlock(); return false; }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    snprintf(s_name, sizeof(s_name), "AIPet-%s", s_status.id);
    rc = ble_svc_gap_device_name_set(s_name);
    if (!rc) rc = ble_gatts_count_cfg(SERVICES);
    if (!rc) rc = ble_gatts_add_svcs(SERVICES);
    if (rc) { nimble_port_deinit(); lock(); s_status.error = rc; unlock(); return false; }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_att_set_preferred_mtu(256);
    s_started = true;
    nimble_port_freertos_init(host_task);
    return true;
}

static void identify(void)
{
    uint8_t digest[32];
    mbedtls_sha256(s_key, sizeof(s_key), digest, 0);
    snprintf(s_status.id, sizeof(s_status.id), "%02x%02x%02x%02x", digest[0],digest[1],digest[2],digest[3]);
    s_status.paired = true;
}

void pet_ble_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_requests = xQueueCreate(1, sizeof(request_t));
    if (!s_lock || !s_requests) { s_status.error = ESP_ERR_NO_MEM; return; }
    nvs_handle_t h;
    esp_err_t err = nvs_open("ai_pet_link", NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return;
    if (err != ESP_OK) { s_status.error = err; return; }
    size_t size = sizeof(s_key);
    err = nvs_get_blob(h, "key", s_key, &size);
    nvs_close(h);
    if (err != ESP_OK || size != sizeof(s_key)) { s_status.error = ESP_FAIL; return; }
    lock(); identify(); unlock();
    start();
}

bool pet_ble_pair(const char *hex)
{
    if (!s_lock || !s_requests || strlen(hex) != 64) return false;
    uint8_t key[32];
    for (unsigned i = 0; i < 32; i++) {
        unsigned value = 0;
        for (unsigned j = 0; j < 2; j++) {
            char c = hex[i * 2 + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            value = value * 16 + (c <= '9' ? c - '0' : c - 'a' + 10);
        }
        key[i] = value;
    }
    if (s_status.paired) return memcmp(key, s_key, sizeof(key)) == 0 && start();
    if (s_status.error) return false; /* Do not overwrite an unreadable pairing. */
    nvs_handle_t h;
    if (nvs_open("ai_pet_link", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, "key", key, sizeof(key));
    if (!err) err = nvs_commit(h);
    nvs_close(h);
    if (err) return false;
    lock(); memcpy(s_key, key, sizeof(key)); identify(); unlock();
    return start();
}

void pet_ble_status(pet_ble_status_t *out)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); return; }
    lock(); *out = s_status; unlock();
}

void pet_ble_meet_start(unsigned species, unsigned stage, unsigned branch)
{
    if (!s_lock) return;
    uint32_t nonce;
    do { esp_fill_random(&nonce, sizeof(nonce)); } while (!nonce);
    lock();
    s_meet = (pet_meet_t){0};
    s_meet_error = !s_status.ready ? BLE_HS_ENOTSYNCED : s_status.connected ? BLE_HS_EBUSY : 0;
    if (!s_meet_error && !pet_meet_start(&s_meet, (uint32_t)(esp_timer_get_time() / 1000), nonce, species, stage, branch))
        s_meet_error = BLE_HS_EINVAL;
    s_meet_revision++;
    unlock();
}
void pet_ble_meet_confirm(void)
{
    if (!s_lock) return;
    lock();
    uint8_t before[PET_MEET_WIRE_SIZE] = {0}, after[PET_MEET_WIRE_SIZE] = {0};
    pet_meet_encode(&s_meet, before);
    pet_meet_confirm(&s_meet, (uint32_t)(esp_timer_get_time() / 1000));
    pet_meet_encode(&s_meet, after);
    if (memcmp(before, after, sizeof(before))) s_meet_revision++;
    unlock();
}
void pet_ble_meet_cancel(void)
{
    if (!s_lock) return;
    lock();
    if (s_meet.active) { s_meet.active = false; s_meet_revision++; }
    unlock();
}
void pet_ble_meet_status(pet_meet_t *out, int *error)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); *error = -1; return; }
    lock(); *out = s_meet; *error = s_meet_error; unlock();
}
static void meet_refresh(void)
{
    if (!s_started) return;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    lock();
    uint8_t before[PET_MEET_WIRE_SIZE] = {0}, after[PET_MEET_WIRE_SIZE] = {0};
    pet_meet_encode(&s_meet, before); pet_meet_tick(&s_meet, now); pet_meet_encode(&s_meet, after);
    if (memcmp(before, after, sizeof(before))) s_meet_revision++;
    bool active = s_meet.active, ready = s_status.ready, connected = s_status.connected;
    bool dirty = s_meet_advertised != s_meet_revision;
    unlock();
    if (!ready) return;
    if (active && !ble_gap_disc_active()) {
        struct ble_gap_disc_params params = {0};
        params.itvl = 160; params.window = 48; /* 100 ms interval / 30 ms active scan window. */
        params.passive = 0; params.filter_duplicates = 0;
        int rc = ble_gap_disc(s_addr_type, PET_MEET_WINDOW_MS, &params, gap_event, NULL);
        if (rc) {
            lock(); s_meet.active = false; s_meet_error = rc; s_meet_revision++; unlock();
            active = false; dirty = true;
        }
    } else if (!active && ble_gap_disc_active()) ble_gap_disc_cancel();
    if (dirty && !connected && now - s_meet_refresh_at >= 200U) {
        s_meet_refresh_at = now;
        /* Worker priority 3 is below NimBLE's host task. Never stop/restart
         * advertising synchronously from its GAP callback or an LVGL button. */
        int rc = ble_gap_adv_stop();
        if (rc == 0 || rc == BLE_HS_EALREADY) rc = advertise();
        if (rc) {
            lock(); s_meet_error = rc; s_meet.active = false; unlock();
        }
    }
}

bool pet_ble_receive(char *line, size_t capacity)
{
    if (!s_lock || !s_requests) return false;
    meet_refresh();
    uint16_t terminate = BLE_HS_CONN_HANDLE_NONE;
    lock();
    if (s_conn != BLE_HS_CONN_HANDLE_NONE && esp_timer_get_time() - s_activity > 15000000) terminate = s_conn;
    unlock();
    if (terminate != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(terminate, BLE_ERR_REM_USER_CONN_TERM);
    request_t request;
    if (xQueueReceive(s_requests, &request, 0) != pdTRUE) return false;
    lock();
    if (request.generation != s_generation || s_conn == BLE_HS_CONN_HANDLE_NONE) { unlock(); return false; }
    uint8_t plain[4 + PET_LINE_MAX], aad[22];
    memcpy(aad, "PET3-C", 6); memcpy(aad + 6, s_challenge, 16);
    size_t size = request.length - 28;
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_key, 256);
    if (!rc) rc = mbedtls_gcm_auth_decrypt(&gcm, size, request.data, 12, aad, sizeof(aad),
        request.data + 12 + size, 16, request.data + 12, plain);
    mbedtls_gcm_free(&gcm);
    uint32_t sequence = rc ? 0 : (uint32_t)plain[0]<<24 | (uint32_t)plain[1]<<16 | (uint32_t)plain[2]<<8 | plain[3];
    bool valid = !rc && sequence > s_sequence && size >= 5 && size - 4 < capacity;
    for (unsigned i = 4; valid && i < size; i++) valid = plain[i] >= 32 && plain[i] <= 126;
    if (valid) {
        memcpy(line, plain + 4, size - 4); line[size - 4] = 0;
        valid = !strcmp(line, "PET2 STATUS") || !strcmp(line, "PET2 ROUTE") ||
            !strcmp(line, "PET2 MONTHS") || !strncmp(line, "PET2 SETTLE ", 12) || !strncmp(line, "PET2 SYNC ", 10);
    }
    if (!valid) {
        s_pending = false;
        if (++s_failures >= 3) terminate = s_conn;
        unlock();
        if (terminate != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(terminate, BLE_ERR_REM_USER_CONN_TERM);
        return false;
    }
    s_sequence = sequence;
    s_reply_generation = s_generation;
    s_activity = esp_timer_get_time();
    s_status.authenticated = true;
    unlock();
    return true;
}

void pet_ble_reply(const char *line)
{
    size_t size = strlen(line);
    if (size >= PET_LINE_MAX) return;
    lock();
    if (s_reply_generation != s_generation || !s_pending) { unlock(); return; }
    uint8_t plain[4 + PET_LINE_MAX], aad[22];
    plain[0]=s_sequence>>24; plain[1]=s_sequence>>16; plain[2]=s_sequence>>8; plain[3]=s_sequence;
    memcpy(plain + 4, line, size); size += 4;
    memcpy(aad, "PET3-S", 6); memcpy(aad + 6, s_challenge, 16);
    esp_fill_random(s_reply, 12);
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_key, 256);
    if (!rc) rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, size, s_reply, 12, aad, sizeof(aad),
        plain, s_reply + 12, 16, s_reply + 12 + size);
    mbedtls_gcm_free(&gcm);
    s_reply_len = rc ? 0 : size + 28;
    s_pending = false;
    unlock();
}
