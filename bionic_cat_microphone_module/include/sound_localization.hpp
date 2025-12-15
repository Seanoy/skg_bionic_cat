#ifndef SOUND_LOCALIZATION_HPP
#define SOUND_LOCALIZATION_HPP

#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include <iostream>
namespace BionicCat {
namespace MicrophoneModule {

struct Vec3 {
    float x;
    float y;
    float z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    float magnitude() const;
    Vec3 normalize() const;
};

struct MicArrayConfig {
    std::string array_type{"tetrahedron"};
    bool is_planar{false};
    uint32_t sample_rate{48000};
    uint32_t frame_size{1024};
    int32_t max_delay_samples{64};
    uint32_t channels{4};
    float audio_gain{1.0f};
    float sound_speed{343.0f};
    std::array<Vec3, 4> mic_positions{};
    float min_confidence{0.3f};
    bool smoothing_enabled{true};
    float smoothing_alpha{0.3f};
};

class MicArrayLocalizer {
public:
    explicit MicArrayLocalizer(const MicArrayConfig& config);

    bool localize(const std::array<std::vector<float>, 4>& audio_data,
                  uint32_t num_samples,
                  float& azimuth, float& elevation, float& confidence);

    static MicArrayConfig loadConfig(const std::string& filepath);

    void calc4chSeparateDb(const std::array<std::vector<float>, 4>& channels,
                           double* db_out) const;

private:
    MicArrayConfig config_;
    Vec3 smoothed_direction_;
    bool first_result_{true};
    int max_theoretical_delay_{0};

    bool solve2D(const std::array<float, 3>& tdoa, Vec3& direction);
    bool solve3D(const std::array<float, 3>& tdoa, Vec3& direction);
    float computeCrossCorrelation(const float* sig1, const float* sig2,
                                  uint32_t length, int32_t max_delay,
                                  int32_t& best_delay);
                                   
};

} // namespace MicrophoneModule
} // namespace BionicCat

#endif // SOUND_LOCALIZATION_HPP
