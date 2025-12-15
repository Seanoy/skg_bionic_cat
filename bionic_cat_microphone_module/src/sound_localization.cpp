#include "sound_localization.hpp"
#include <cmath>
#include <yaml-cpp/yaml.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BionicCat {
namespace MicrophoneModule {
    /**
 * @brief 检查麦克风阵列是否为平面阵列 (共面检测)
 * 使用体积法：如果4点共面，则四面体体积为0
 */
bool checkPlanarArray(const std::array<Vec3, 4>& positions) {
    // 计算向量
    Vec3 v1 = positions[1] - positions[0];
    Vec3 v2 = positions[2] - positions[0];
    Vec3 v3 = positions[3] - positions[0];
    
    // 计算混合积 (v1 · (v2 × v3))，即四面体体积的6倍
    // 叉积 v2 × v3
    Vec3 cross;
    cross.x = v2.y * v3.z - v2.z * v3.y;
    cross.y = v2.z * v3.x - v2.x * v3.z;
    cross.z = v2.x * v3.y - v2.y * v3.x;
    
    // 点积 v1 · cross
    float volume6 = v1.x * cross.x + v1.y * cross.y + v1.z * cross.z;
    
    // 如果体积接近0，则共面
    return fabsf(volume6) < 1e-6f;
}
/**
 * @brief 生成正四面体阵列位置
 */
void generateTetrahedronPositions(std::array<Vec3, 4>& positions, float edge_length) {
    float a = edge_length;
    float h = a * sqrtf(2.0f / 3.0f);  // 四面体高度
    float r = a / sqrtf(3.0f);          // 底面外接圆半径
    
    // 顶点 (在上方)
    positions[0] = Vec3(0, 0, h * 0.75f);
    
    // 底面三个顶点 (等边三角形)
    positions[1] = Vec3(r, 0, -h * 0.25f);
    positions[2] = Vec3(-r * 0.5f, r * sqrtf(3.0f) / 2.0f, -h * 0.25f);
    positions[3] = Vec3(-r * 0.5f, -r * sqrtf(3.0f) / 2.0f, -h * 0.25f);
}
/**
 * @brief 生成正方形阵列位置
 */
void generateSquarePositions(std::array<Vec3, 4>& positions, float edge_length) {
    float half = edge_length / 2.0f;
    
    // 左上、左下、右下、右上 (逆时针)
    positions[0] = Vec3(-half, half, 0);   // 左上
    positions[1] = Vec3(-half, -half, 0);  // 左下
    positions[2] = Vec3(half, -half, 0);   // 右下
    positions[3] = Vec3(half, half, 0);    // 右上
};
// ===== Vec3 =====
float Vec3::magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 Vec3::normalize() const {
    float mag = magnitude();
    if (mag > 1e-6f) {
        return Vec3(x / mag, y / mag, z / mag);
    }
    return Vec3(0.f, 0.f, 0.f);
}

static inline float radToDeg(float rad) { return rad * 180.0f / M_PI; }

// ===== MicArrayLocalizer =====
MicArrayLocalizer::MicArrayLocalizer(const MicArrayConfig& config)
    : config_(config) {
    smoothed_direction_ = Vec3(1.f, 0.f, 0.f);

    // 计算最大理论延迟（基于麦克风最大间距）
    float max_dist = 0.0f;
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            Vec3 d = config_.mic_positions[j] - config_.mic_positions[i];
            float dist = d.magnitude();
            if (dist > max_dist) max_dist = dist;
        }
    }
    float max_delay_sec = max_dist / config_.sound_speed;
    max_theoretical_delay_ = static_cast<int>(max_delay_sec * config_.sample_rate) + 1;
}

bool MicArrayLocalizer::localize(const std::array<std::vector<float>, 4>& audio_data,
                                 uint32_t num_samples,
                                 float& azimuth, float& elevation, float& confidence) {
    std::array<float, 3> tdoa{};
    float total_correlation = 0.0f;

    for (int i = 1; i < 4; ++i) {
        int32_t delay_samples = 0;
        float corr = computeCrossCorrelation(
            audio_data[0].data(),
            audio_data[i].data(),
            num_samples,
            config_.max_delay_samples,
            delay_samples);
        tdoa[i - 1] = -(float)delay_samples / config_.sample_rate;
        total_correlation += corr;
    }
    confidence = total_correlation / 3.0f;

    Vec3 direction;
    bool ok = false;
    if (config_.is_planar) {
        ok = solve2D(tdoa, direction);
    } else {
        ok = solve3D(tdoa, direction);
    }
    if (!ok) return false;

    direction = direction.normalize();

    if (config_.smoothing_enabled) {
        if (first_result_) {
            smoothed_direction_ = direction;
            first_result_ = false;
        } else {
            smoothed_direction_ = direction * config_.smoothing_alpha +
                                  smoothed_direction_ * (1.0f - config_.smoothing_alpha);
            smoothed_direction_ = smoothed_direction_.normalize();
        }
        direction = smoothed_direction_;
    }

    azimuth = radToDeg(std::atan2(direction.y, direction.x));
    elevation = radToDeg(std::asin(direction.z));
    return true;
}

