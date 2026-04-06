#version 430 core
layout(local_size_x=16,local_size_y=16) in;

layout(rgba32f,binding=0) uniform image2D img_in;
layout(rgba32f,binding=1) uniform image2D img_out;
layout(r32f   ,binding=2) uniform image2D img_mask;

uniform float uDt, uH, uL, uBField, uQ, uXi, uLambda, uGamma;
uniform bool uUseNoise;
uniform float uTime;

void main(){
  ivec2 curr = ivec2(gl_GlobalInvocationID.xy);
  float mask = imageLoad(img_mask, curr).rg;

  // check for vacuum
  if (mask < 0.5) {
    imageStore(img_out, curr, vec4(0,0));
    return;
  }

  vec2  p   = imageLoad(img_in, curr).rg;
  ivec2 dim = imageSize(img_in);

  auto getOrderParam = [&](ivec2 pos) {
    return (imageLoad(img_mask, pos).r > 0.5) ? imageLoad(img_in, pos).rg : p;
  };

  float xPhys  = (float(curr.x) - 0.5 * float(dim.x - 1)) * uH;
  float Ay     = uBField * xPhys;
  float yPhase = uQ * Ay * uH;
  vec2  linkY  = vec2(cos(yPhase), sin(yPhase));

  ivec2 up    = getOrderParam(ivec2(curr.x, (curr.y + 1) % dim.y);
  ivec2 down  = ivec2(curr.x, (curr.y - 1 + dim.y) % dim.y);

  vec2 noP    = imageLoad(img_in, up).rg;
  vec2 soP    = imageLoad(img_in, down).rg;

  noP         = vec2(noP.x * linkY.x - noP.y * linkY.y,
                     noP.x * linkY.y + noP.y * linkY.x);
  soP         = vec2(soP.x * linkY.x + soP.y * linkY.y,
                    -soP.x * linkY.y + soP.y * linkY.x);

  vec2 rhs = (noP + soP - 2.0 * p)/(uH * uH) - (p * (p.x * p.x + p.y * p.y));

  vec2 p_next = p + uDt*rhs;

  // noise (scaled properly)
  if (uUseNoise) {
    float noiseAmp = 0.1; // fraction of equilibrium |ψ|
    vec2 seed1 = vec2(curr)+uTime*1.7321;
    vec2 seed2 = vec2(curr)+uTime*2.6457+1.23;
    float n1 = fract(sin(dot(seed1,vec2(12.9898,78.233)))*43758.5453);
    float n2 = fract(sin(dot(seed2,vec2(4.898,7.230)))*23421.631);
    vec2 noise = noiseAmp*vec2(n1-0.5,n2-0.5);
    p_next += sqrt(uDt)*noise;
  }

  imageStore(img_out,curr,vec4(p_next,0.0,0.0));
}
