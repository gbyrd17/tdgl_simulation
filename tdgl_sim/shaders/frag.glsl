#version 330 core

in vec2 fragCoord; 
out vec4 fragColor;

uniform sampler2D uField; 
uniform sampler2D uMask;  
uniform int uRenderMode;  

uniform float uH, uL, uAlpha, uBeta, uQ, uBField, uEpsilon;

#define PI 3.14159265359

vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 viridis(float t) {
  const vec3 c0 = vec3(0.2777, 0.0054, 0.3341);
  const vec3 c1 = vec3(0.1051, 1.4046, 1.3846);
  const vec3 c2 = vec3(-0.3309, 0.2177, 0.9524);
  const vec3 c3 = vec3(-4.6372, -1.9639, -3.4541);
  const vec3 c4 = vec3(10.3079, 3.1208, 2.7513);
  const vec3 c5 = vec3(-5.7159, -1.3519, -0.9678);
  return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * c5))));
}

void main() {
  vec2 uv = fragCoord * 0.5 + 0.5;
  float mask = texture(uMask, uv).r;

  if (mask < 0.5) {
    fragColor = vec4(0.02, 0.02, 0.05, 1.0);
    return;
  }

  vec2 data = texture(uField, uv).rg;
  float mag = length(data);
  float phase = atan(data.y, data.x);
  float texel = 1.0 / float(textureSize(uField, 0).x);

  if (uRenderMode == 0) {
    // MODE 0: VIRIDIS MAGNITUDE
    float phiEq = sqrt(uEpsilon);
    float displaymag = clamp(mag / phiEq, 0.0, 1.5);
    vec3 col = viridis(displaymag);
    col *= mix(0.65, 1.0, smoothstep(0.0, 0.8 * phiEq, mag));
    fragColor = vec4(col, 1.0);

  } else if (uRenderMode == 1) {
    // MODE 1: PHASE + COLOR BAR
    float phase_norm = fract((phase / (2.0 * 3.14159265359)) + 1.0);
    float value = clamp(mag * 5.0, 0.15, 1.0);
    vec3 phaseCol = hsv2rgb(vec3(phase_norm, 1.0, value));
    
    // Draw a vertical color legend at the right edge of the viewport
    vec3 legend = vec3(0.0);
    if (uv.x > 0.90) {
      float t = smoothstep(0.90, 1.0, uv.x);
      float y = uv.y;
      float hue = fract(1.0 - y);
      legend = hsv2rgb(vec3(hue, 1.0, 0.9));
    }

    fragColor = vec4(mix(phaseCol, legend, step(0.90, uv.x)), 1.0);

  } else {
    // MODE 2: SUPERCURRENTS
    vec2 dX = texture(uField, uv + vec2(texel, 0.0)).rg;
    vec2 dY = texture(uField, uv + vec2(0.0, texel)).rg;

    // Use uL, uBField, uQ, uH to prevent link errors
    float yPhys = (uv.y - 0.5) * uL;
    float Ax = -uBField * yPhys; 
    vec2 linkX = vec2(cos(uQ * Ax * uH), sin(uQ * Ax * uH));

    vec2 shifted_dX = vec2(dX.x * linkX.x - dX.y * linkX.y, 
                           dX.x * linkX.y + dX.y * linkX.x);

    float Jx = (data.x * shifted_dX.y - data.y * shifted_dX.x) / uH;
    float Jy = (data.x * dY.y - data.y * dY.x) / uH;
    float J_mag = length(vec2(Jx, Jy));

    float hue = (phase + PI) / (2.0 * PI);
    vec3 phaseCol = hsv2rgb(vec3(hue, 0.8, 0.1));
    float currentIntensity = smoothstep(0.0, 0.1, J_mag); 
    vec3 currentGlow = vec3(0.0, 1.0, 1.0) * currentIntensity;

    fragColor = vec4(phaseCol + currentGlow, 1.0);
  }
}