bool MicArrayLocalizer::solve2D(const std::array<float, 3>& tdoa, Vec3& direction) {
    float A[2][2] = {{0}};
    float b[2] = {0};

    for (int i = 1; i < 4; ++i) {
        Vec3 diff = config_.mic_positions[i] - config_.mic_positions[0];
        float rangeD = tdoa[i - 1] * config_.sound_speed;
        A[0][0] += diff.x * diff.x;
        A[0][1] += diff.x * diff.y;
        A[1][0] += diff.y * diff.x;
        A[1][1] += diff.y * diff.y;
        b[0] += diff.x * rangeD;
        b[1] += diff.y * rangeD;
    }

    float det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    if (std::fabs(det) < 1e-10f) return false;
    float invDet = 1.0f / det;

    direction.x = (A[1][1] * b[0] - A[0][1] * b[1]) * invDet;
    direction.y = (A[0][0] * b[1] - A[1][0] * b[0]) * invDet;
    direction.z = 0.0f;
    return true;
}

bool MicArrayLocalizer::solve3D(const std::array<float, 3>& tdoa, Vec3& direction) {
    float A[3][3] = {{0}};
    float b[3] = {0};

    for (int i = 1; i < 4; ++i) {
        Vec3 diff = config_.mic_positions[i] - config_.mic_positions[0];
        float rangeD = tdoa[i - 1] * config_.sound_speed;
        A[0][0] += diff.x * diff.x;
        A[0][1] += diff.x * diff.y;
        A[0][2] += diff.x * diff.z;
        A[1][0] += diff.y * diff.x;
        A[1][1] += diff.y * diff.y;
        A[1][2] += diff.y * diff.z;
        A[2][0] += diff.z * diff.x;
        A[2][1] += diff.z * diff.y;
        A[2][2] += diff.z * diff.z;
        b[0] += diff.x * rangeD;
        b[1] += diff.y * rangeD;
        b[2] += diff.z * rangeD;
    }

    float det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
              - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
              + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (std::fabs(det) < 1e-10f) return false;
    float invDet = 1.0f / det;

    float invA[3][3];
    invA[0][0] = (A[1][1] * A[2][2] - A[1][2] * A[2][1]) * invDet;
    invA[0][1] = (A[0][2] * A[2][1] - A[0][1] * A[2][2]) * invDet;
    invA[0][2] = (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invDet;
    invA[1][0] = (A[1][2] * A[2][0] - A[1][0] * A[2][2]) * invDet;
    invA[1][1] = (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invDet;
    invA[1][2] = (A[0][2] * A[1][0] - A[0][0] * A[1][2]) * invDet;
    invA[2][0] = (A[1][0] * A[2][1] - A[1][1] * A[2][0]) * invDet;
    invA[2][1] = (A[0][1] * A[2][0] - A[0][0] * A[2][1]) * invDet;
    invA[2][2] = (A[0][0] * A[1][1] - A[0][1] * A[1][0]) * invDet;

    direction.x = invA[0][0] * b[0] + invA[0][1] * b[1] + invA[0][2] * b[2];
    direction.y = invA[1][0] * b[0] + invA[1][1] * b[1] + invA[1][2] * b[2];
    direction.z = invA[2][0] * b[0] + invA[2][1] * b[1] + invA[2][2] * b[2];
    return true;
}

float MicArrayLocalizer::computeCrossCorrelation(const float* sig1, const float* sig2,
                                                 uint32_t length, int32_t max_delay,
                                                 int32_t& best_delay) {
    float max_corr = -1e30f;
    best_delay = 0;

    for (int32_t delay = -max_delay; delay <= max_delay; ++delay) {
        float corr = 0.0f;
        float energy1 = 0.0f;
        float energy2 = 0.0f;

        uint32_t start = (delay >= 0) ? (uint32_t)delay : 0;
        uint32_t end = (delay >= 0) ? length : length + delay;

        for (uint32_t i = start; i < end; ++i) {
            int32_t j = (int32_t)i - delay;
            if (j >= 0 && j < (int32_t)length) {
                corr += sig1[i] * sig2[j];
                energy1 += sig1[i] * sig1[i];
                energy2 += sig2[j] * sig2[j];
            }
        }

        float norm = std::sqrt(energy1 * energy2);
        if (norm > 1e-10f) {
            corr /= norm;
        }
        if (corr > max_corr) {
            max_corr = corr;
            best_delay = delay;
        }
    }
    return max_corr;
}

MicArrayConfig MicArrayLocalizer::loadConfig(const std::string& filepath) {
    MicArrayConfig config;
    std::cout << "Loading microphone array configuration from: " << filepath << std::endl;
    try {
        YAML::Node yaml = YAML::LoadFile(filepath);
        
        // 阵列类型
        if (yaml["microphones"] && yaml["microphones"]["array_type"]) {
            config.array_type = yaml["microphones"]["array_type"].as<std::string>();
        }
        
        // 音频参数
        if (yaml["audio"]) {
            if (yaml["audio"]["sample_rate"])
                config.sample_rate = yaml["audio"]["sample_rate"].as<uint32_t>();
            if (yaml["audio"]["frame_size"])
                config.frame_size = yaml["audio"]["frame_size"].as<uint32_t>();
            if (yaml["audio"]["max_delay_samples"])
                config.max_delay_samples = yaml["audio"]["max_delay_samples"].as<int32_t>();
            if (yaml["audio"]["channels"])
                config.channels = yaml["audio"]["channels"].as<uint32_t>();
            if (yaml["audio"]["gain"])
                config.audio_gain = yaml["audio"]["gain"].as<float>();
        }
        
        // 环境参数
        if (yaml["environment"]) {
            if (yaml["environment"]["sound_speed"])
                config.sound_speed = yaml["environment"]["sound_speed"].as<float>();
        }
        
        // 麦克风位置
        if (yaml["microphones"]) {
            // 检查是否使用预定义阵列
            if (config.array_type == "tetrahedron" && yaml["microphones"]["tetrahedron"]) {
                float edge_length = 0.08f;
                if (yaml["microphones"]["tetrahedron"]["edge_length"]) {
                    edge_length = yaml["microphones"]["tetrahedron"]["edge_length"].as<float>();
                }
                generateTetrahedronPositions(config.mic_positions, edge_length);
                std::cout << "使用正四面体阵列，边长: " << (edge_length * 100) << " cm" << std::endl;
            }
            else if (config.array_type == "square" && yaml["microphones"]["square"]) {
                float edge_length = 0.08f;
                if (yaml["microphones"]["square"]["edge_length"]) {
                    edge_length = yaml["microphones"]["square"]["edge_length"].as<float>();
                }
                generateSquarePositions(config.mic_positions, edge_length);
                std::cout << "使用正方形阵列，边长: " << (edge_length * 100) << " cm" << std::endl;
            }
            else if (yaml["microphones"]["positions"]) {
                // 自定义位置
                for (int i = 0; i < 4; i++) {
                    std::string key = "mic_" + std::to_string(i);
                    if (yaml["microphones"]["positions"][key]) {
                        config.mic_positions[i].x = yaml["microphones"]["positions"][key]["x"].as<float>();
                        config.mic_positions[i].y = yaml["microphones"]["positions"][key]["y"].as<float>();
                        config.mic_positions[i].z = yaml["microphones"]["positions"][key]["z"].as<float>();
                    }
                }
                std::cout << "使用自定义麦克风位置" << std::endl;
            }
        }
        
        // 自动检测是否为平面阵列
        config.is_planar = checkPlanarArray(config.mic_positions);
        
        // 定位参数
        if (yaml["localization"]) {
            if (yaml["localization"]["min_confidence"])
                config.min_confidence = yaml["localization"]["min_confidence"].as<float>();
            if (yaml["localization"]["smoothing"]) {
                if (yaml["localization"]["smoothing"]["enabled"])
                    config.smoothing_enabled = yaml["localization"]["smoothing"]["enabled"].as<bool>();
                if (yaml["localization"]["smoothing"]["alpha"])
                    config.smoothing_alpha = yaml["localization"]["smoothing"]["alpha"].as<float>();
            }
        }
          
        std::cout << "配置文件加载成功: " << filepath << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "加载配置文件失败: " << e.what() << std::endl;
        std::cerr << "使用默认正四面体配置" << std::endl;
        generateTetrahedronPositions(config.mic_positions, 0.08f);
        config.is_planar = false;
    }
      return config;
}

void MicArrayLocalizer::calc4chSeparateDb(const std::array<std::vector<float>, 4>& channels,
                                          double* db_out) const {
    const int num_channels = 4;
    for (int ch = 0; ch < num_channels; ++ch) {
        const auto& sig = channels[ch];
        double rms = 0.0;
        for (float v : sig) rms += (double)v * (double)v;
        rms = sig.empty() ? 0.0 : std::sqrt(rms / (double)sig.size());
        double db = (rms <= 1e-12) ? -120.0 : 20.0 * std::log10(rms);
        db_out[ch] = db;
    }
}

} // namespace MicrophoneModule
} // namespace BionicCat
