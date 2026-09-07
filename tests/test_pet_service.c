/* Execute the production service transaction/queue boundary with platform mocks.
 * This checks C and behavior, not the ESP-IDF ABI or actual RTOS scheduling. */
#include "../main/pet_service.c"
#include <assert.h>

static char response[PET_LINE_MAX];
static bool fail_save;
static unsigned saves;
static unsigned bond_saves;
static bool fail_bond_save;
static action_t queued;
static int marker;
static pet_meet_t radio_meet;
static unsigned radio_nonce;
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
void pet_ble_meet_start(unsigned id, unsigned stage, unsigned branch)
{ assert(pet_meet_start(&radio_meet, 1000, ++radio_nonce, id, stage, branch)); }
void pet_ble_meet_confirm(void) { pet_meet_confirm(&radio_meet, 1000); }
void pet_ble_meet_cancel(void) { radio_meet.active = false; }
void pet_ble_meet_status(pet_meet_t *out, int *error) { *out = radio_meet; *error = 0; }
bool pet_house_store_boot(pet_house_t *house, uint32_t *generation, bool *blocked)
{ pet_house_init(house, NULL); *generation = 0; *blocked = false; return true; }
bool pet_house_store_save(const pet_house_t *house, uint32_t *generation)
{ assert(pet_house_valid(house)); saves++; if (fail_save) return false; (*generation)++; return true; }
int pet_bond_store_load(pet_bond_t *bond, uint32_t *generation)
{ (void)bond; (void)generation; return 0; }
bool pet_bond_store_save(const pet_bond_t *bond, uint32_t *generation)
{ assert(pet_bond_valid(bond)); bond_saves++; if (fail_bond_save) return false; (*generation)++; return true; }

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
    s_storage_blocked = false;
    state.storage_ok = state.bond_storage_ok = true;
    assert(pet_house_choose(&state.house, PET_AGUMON));
    pet_bond_init(&state.bond); state.synced_at = 999; publish(&state);
    pet_house_t original = state.house;
    shown = state;
    assert(pet_service_train(&shown, 6, 3, 1));
    action_t training = queued;
    process_action(&state, &training);
    assert(state.training_ticket == 1 && state.training_result == PET_TRAIN_REWARDED && state.training_gain == 3);
    assert(state.bond.points[0] == 3 && bond_saves == 1 && saves == before);
    process_action(&state, &training);
    assert(bond_saves == 1 && state.bond.points[0] == 3); /* Duplicate result, no double reward. */
    assert(pet_service_train(&shown, 3, 3, 2)); training = queued;
    fail_bond_save = true; process_action(&state, &training);
    assert(state.training_result == PET_TRAIN_SAVE_ERROR && state.bond.points[0] == 3 && state.storage_ok);
    fail_bond_save = false; process_action(&state, &training);
    assert(state.training_result == PET_TRAIN_REWARDED && state.bond.points[0] == 5 && state.bond_storage_ok);
    assert(pet_service_train(&shown, 0, 1, 3)); process_action(&state, &queued);
    assert(state.bond.points[0] == 6 && state.bond.rewarded_today == 3);
    unsigned previous_bond_saves = bond_saves;
    assert(pet_service_train(&shown, 6, 3, 4)); process_action(&state, &queued);
    assert(state.training_result == PET_TRAIN_PRACTICE && bond_saves == previous_bond_saves);
    process_action(&state, &training); /* Older ticket cannot replace a newer receipt. */
    assert(state.training_ticket == 4);
    state.synced_at = 0;
    assert(pet_service_train(&shown, 6, 3, 5)); process_action(&state, &queued);
    assert(state.training_result == PET_TRAIN_OFFLINE);
    state.synced_at = 999;
    shown.house.active_id = PET_GABUMON;
    assert(pet_service_train(&shown, 6, 3, 6)); process_action(&state, &queued);
    assert(state.training_result == PET_TRAIN_STALE);
    shown = state; shown.house.life.date++;
    assert(pet_service_train(&shown, 6, 3, 7)); process_action(&state, &queued);
    assert(state.training_result == PET_TRAIN_STALE);
    shown = state; s_bond_blocked = true;
    assert(pet_service_train(&shown, 6, 3, 8)); process_action(&state, &queued);
    assert(state.training_result == PET_TRAIN_SAVE_ERROR && state.storage_ok && bond_saves == previous_bond_saves);
    assert(!memcmp(&state.house, &original, sizeof(original)));
    handle_line(&state, "PET2 BOND");
    assert(!strncmp(response, "PET2 REJECTED ", 14));
    assert(!pet_service_train(&shown, 6, 1, 9) && !pet_service_train(&shown, 0, 0, 0));
    original = state.house; shown = state;
    assert(pet_service_branch(&shown, 1)); process_action(&state, &queued);
    assert(!memcmp(&original, &state.house, sizeof(original))); /* Blocked bond cannot unlock. */
    s_bond_blocked = false; state.bond.points[0] = 30; state.bond_storage_ok = true;
    fail_save = true; process_action(&state, &queued);
    assert(!state.storage_ok && !state.house.branch_mask);
    fail_save = false; process_action(&state, &queued);
    assert(state.storage_ok && state.house.branch_mask == 1);
    before = saves; process_action(&state, &queued); assert(saves == before); /* Exact set, not toggle. */
    shown = state; shown.house.life.date = 20261001;
    assert(pet_service_branch(&shown, 0)); process_action(&state, &queued);
    assert(state.house.branch_mask == 1);
    shown = state; shown.house.active_id = PET_GABUMON;
    assert(!pet_service_branch(&shown, 1));
    shown = state; state.bond_storage_ok = false;
    assert(pet_service_branch(&shown, 0)); process_action(&state, &queued);
    assert(!state.house.branch_mask && !memcmp(&original, &state.house, sizeof(original)));
    handle_line(&state, "PET2 BRANCH"); assert(!strncmp(response, "PET2 REJECTED ", 14));
    shown = state; before = saves; previous_bond_saves = bond_saves;
    assert(pet_service_meet(&shown, PET_MEET_START));
    assert(pet_service_meet(NULL, PET_MEET_CANCEL));
    assert(!pet_service_meet(&shown, PET_MEET_CONFIRM));
    process_meeting(&state); assert(!state.meeting.active); /* Exit cancels pending start. */
    assert(pet_service_meet(&shown, PET_MEET_START)); process_meeting(&state);
    assert(state.meeting.active && state.meeting.species == PET_AGUMON);
    shown = state; shown.meeting.nonce++;
    radio_meet.peer = 42; radio_meet.sightings = 2; radio_meet.seen_at = 1000;
    assert(pet_service_meet(&shown, PET_MEET_CONFIRM)); process_meeting(&state);
    assert(!state.meeting.confirmed); /* Stale UI must not greet a new session. */
    shown = state; assert(pet_service_meet(&shown, PET_MEET_CONFIRM)); process_meeting(&state);
    assert(state.meeting.confirmed);
    assert(pet_house_choose(&state.house, PET_GABUMON)); process_meeting(&state);
    assert(!state.meeting.active && saves == before && bond_saves == previous_bond_saves);
    assert(!pet_service_meet(&shown, 0));
    handle_line(&state, "PET2 MEET"); assert(!strncmp(response, "PET2 REJECTED ", 14));
    puts("pet_service: PASS (production transactions, commit-before-ACK, replay, failure preservation, stale UI actions)");
}
