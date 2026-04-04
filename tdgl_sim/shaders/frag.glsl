#version 330 core
in vec2 fragCoord;
out vec4 FragColor;

uniform sampler2D uField; // red = real, green = imaginary
uniform float uH, uL, uQ, uBField, uAlpha, uBeta, uPhiEq2;

// uRenderMode = {0,1} -- control for what tile is being rendered
//   0: phase(color)/contour(lines)  (vortex map)
//   1: height(lines)/current(cyan) (cooper pair density + supercurrents)
uniform int uRenderMode; 

#define PI 3.14159265359

// HSV to RGB
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    // setup shared coordinates
    vec2 uv     = fragCoord * 0.5 + 0.5;
    float texel = 1.0 / float(textureSize(uField, 0).x); // based on dim

    // sample raw data (real/imaginary)
    vec2 data   = texture(uField, uv).rg;
    float real  = data.x;
    float imag  = data.y;

    // calculate magnitude
    float mag2      = real * real + imag * imag;
    float mag       = sqrt(mag2);
    float norm_mag  = (uPhiEq2 > 0.0) ? clamp(mag / sqrt(uPhiEq2), 0.0, 1.0) : 0.0;

    if (uRenderMode == 0) {
        // PHASE & CONTOURS 
        float phase = atan(imag, real); // range [-PI, PI]
        float hue   = (phase + PI) / (2.0 * PI);

        // magnitude grid
        float logMag = log(mag + 0.0001); 
        float rings  = sin(2.0 * PI * logMag * 1.5); // adjust 1.5 for ring density
        float gridM  = smoothstep(0.7, 1.0, rings);

        // phase grid
        float spokes = sin(phase * 8.0); // 8 spokes for 45-degree increments
        float gridP  = smoothstep(0.8, 1.0, spokes);

        // combine into domain coloring
        vec3 baseColor = hsv2rgb(vec3(hue, 0.8, 0.9));

        // darken base color based on the grid lines
        // use norm_mag so color fades to black incores
        float grid = clamp(gridM + gridP, 0.0, 1.0);
        vec3 finalColor = baseColor * (1.0 - 0.3 * grid) * norm_mag;

        float contour = sin(norm_mag * 25.0); 
        float line    = smoothstep(0.9, 1.0, contour);
        finalColor    = mix(finalColor, vec3(1.0), line * 0.2);

        FragColor = vec4(finalColor, 1.0);
    }  else {
        // HEIGHT MAP & SUPERCURRENT

        // sample neighbors for spatial gradients
        vec2 dX = texture(uField, uv + vec2(texel, 0.0)).rg;
        vec2 dY = texture(uField, uv + vec2(0.0, texel)).rg;

        // covariance considerations
        if (uv.x + texel > 1.0) {
          float yPhys = uv.y * uL;
          float shift = -uQ * uBField * uL * yPhys;

          dX = vec2(dX.x * cos(shift) - dX.y * sin(shift),
                    dX.x * sin(shift) + dX.y * cos(shift));
        }

        float Ay = uBField * (uv.x * uL - 0.5f * uL);

        // covariant finite differences: D_x phi ~ (phi_{i+1} - phi_i) / h
        // uv space: texel == 1 grid step w/ size h = uH

        // FIX: Implemented Lattice Supercurrent (Covariant Finite Difference)
        // This is gauge-invariant and prevents current oversaturation at high fields.
        // J_y = (1/h) * Im( psi*(n) * U_y(n) * psi(n+y) )

        vec2 linkY      = vec2(cos(uQ * Ay * uH), sin(uQ * Ay * uH));
        // Covariant neighbor: U_y * psi(y+h)
        vec2 shifted_dY = vec2(dY.x   * linkY.x + dY.y * linkY.y, 
                               -dY.x  * linkY.y + dY.y * linkY.x);

        // vec2 dPhi_x = (dX - data) / uH; // OLD
        // vec2 dPhi_y = (dY - data) / uH; // OLD

        // apply x-gauge (Ax=0 in Landau)
        // apply y-gauge D_y phi = (U_y * phi_{j+1} - phi) / h, 
        //    Ay already accounted via texel sample

        // Supercurrent: J = Im(phi* D phi) - q*A*|phi|^2
        // Jx = Re(phi) * Im(D_x phi) - Im(phi) * Re(D_x phi) // OLD
        // float Jx = real * dPhi_x.y - imag * dPhi_x.x; // OLD: Gauge dependent

        // Jx = Im(psi* * psi_next_x) / h (Ax = 0 implies Ux = 1)
        float Jx    = (real * dX.y - imag * dX.x) / uH; 

        // Jy = Im(phi* D_y phi) - q*Ay*|phi|^2 // OLD
        // float Jy = (real * dPhi_y.y - imag * dPhi_y.x) - (uQ * Ay * mag2); // OLD: Numerically unstable

        // Jy = Im(psi* * shifted_dY) / h
        float Jy    = (real * shifted_dY.y - imag * shifted_dY.x) / uH;
        float J_mag = length(vec2(Jx, Jy)) * 10.0;

        // height map lighting
        float h_right = length(dX);
        float h_up    = length(dY);
        vec3 normal   = normalize(vec3(mag - h_right, mag - h_up, 0.02));
        vec3 lightDir = normalize(vec3(0.5, 0.5, 1.0));
        float diff    = max(dot(normal, lightDir), 0.0);

        // base color + diffuse lighting + cyan current glow
        vec3 baseColor = vec3(0.02, 0.02, 0.05) + diff * vec3(0.35);
        vec3 currentGlow = vec3(0.0, 1.0, 1.0) * J_mag;

        FragColor = vec4(baseColor + currentGlow, 1.0);
    }
}
