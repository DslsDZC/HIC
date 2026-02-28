#include "service.h"

/* 简化的注册函数 */
/* 空实现 */
hic_status_t service_init(void) { return HIC_SUCCESS; }
hic_status_t service_start(void) { return HIC_SUCCESS; }
hic_status_t service_stop(void) { return HIC_SUCCESS; }
hic_status_t service_cleanup(void) { return HIC_SUCCESS; }
hic_status_t service_get_info(char* buffer, u32 size) { 
    if (buffer && size > 0) buffer[0] = '\0';
    return HIC_SUCCESS;
}

const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

void service_register_self(void) {
    service_register("$service", &g_service_api);
}
