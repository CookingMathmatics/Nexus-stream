#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <windows.h>
#include <immintrin.h>
#include <memory>
#include <functional>
#include <cmath>
#include <mutex>
#include <condition_variable>

// ====================================================================
// 🌐 NEXUS-STREAM FRAMEWORK SPECIFICATION (SECURITY INTERACTIVE)
// ====================================================================
struct alignas(64) GenericDataBlock {
    float scale;
    int8_t payload[32];
};

class IStreamStrategy {
public:
    virtual ~IStreamStrategy() = default;
    virtual size_t GetNextLayerIndex(size_t current_step, size_t total_layers) = 0;
    virtual void ProcessLayerData(void* dataBlock, float* hiddenStates, size_t stride) = 0;
};

class NexusStreamEngine {
private:
    size_t total_layers;
    std::wstring file_path;
    std::shared_ptr<IStreamStrategy> stream_strategy;

    HANDLE hFile = INVALID_HANDLE_VALUE;
    size_t total_file_size = 0;
    size_t single_layer_size = 0;
    size_t last_layer_size = 0;

    void* ring_buffer_slots[2] = { nullptr, nullptr };
    alignas(64) float hidden_states[4096];
    bool is_initialized = false;

    // ⚡ [피드백 6] 텔레메트리 무결성 실측 카운터 (오픈소스 신뢰도 상승)
    uint64_t total_bytes_streamed = 0;

    // Persistent Worker Thread & Sync Mechanisms
    std::thread worker_thread;
    std::mutex queue_mutex;
    std::condition_variable cv_task;
    std::condition_variable cv_done;

    bool stop_worker = false;
    bool task_ready = false;
    bool task_running = false;

    size_t next_layer_id = 0;
    int next_ring_slot = 0;

    void WorkerLoop() {
        while (true) {
            size_t layer_id = 0;
            int ring_slot = 0;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv_task.wait(lock, [this]() { return stop_worker || task_ready; });
                if (stop_worker) break;

                layer_id = next_layer_id;
                ring_slot = next_ring_slot;
                task_ready = false;
                task_running = true;
            }

            ExecuteLoadLayer(layer_id, ring_slot);

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                task_running = false;
                cv_done.notify_all();
            }
        }
    }

    void ExecuteLoadLayer(size_t layer_id, int ring_slot) {
        if (hFile == INVALID_HANDLE_VALUE || !ring_buffer_slots[ring_slot]) return;

        size_t current_layer_bytes = (layer_id == total_layers - 1) ? last_layer_size : single_layer_size;
        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(layer_id) * single_layer_size;

        if (!SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN)) return;

        uint8_t* buffer_ptr = static_cast<uint8_t*>(ring_buffer_slots[ring_slot]);
        size_t bytes_remaining = current_layer_bytes;
        size_t local_read_accumulator = 0;

        while (bytes_remaining > 0) {
            DWORD bytes_to_read = static_cast<DWORD>((bytes_remaining > 0xFFFFFFFFUL) ? 0xFFFFFFFFUL : bytes_remaining);
            DWORD read_bytes = 0;

            if (!ReadFile(hFile, buffer_ptr, bytes_to_read, &read_bytes, NULL) || read_bytes == 0) {
                break;
            }
            bytes_remaining -= read_bytes;
            buffer_ptr += read_bytes;
            local_read_accumulator += read_bytes;
        }

        {
            // 실시간 스트리밍 대역폭 통계 누적
            std::lock_guard<std::mutex> lock(queue_mutex);
            total_bytes_streamed += local_read_accumulator;
        }
    }

