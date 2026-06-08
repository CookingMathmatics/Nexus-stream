/*
 * linker.ld - Pure Bare-Metal Confinement Configuration
 * [RED TEAM MITIGATION] 캐시라인 파괴 및 정렬 붕괴 방어용 64바이트 고정 격벽
 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 16K   /* 가속 펌웨어 보안 영역 */
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 16K   /* 16 KiB 정적 스파스 격리 버퍼 */
}

SECTIONS
{
    /* 벡터 테이블 및 기계어 명령어 섹션 */
    .text : 
    {
        . = ALIGN(64); /* 하드웨어 버스 프리페치 최적화 보장 정렬 */
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        . = ALIGN(64);
    } > FLASH

    /* 초기화되지 않은 정적 스파스 버퍼 데이터 가두기 구역 */
    .bss : 
    {
        . = ALIGN(64); /* 공격자가 메모리 경계를 침범해도 버스 지연이 발생하지 않도록 격리 */
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(64);
        _ebss = .;
    } > RAM

    _estack = ORIGIN(RAM) + LENGTH(RAM); /* 최상단 스택 주소 정렬 바인딩 */
}
