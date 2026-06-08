/* 링커 스크립트: 16 KiB SRAM 및 64 KiB Flash 초경량 칩셋 타깃 */

/* 1. 하드웨어 진입점(Entry Point) 선언 */
ENTRY(Reset_Handler)

/* 2. 초소형 칩셋의 물리적 영토(MEMORY) 한계 확정 */
MEMORY
{
    /* Flash: 코드가 저장되는 공간 (읽기/실행 전용, 64 KiB 제한) */
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    
    /* SRAM: 지휘관님의 희소 링버퍼가 상주할 공간 (읽기/쓰기, 16 KiB 극단 제한) */
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
}

/* 3. 각 코드와 데이터가 들어갈 세부 구역(SECTIONS) 정의 */
SECTIONS
{
    /* [구역 A] 인터럽트 벡터 테이블 및 실행 코드는 FLASH에 박아둠 (XIP 전략) */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector)) /* 칩셋이 켜지자마자 읽을 주소록 */
        *(.text)             /* 지휘관님의 가속 커널 함수들 */
        *(.text.*)
        *(.rodata)           /* 위상 기하학용 정수 상수들 */
        *(.rodata.*)
        . = ALIGN(4);
    } > FLASH

    /* [구역 B] 초기화가 필요한 변수 영역 (형님의 규칙에 따라 초기값 전역변수를 금지하여 비어있음) */
    .data :
    {
        . = ALIGN(4);
        *(.data)
        *(.data.*)
        . = ALIGN(4);
    } > SRAM AT > FLASH

    /* [구역 C] 가장 중요한 고정 희소 링버퍼 영역 (0.00% 동적할당 오버헤드 사수) */
    .bss :
    {
        . = ALIGN(4);        /* 💥 기존 ALIGN(64) 방패를 버리고 초소형 칩셋 버스 정렬 최적화 라인 구축 */
        __bss_start__ = .;
        *(.bss)              /* 16 KiB SRAM 내부에 낭비 없이 고정 선언될 초경량 링버퍼 */
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > SRAM
}