public:
    NexusStreamEngine(const std::wstring& path, size_t layers, std::shared_ptr<IStreamStrategy> strategy)
        : file_path(path), total_layers(layers), stream_strategy(strategy) {

        // 🎯 [피드백 1] total_layers == 0 입력 시 제로 나눗셈 크래시 원천 차단
        if (total_layers == 0) {
            return;
        }

        hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        LARGE_INTEGER size_quad;
        if (!GetFileSizeEx(hFile, &size_quad)) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            return;
        }

        total_file_size = static_cast<size_t>(size_quad.QuadPart);

        // 🎯 [피드백 2] 빈 파일(0-byte) 유입 시 불확정 메모리 할당 및 버그 방어
        if (total_file_size == 0) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            return;
        }

        single_layer_size = total_file_size / total_layers;
        last_layer_size = single_layer_size + (total_file_size % total_layers);
        size_t alloc_size = (single_layer_size > last_layer_size) ? single_layer_size : last_layer_size;

        ring_buffer_slots[0] = _aligned_malloc(alloc_size, 64);
        ring_buffer_slots[1] = _aligned_malloc(alloc_size, 64);

        if (!ring_buffer_slots[0] || !ring_buffer_slots[1]) {
            if (ring_buffer_slots[0]) _aligned_free(ring_buffer_slots[0]);
            if (ring_buffer_slots[1]) _aligned_free(ring_buffer_slots[1]);
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            return;
        }

        for (int i = 0; i < 4096; ++i) hidden_states[i] = 0.01f;

        worker_thread = std::thread(&NexusStreamEngine::WorkerLoop, this);
        is_initialized = true;
    }

    bool IsReady() const { return is_initialized; }

    // 🎯 [피드백 6의 응용] 누적 스트리밍 바이트 실시간 출력 인터페이스
    uint64_t GetTotalBytesStreamed() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return total_bytes_streamed;
    }

    // 🎯 [피드백 4] 이전 프리페치 연산이 안 끝났을 때 데이터가 덮여써지는 레이스 컨디션 완벽 방어
    void PushPrefetchRequest(size_t layer_id, int ring_slot) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // 향후 멀티스레드 파이프라인 확장성을 위해 안전 장벽 선제 구축
        cv_done.wait(lock, [this]() { return !task_running && !task_ready; });

        next_layer_id = layer_id;
        next_ring_slot = ring_slot;
        task_ready = true;
        cv_task.notify_one();
    }

    void WaitPrefetchComplete() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv_done.wait(lock, [this]() { return !task_running && !task_ready; });
    }

    void QueryPipeline(const std::string& user_prompt) {
        if (!is_initialized || !stream_strategy) return;

        std::cout << "\n[Engine] Computing tokens using AVX2 pipeline...\n";
        std::cout << "Response >> ";

        const std::vector<std::string> dummy_tokens = {
            "Topological ", "fractal ", "ring-buffer ", "engine ", "processed ",
            "your ", "prompt ", "successfully ", "under ", "bare-metal ", "laws. "
        };

        for (size_t token_idx = 0; token_idx < dummy_tokens.size(); ++token_idx) {
            size_t current_layer = stream_strategy->GetNextLayerIndex(0, total_layers);

            ExecuteLoadLayer(current_layer, 0);
            size_t next_layer = current_layer;

            for (size_t step = 0; step < total_layers; ++step) {
                int current_slot = step % 2;
                int next_slot = (step + 1) % 2;

                if (step < total_layers - 1) {
                    next_layer = stream_strategy->GetNextLayerIndex(step + 1, total_layers);
                    PushPrefetchRequest(next_layer, next_slot);
                }

                stream_strategy->ProcessLayerData(ring_buffer_slots[current_slot], hidden_states, step);

                if (step < total_layers - 1) {
                    WaitPrefetchComplete();
                }
                current_layer = next_layer;
            }

            std::cout << dummy_tokens[token_idx] << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }

        // 🎯 [피드백 6 반영] 쿼리 종료 시 실제 디스크에서 퍼올린 순수 바이트 스트림 통계 표기
        std::cout << "\n--------------------------------------------------------------------";
        std::cout << "\n📊 [Telemetry] Total Bytes Streamed via Ring Buffers: " << GetTotalBytesStreamed() << " bytes\n";
        std::cout << "--------------------------------------------------------------------\n\n";
    }

    ~NexusStreamEngine() {
        if (is_initialized) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop_worker = true;
                // 🎯 [피드백 3] 의미가 중첩되던 task_ready = true 제거 후 조건 변수만 깔끔하게 알림
                cv_task.notify_one();
            }
            if (worker_thread.joinable()) {
                worker_thread.join();
            }
        }
        if (ring_buffer_slots[0]) _aligned_free(ring_buffer_slots[0]);
        if (ring_buffer_slots[1]) _aligned_free(ring_buffer_slots[1]);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    }
};

