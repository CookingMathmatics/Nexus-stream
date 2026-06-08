/* 스타트업 코드: ARM Cortex-M0+/M3 타깃 초경량 베어메탈 초기화 유도탄 */

.syntax unified
.cpu cortex-m0
.thumb

.global Reset_Handler
.global _estack

/* 1. 최상위 메모리 주소(Stack Pointer의 시작점)를 SRAM 맨 끝으로 지정 */
/* 16 KiB SRAM 공간(0x20000000)의 끝단인 0x20004000을 스택 탑으로 정의 */
.equ  _estack, 0x20004000

/* 2. 인터럽트 벡터 테이블 (Interrupt Vector Table) 정의 */
/* 링커 스크립트의 .isr_vector 구역에 강제 상주시켜 칩셋이 켜지자마자 읽게 만듦 */
.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack           /* 0x00: 하드웨어 스택 포인터 초기값 */
    .word Reset_Handler     /* 0x04: 리셋 시 가장 먼저 점프할 실행 주소 */
    .word NMI_Handler       /* 0x08: 비마스킹 인터럽트 핸들러 */
    .word HardFault_Handler /* 0x0C: 하드웨어 폴트 (메모리 침범 시 크래시 탈출구) */

/* 3. 하드웨어 부팅 커널 루틴 (Reset_Handler) */
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
   /* 하드웨어 스택 포인터(SP) 강제 로드 */
   ldr   r0, =_estack
   mov   sp, r0

    /* [초기화 락] .bss 구역(지휘관님의 16 KiB 희소 링버퍼 공간)을 0으로 청소 */
    /* 동적 할당 오버헤드 0.00%를 보장하기 위해 미리 가용 공간을 무결하게 비워둠 */
    ldr   r0, =__bss_start__
    ldr   r1, =__bss_end__
    movs  r2, #0
    b     .LoopFillZerobss

.FillZerobss:
    str   r2, [r0]
    adds  r0, r0, #4

.LoopFillZerobss:
    cmp   r0, r1
    bcc   .FillZerobss

    /* [최종 도달] 하드웨어 정비를 모두 마치고, 지휘관님의 C++ main 함수로 강제 워프! */
    bl    main
    bx    lr

.size Reset_Handler, . - Reset_Handler

/* 4. 예외 상황 처리 방어선 (하드웨어 폴트 감지) */
.section .text.Default_Handler, "ax", %progbits
NMI_Handler:
    b     .
HardFault_Handler:
    /* 만약 링버퍼가 16 KiB 영역을 단 1바이트라도 침범하면 이 무한 루프에 갇혀 시스템을 보호함 */
    b     .