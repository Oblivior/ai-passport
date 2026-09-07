/* Execute the production service transaction/queue boundary with platform mocks.
 * This checks C and behavior, not the ESP-IDF ABI or actual RTOS scheduling. */
#include "../main/pet_service.c"
#include <assert.h>

static char response[PET_LINE_MAX];
static bool fail_save;
static unsigned saves;
static action_t queued;
static int marker;
QueueHandle_t xQueueCreate(unsigned count, size_t size) { assert(count == 1 && size == sizeof(action_t)); return &marker; }
int xQueueSend(QueueHandle_t q, const void *item, uint32_t wait) { (void)q; (void)wait; queued = *(const action_t *)item; return pdTRUE; }
int xQueueReceive(QueueHandle_t q, void *item, uint32_t wait) { (void)q; (void)item; (void)wait; return 0; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &marker; }
int xSemaphoreTake(SemaphoreHandle_t lock, uint32_t wait) { (void)lock; (void)wait; return pdTRUE; }
int xSemaphoreGive(SemaphoreHandle_t lock) { (void)lock; return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t lock) { (void)lock; }
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *task)
{ (void)entry; (void)name; (void)arg; (void)priority; (void)task; assert(stack == 8192); return pdPASS; }
void vTaskDelay(uint32_t ticks) { (void)ticks; }
int64_t esp_timer_get_time(void) { return 1000000; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 48000; }
size_t heap_caps_get_largest_free_block(unsigned caps) { return heap_caps_get_free_size(caps); }
esp_err_t demo_radio_nvs_prepare(void) { return ESP_OK; }
bool usb_serial_jtag_is_driver_installed(void) { return true; }
esp_err_t usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *config) { (void)config; return ESP_OK; }
void usb_serial_jtag_vfs_use_driver(void) {}
void pet_ble_init(void) {}
bool pet_ble_pair(const char *key) { (void)key; return false; }
void pet_ble_status(pet_ble_status_t *out) { memset(out, 0, sizeof(*out)); }
bool pet_ble_receive(char *line, size_t capacity) { (void)line; (void)capacity; return false; }
void pet_ble_reply(const char *line) { snprintf(response, sizeof(response), "%s", line); }
bool pet_house_store_boot(pet_house_t *house, uint32_t *generation, bool *blocked)
{ pet_house_init(house, NULL); *generation = 0; *blocked = false; return true; }
bool pet_house_store_save(const pet_house_t *house, uint32_t *generation)
{ assert(pet_house_valid(house)); saves++; if (fail_save) return false; (*generation)++; return true; }

int main(void)
{
    assert(pet_service_start());
    pet_snapshot_t state = {.ready = true, .storage_ok = true, .revision = 1};
    pet_house_init(&state.house, NULL);
    assert(pet_house_choose(&state.house, PET_AGUMON));
    publish(&state);
    s_wireless_request = true;
    const char *sync = "PET2 SYNC 20260907 300 1000 0000002000000000000000000000000";
    handle_line(&state, sync);
    assert(!strncmp(response, "PET2 ACK ", 9) && saves == 1 && pet_life_pending(&state.house.life) == 2);
    handle_line(&state, sync);
    assert(!strncmp(response, "PET2 ACK ", 9) && saves == 1); /* Idempotent, no flash churn. */
    pet_snapshot_t shown = state;
    assert(pet_service_eat(&shown));
    assert(queued.expected_id == PET_AGUMON && queued.month == 202609 && !queued.choose_id);
    pet_house_t next = state.house;
    assert(pet_house_action(&next, queued.expected_id, queued.month, queued.choose_id));
    fail_save = true;
    assert(!commit(&state, &next) && !state.storage_ok && pet_house_meals(&state.house, PET_AGUMON) == 0);
    handle_line(&state, sync);
    assert(!strncmp(response, "PET2 STORAGE_ERROR ", 19));
    fail_save = false;
    assert(commit(&state, &next) && state.storage_ok && pet_house_meals(&state.house, PET_AGUMON) == 1);
    assert(pet_house_choose(&state.house, PET_GABUMON)); publish(&state);
    /* New worker state must not retarget a button from an older displayed pet. */
    assert(pet_service_eat(&shown) && queued.expected_id == PET_AGUMON);
    next = state.house;
    assert(!pet_house_action(&next, queued.expected_id, queued.month, queued.choose_id));
    assert(pet_service_choose(&shown, PET_PATAMON) && queued.expected_id == PET_AGUMON);
    assert(!pet_house_action(&next, queued.expected_id, queued.month, queued.choose_id));
    handle_line(&state, "PET2 STATUS");
    assert(strstr(response, "pending=1 meals=0 days=0 stage=0"));
    handle_line(&state, "PET2 HOUSE");
    assert(!strncmp(response, "PET2 REJECTED ", 14)); /* Still USB-only. */
    s_storage_blocked = true;
    unsigned before = saves;
    handle_line(&state, sync);
    assert(!strncmp(response, "PET2 STORAGE_ERROR ", 19) && saves == before);
    puts("pet_service: PASS (production transactions, commit-before-ACK, replay, failure preservation, stale UI actions)");
}