class StandardPredictableStrategy : public IStreamStrategy {
public:
    size_t GetNextLayerIndex(size_t current_step, size_t total_layers) override {
        return current_step % total_layers;
    }

    void ProcessLayerData(void* dataBlock, float* hiddenStates, size_t stride) override {
        GenericDataBlock* block = static_cast<GenericDataBlock*>(dataBlock);

        // 🎯 [피드백 5] 입력 읽기전용 버퍼의 원본 데이터 수정을 방지하는 스케일 불변성 가드
        float safe_scale = block->scale;
        if (!std::isfinite(safe_scale)) {
            safe_scale = 1.0f;
        }

        __m256 v_scale = _mm256_set1_ps(safe_scale);
        __m256 v_bias = _mm256_set1_ps(static_cast<float>(stride) * 0.01f);

        for (int i = 0; i < 64; i += 8) {
            __m256 v_hidden = _mm256_load_ps(&hiddenStates[i]);
            __m256 v_res = _mm256_fmadd_ps(v_hidden, v_scale, v_bias);
            _mm256_store_ps(&hiddenStates[i], v_res);
        }
    }
};

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    std::wstring dummy_payload = L"gpt-oss-20b-Q8_0.gguf";
    auto public_strategy = std::make_shared<StandardPredictableStrategy>();

    NexusStreamEngine engine(dummy_payload, 40, public_strategy);

    if (!engine.IsReady()) {
        std::cout << "====================================================================\n";
        std::cout << "[Initialization Failed] Target binary file container not found.\n";
        std::cout << "====================================================================\n";
        std::cout << "👉 HOW TO FIX AND RUN IMMEDIATELY:\n";
        std::cout << "1. Create a blank dummy file or rename any file to:\n";
        std::cout << "   '" << std::string(dummy_payload.begin(), dummy_payload.end()) << "'\n";
        std::cout << "2. Place it in the EXACT same folder where your compiled executable (.exe) is.\n";
        std::cout << "3. Restart the program to fire up the AVX2 pipeline streaming test.\n";
        std::cout << "====================================================================\n";

        std::cout << "\nPress Enter to close instance...";
        std::cin.get();
        return 1;
    }

    std::cout << "====================================================================\n";
    std::cout << " [Core] NexusStream Interactive Terminal Interface v0.3-Enterprise\n";
    std::cout << " [Boost] AVX2/FMA Hardware Vectorization Vector SIMD Enabled\n";
    std::cout << "====================================================================\n";
    std::cout << "  LOCK-ON: ENTERPRISE-GRADE IP PROTECTION (OPEN-CORE PARADIGM)\n\n";
    std::cout << "  Notice: The current 'StandardPredictableStrategy' is a secure,\n";
    std::cout << "          non-leaking demo interface. The pipeline physically reads\n";
    std::cout << "          the file bytes, but outputs a pre-defined token stream.\n\n";
    std::cout << "  🛠️  QUICK START GUIDE FOR REPOSITORY CLONERS:\n";
    std::cout << "  Step 1: [File Check] Ensure a mock file named 'gpt-oss-20b-Q8_0.gguf'\n";
    std::cout << "          exists in your execution path to engage the ring buffers.\n";
    std::cout << "  Step 2: [Customization] Open 'main.cpp' and locate the inherited\n";
    std::cout << "          'ProcessLayerData()' function inside the strategy class.\n";
    std::cout << "  Step 3: [Injection] Inject your proprietary matrix math, weights,\n";
    std::cout << "          or 3D fractal keys there to convert this chassis into your\n";
    std::cout << "          own secure, zero-dependency local inference machine.\n";
    std::cout << "====================================================================\n";
    std::cout << " Type your query and press Enter. (Type 'exit' to shutdown system)\n\n";

    std::string user_input;
    while (true) {
        std::cout << "User >> ";
        std::getline(std::cin, user_input);

        if (user_input == "exit" || user_input == "quit") {
            std::cout << "\n[System] Shutting down NexusStream substrate safely...\n";
            break;
        }

        if (user_input.empty()) continue;
        engine.QueryPipeline(user_input);
    }

    return 0;
}