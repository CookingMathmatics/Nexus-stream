# NexusStream: Universal Async Dual-Ring Buffer Engine

`NexusStream`은 Windows 네이티브 환경에서 대용량 바이너리 및 가중치 파일을 초경량 메모리 풋프린트로 고속 스트리밍하기 위해 설계된 **순정 C++ 베어메탈 가속 프레임워크**입니다.

기존의 무거운 가상화 레이어나 OS의 자동 `mmap` 제어에 의존하지 않고, Win32 API와 하드웨어 최적화를 통해 시스템 자원을 극한으로 사수하며 안전한 실시간 데이터 파이프라인을 구축합니다.

## 🚀 Key Features

- **Zero-Dependency Architecture**: 외부 라이브러리, 파이썬 패키지 일절 배제. 오직 Pure C++ 표준 표준과 Win32 커널 API만으로 구동되어 공급망 취약점(Supply Chain Attack) 원천 차단.
- **Ultra-Low Memory Footprint**: 11GB 이상의 대용량 가중치 파일을 핸들링하면서도, 내부적으로 격리된 단 **576MB**의 고정 RAM 슬롯(이중 링버퍼)만 소모하여 호스트 자원 완벽 보호.
- **Asynchronous Overlapped I/O**: 메인 연산 코어(Compute Thread)와 백그라운드 디스크 스트리밍 버스(Load Thread)가 격리된 핑퐁 파이프라인 구조로 구동되어 디스크 무작위 탐색 패널티를 상쇄.
- **Hardware SIMD Vectorization**: 64바이트 엄격 정렬(64-Byte Aligned Memory) 레이아웃 가드를 기반으로, CPU 내부의 256비트 레지스터를 직접 타격하는 **AVX2 / FMA 인트린직 명령어** 완벽 지원.
- **Pluggable Architecture**: `IStreamStrategy` 인터페이스를 제공하여, 개발자가 자신만의 가상 주소 점프 알고리즘이나 독자적인 가속/복호화 커널을 안전하게 주입(Injection)할 수 있도록 추상화 완료.

## 🛠️ Hardware Requirements & Optimizations

- **OS**: Windows 10 / 11 (64-bit) Native
- **Compiler**: MSVC (Visual Studio 2022 이상, C++17/C++20 표준 권장)
- **Required MSVC Flags**: 
  - 고급 명령 집합 사용: `Advanced Vector Extensions 2 (/arch:AVX2)`
  - 최적화: `속도 극대화 (/O2)`
  - 고급 -> 문자 집합: `유니코드 문자 집합 사용`

## 💻 Quick Start

`NexusStream`은 헤더 온리(Header-only) 형태로도 쉽게 확장할 수 있는 유연한 구조를 가집니다. 프로젝트에 포함된 `main.cpp`를 통해 이중 링버퍼 파이프라인이 크래시 없이 대용량 파일을 고속으로 순회하며 스트리밍하는 예제를 즉시 구동해볼 수 있습니다.

```cpp
// 1. 자신만의 스트리밍 및 연산 전략 정의
class CustomStrategy : public IStreamStrategy {
public: // <-- 외부 접근 권한 완벽 락온!
    size_t GetNextLayerIndex(size_t step, size_t total) override { return step % total; }
    void ProcessLayerData(void* data, float* states, size_t stride) override { /* AVX2 GEMM Load/Store */ }
};

// 2. 파이프라인 엔진 격발
auto strategy = std::make_shared<CustomStrategy>();
NexusStreamEngine engine(L"model_payload.gguf", 40, strategy);
engine.ExecuteStreamPipeline(12);
