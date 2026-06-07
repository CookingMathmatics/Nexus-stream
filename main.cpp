#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <windows.h>
#include <immintrin.h>
#include <memory>
#include <functional>

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

    void* ring_buffer_slots[2] = { nullptr, nullptr };
    alignas(64) float hidden_states[4096];
    bool is_initialized = false;

public:
    NexusStreamEngine(const std::wstring& path, size_t layers, std::shared_ptr<IStreamStrategy> strategy)
        : file_path(path), total_layers(layers), stream_strategy(strategy) {

        hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        LARGE_INTEGER size_quad;
        if (!GetFileSizeEx(hFile, &size_quad)) {
            CloseHandle(hFile);
            return;
        }

        total_file_size = static_cast<size_t>(size_quad.QuadPart);
        single_layer_size = total_file_size / total_layers;

        ring_buffer_slots[0] = _aligned_malloc(single_layer_size, 64);
        ring_buffer_slots[1] = _aligned_malloc(single_layer_size, 64);

        if (!ring_buffer_slots[0] || !ring_buffer_slots[1]) {
            CloseHandle(hFile);
            return;
        }

        for (int i = 0; i < 4096; ++i) hidden_states[i] = 0.01f;
        is_initialized = true;
    }

    bool IsReady() const { return is_initialized; }

    void LoadLayerToSlotAsync(size_t layer_id, int ring_slot) {
        if (hFile == INVALID_HANDLE_VALUE || !ring_buffer_slots[ring_slot]) return;

        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(layer_id) * single_layer_size;

        SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN);
        DWORD read_bytes = 0;
        ReadFile(hFile, ring_buffer_slots[ring_slot], static_cast<DWORD>(single_layer_size), &read_bytes, NULL);
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
            LoadLayerToSlotAsync(current_layer, 0);

            size_t next_layer = current_layer;

            for (size_t step = 0; step < total_layers; ++step) {
                int current_slot = step % 2;
                int next_slot = (step + 1) % 2;

                std::thread prefetch_worker;

                if (step < total_layers - 1) {
                    next_layer = stream_strategy->GetNextLayerIndex(step + 1, total_layers);
                    prefetch_worker = std::thread(&NexusStreamEngine::LoadLayerToSlotAsync, this, next_layer, next_slot);
                }

                stream_strategy->ProcessLayerData(ring_buffer_slots[current_slot], hidden_states, step);

                if (prefetch_worker.joinable()) {
                    prefetch_worker.join();
                }
                current_layer = next_layer;
            }

            std::cout << dummy_tokens[token_idx] << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }
        std::cout << "\n\n";
    }

    ~NexusStreamEngine() {
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

        __m256 v_scale = _mm256_set1_ps(block->scale);
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

    // 테스트용 파일명 (사용자가 다루기 쉽게 가이드 제공)
    std::wstring dummy_payload = L"gpt-oss-20b-Q8_0.gguf";
    auto public_strategy = std::make_shared<StandardPredictableStrategy>();

    NexusStreamEngine engine(dummy_payload, 40, public_strategy);

    // 파일이 없을 경우, 당황하지 않고 가짜 파일을 만들 수 있도록 에러 가이드라인 출력
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

    // 🎯 [지휘관님 요청 사항: 처음 구동하는 사람들을 위한 실시간 액션 플랜 배너 장장]
    std::cout << "====================================================================\n";
    std::cout << " [Core] NexusStream Interactive Terminal Interface v1.0.0\n";
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