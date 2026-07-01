/*
 * hw_diag.h — 硬件诊断接口（SRAM + SPI Flash 启动自检）
 *
 * 架构角色：一次性启动诊断。在 GUI 任务初始化阶段调用。
 * 单独拆出此模块，避免诊断代码长期驻留在 freertos.c。
 */

#ifndef __HW_DIAG_H__
#define __HW_DIAG_H__

void hw_diag(void);

#endif /* __HW_DIAG_H__ */
