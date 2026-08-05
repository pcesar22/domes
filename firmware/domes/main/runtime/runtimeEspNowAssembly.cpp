#include "infra/taskManager.hpp"
#include "infra/taskTopology.hpp"
#include "interfaces/iPlatformIdentity.hpp"
#include "interfaces/iRandomSource.hpp"
#include "runtimeAssembly.hpp"
#include "services/espNowService.hpp"

namespace domes::runtime {

esp_err_t RuntimeAssembly::prepareEspNowService(EspNowTransport& transport,
                                                IPlatformIdentity& identity,
                                                IRandomSource& random) {
    if (!handles_.features || preparedEspNow_ || handles_.espNow) {
        return ESP_ERR_INVALID_STATE;
    }

    static EspNowService service(transport, *handles_.features, identity, random);
    const esp_err_t err = service.init();
    if (err != ESP_OK) {
        return err;
    }
    if (handles_.game) {
        service.setGameEngine(handles_.game);
    }
    if (handles_.led) {
        service.setLedService(handles_.led);
    }
    if (handles_.modes) {
        service.setModeManager(handles_.modes);
    }
    if (handles_.injectableTouch) {
        service.setInjectableTouchDriver(handles_.injectableTouch);
    }
    preparedEspNow_ = &service;
    return ESP_OK;
}

esp_err_t RuntimeAssembly::startEspNowService(infra::TaskManager& taskManager) {
    if (!preparedEspNow_ || handles_.espNow || espNowStartAttempted_) {
        return ESP_ERR_INVALID_STATE;
    }
    espNowStartAttempted_ = true;
    const esp_err_t err = taskManager.createTask(infra::task::kEspNowService, *preparedEspNow_);
    if (err == ESP_OK) {
        handles_.espNow = preparedEspNow_;
    }
    return err;
}

}  // namespace domes::runtime
