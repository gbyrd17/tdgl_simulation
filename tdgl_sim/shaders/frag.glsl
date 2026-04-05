#version 330 core
in vec2 fragCoord;
out vec4 FragColor;

uniform sampler2D uField;
uniform float uH, uL, uQ, uBField, uPhiEq2;
uniform int uRenderMode;

#define PI 3.14159265359

vec3 hsv2rgb(vec3 c) {
    vec4 K    = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p    = abs(fract(c.xxx + K.xyz)*6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p-K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 uv = fragCoord * 0.5 + 0.5; // normalized
    float texel = 1.0 / float(textureSize(uField,0).x);

    vec2 data = texture(uField, uv).rg;
    float mag2 = data.x*data.x + data.y*data.y;
    float mag  = sqrt(mag2);
    float norm_mag = (uPhiEq2>0.0) ? clamp(mag/sqrt(uPhiEq2),0.0,1.0) : 0.0;

    if(uRenderMode==0){
        float phase = atan(data.y,data.x);
        float hue = (phase+PI)/(2.0*PI);

        float logMag = log(mag+1e-6);
        float rings = sin(2.0*PI*logMag*1.5);
        float gridM = smoothstep(0.7,1.0,rings);

        float spokes = sin(phase*8.0);
        float gridP = smoothstep(0.8,1.0,spokes);

        vec3 baseColor = hsv2rgb(vec3(hue,0.8,0.9));
        float grid = clamp(gridM + gridP,0.0,1.0);
        vec3 finalColor = baseColor*(1.0-0.3*grid)*norm_mag;

        float contour = sin(norm_mag*25.0);
        float line = smoothstep(0.9,1.0,contour);
        finalColor = mix(finalColor,vec3(1.0),line*0.2);

        FragColor = vec4(finalColor,1.0);

    } else {
        // --- Supercurrent map ---
        vec2 dX = texture(uField, uv + vec2(texel,0.0)).rg;
        vec2 dY = texture(uField, uv + vec2(0.0,texel)).rg;

        float xPhys = (uv.x - 0.5)*uL;
        float yPhys = (uv.y - 0.5)*uL;
        float Ay = uBField * xPhys;

        vec2 linkY = vec2(cos(uQ*Ay*uH), sin(uQ*Ay*uH));
        vec2 shifted_dY = vec2(dY.x*linkY.x + dY.y*linkY.y, -dY.x*linkY.y + dY.y*linkY.x);

        float Jx = (data.x*dX.y - data.y*dX.x)/uH;
        float Jy = (data.x*shifted_dY.y - data.y*shifted_dY.x)/uH;
        float J_mag = length(vec2(Jx,Jy))*10.0;

        float hR = length(dX);
        float hU = length(dY);
        vec3 normal = normalize(vec3(mag - hR, mag - hU,0.02));
        vec3 lightDir = normalize(vec3(0.5,0.5,1.0));
        float diff = max(dot(normal,lightDir),0.0);

        vec3 baseColor = vec3(0.02,0.02,0.05) + diff*vec3(0.35);
        vec3 currentGlow = vec3(0.0,1.0,1.0)*J_mag;

        FragColor = vec4(baseColor + currentGlow,1.0);
    }
}
