#include <cmath>
#include <glm/glm.hpp>

class MagneticProfile {
public:
  enum class RampType { LINEAR, EXPONENTIAL, SIGMOID };

  struct Ramp {
    float t_start, t_end;
    glm::vec3 b_initial;
    glm::vec3 b_final;
    RampType type;

    bool isValid() const {
      return t_start < t_end && std::isfinite(b_initial.x) &&
             std::isfinite(b_final.x);
    }
  };

  MagneticProfile();

  void addRamp(float t_start, float t_end, glm::vec3 b_initial,
               glm::vec3 b_final, RampType type);

  glm::vec3 evaluate(float sim_time);

  const std::vector<Ramp> &getRamps() const { return ramps; }
  bool isEmpty() const { return ramps.empty(); }
  float getDuration() const;

  bool validate() const;

private:
  std::vector<Ramp> ramps;
  glm::vec3 b_cached = glm::vec3(0.0f);
  float last_eval_time = -1.0f;

  float interpolate(float t, float t_start, float t_end, float val_start,
                    float val_end, RampType type) const;

  const Ramp *whatRamp(float sim_time) const;
};
