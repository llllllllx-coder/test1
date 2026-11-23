#ifndef BSP_KEY_H
#define BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BSP_KEY_DEBOUNCE_MS 20U

void BSP_KEY_OnExti(uint16_t gpio_pin);
uint8_t BSP_KEY_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_H */
