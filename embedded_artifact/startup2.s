/*
 * startup.s - Cortex-M0 Core Vector Table & System Recovery
 * [RED TEAM MITIGATION] 자가 복구 하드웨어 리셋(VECTRESET) 프로토콜 주입
 */

.syntax unified
.cpu cortex-m0
.thumb

.global Vector_Table
.global HardFault_Handler

.section .vectors, "a"
.align 2

Vector_Table:
    .word   _estack             /* 0: Top of Stack */
    .word   Reset_Handler       /* 1: Reset Handler */
    .word   NMI_Handler         /* 2: NMI Handler */
    .word   HardFault_Handler   /* 3: Hard Fault Handler (지옥의 방어선) */

.section .text

.thumb_func
Reset_Handler:
    /* 하드웨어 초기화 및 메인 커널 점프 시퀀스 */
    bl      main
    b       .

.thumb_func
HardFault_Handler:
    /* * [BLUE TEAM ANTI-GLITCH] 
     * 공격자의 전압 글리치로 인한 Hard-Lockup(좀비화) 무력화 조치.
     * ARM Cortex-M Application Interrupt and Reset Control Register(AIRCR)를 직접 타격.
     */
    ldr r0, =0xE000ED0C  /* AIRCR 레지스터 주소 로드 */
    ldr r1, =0x05FA0004  /* 시스템 자가 재부팅 키 (VECTRESET 강제 집행) */
    str r1, [r0]         /* 레지스터에 비트 밀어 넣기 */

_infinite_defense:
    b       _infinite_defense   /* 리셋 완료 전까지 메모리 오염 차단 격벽 */
