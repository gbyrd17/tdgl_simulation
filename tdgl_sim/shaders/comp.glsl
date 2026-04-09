#version 430 core
layout(local_size_x=16,local_size_y=16) in;

layout(rgba32f, binding=0) uniform image2D img_in;
layout(rgba32f, binding=1) uniform image2D img_out;
layout(r32f   , binding=2) uniform image2D img_mask;
uniform sampler2D img_sampler;

uniform float uDt, uH, uL, uBField, uQ, uXi, uAlpha, uLambda, uGamma, uRelax, uEpsilon;
uniform bool uUseNoise;
uniform float uTime;

vec2 cMult (vec2 a, vec2 b) {
  return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x);
}

vec2 cConj (vec2 a) {
  return vec2(a.x, -a.y);
}

void main() {
  ivec2 curr = ivec2(gl_GlobalInvocationID.xy);
  ivec2 dim = imageSize(img_in);
  float mask = imageLoad(img_mask, curr).r;
  if (curr.x >= dim.x || curr.y >= dim.y) {return;}

  // check for vacuum
  if (mask < 0.5) {
    imageStore(img_out, curr, vec4(0, 0, 0, 1));
    return;
  }

  vec2 phi = imageLoad(img_in, curr).rg; 

  // find neighbor vals using texture sampling (periodic boundaries)
  vec2 texCoord = (vec2(curr) + 0.5f) / vec2(dim);  // center of texel
  vec2 phiUp    = texture(img_sampler, texCoord + vec2(0.0f, 1.0f/dim.y)).rg;
  vec2 phiDown  = texture(img_sampler, texCoord + vec2(0.0f, -1.0f/dim.y)).rg;
  vec2 phiRight = texture(img_sampler, texCoord + vec2(1.0f/dim.x, 0.0f)).rg;
  vec2 phiLeft  = texture(img_sampler, texCoord + vec2(-1.0f/dim.x, 0.0f)).rg;

  // calc link variables in landau gauge with B in z dir ==> A = (-B*y, 0, 0)
  // particle moving in x dir gets phase shift proportional to B*y*dx
  float yDist   = (float(curr.y) - 0.5f * float(dim.y - 1.0f)) * uH;     // y position relative to center
  float xPhase  = uQ * uBField * yDist * uH;                     // phase shift for x-links: q*B*y*dx
  vec2 xU       = vec2(cos(xPhase), sin(xPhase));                 // x-direction link variable
  vec2 yU       = vec2(1.0, 0.0);                                 // y-direction link variable (no B field)

  // eq1 in pytdgl paper tells us that
  //      (u / sqrt(1 + gamma^2 |phi^2|)) * (d phi / dt + i*mu + (gamma^2 / 2) * (d|phi|^2 / dt)phi)...
  //            ... = epsilon*phi - |phi|^2 * phi + laplacian

  // covariant laplacian: sum( U_ij * phi_j - phi_i ) if U is spacial link var
  float weight   = 1.0f; // edge_length / distance; trivial for structured grids ####
  vec2 laplacian =  weight * ((cMult(xU, phiRight)  - phi) +
                    (cMult(cConj(xU), phiLeft)      - phi) +
                    (cMult(yU, phiUp)               - phi) +
                    (cMult(cConj(yU), phiDown)      - phi));
  laplacian     /= (uH * uH);       // effective area; trivial for structured grids ####
  float lapScale = 1.0f; // gradient energy scaling; must be 1.0 in properly normalized TDGL units

  // for now we set mu = 0 since we havent implemented terminals yet, so no current flowing
  // calculate the euler step variable w using temporal link var tU
  float mu      = 0.0f; // electric scalar potential felt at point, not implemented yet ####
  vec2 tU       = vec2(cos(- mu * uDt), sin(-mu * uDt));
  float phi2 = dot(phi, phi);

  // TDGL implicit Euler time-stepping (following pyTDGL Eq. 16-17)
  // ψ^{n+1} + z^n |ψ^{n+1}|^2 = w^n
  
  // Nonlinear coefficient z^n = (γ^2 / 2) * ψ^n
  vec2 z = 0.5f * uGamma * uGamma * phi;
  
  // Explicit RHS w^n = ψ^n + (Δt/u) * [GL terms] / sqrt(1 + γ^2 |ψ|^2)
  float prefactor = uDt / (uRelax * sqrt(1.0f + uGamma * uGamma * phi2));
  vec2 glTerms = (uEpsilon - phi2) * phi + lapScale * laplacian;
  vec2 w = phi + prefactor * glTerms;

  // quadratic solve for |ψ^{n+1}|^2 (pyTDGL Eq. 18-20)
  float w2 = dot(w, w);
  float z2 = dot(z, z);
  float c  = dot(z, w);  // Re(z* w)

  float disc = (2.0f*c + 1.0f)*(2.0f*c + 1.0f) - 4.0f*z2*w2;
  
  // Initialize phiNext at function scope
  vec2 phiNext;
  
  // Check for convergence (discriminant should be >= 0)
  if (disc < 0.0f) {
    // Fallback: use explicit Euler if quadratic fails
    phiNext = w;
  } else {
    // pyTDGL quadratic formula (modified to avoid division by zero)
    float s = (2.0f*w2) / ((2.0f*c + 1.0f) + sqrt(disc));
    phiNext = w - z * s;
  }


  int debugState = 0; float debug = 0.0f;
  if        (debugState == 1) {
    debug = phi2;  // Current |ψ|^2
  } else if (debugState == 2) {
    debug = dot(laplacian, laplacian);  // Laplacian magnitude
  } else if (debugState == 3) {
    debug = length(phiNext - phi);  // Change in ψ per step
  } else if (debugState == 4) {
    debug = disc;  // Quadratic discriminant
  } else if (debugState == 5) {
    debug = float(disc < 0.0f);  // Quadratic failure flag
  } else if (debugState == 6) {
    debug = atan(phiNext.y, phiNext.x) / (2.0f * 3.14159265359f) + 0.5f;  // Phase in [0,1]
  }
  

  // noise 
  if (uUseNoise) {
    float noiseAmp = 0.1; // fraction of equilibrium |ψ|
    vec2 seed1 = vec2(curr)+uTime*1.7321;
    vec2 seed2 = vec2(curr)+uTime*2.6457+1.23;
    float n1 = fract(sin(dot(seed1,vec2(12.9898,78.233)))*43758.5453);
    float n2 = fract(sin(dot(seed2,vec2(4.898,7.230)))*23421.631);
    vec2 noise = noiseAmp*vec2(n1-0.5,n2-0.5);
    phiNext += sqrt(uDt)*noise;
  }

  imageStore(img_out, curr, vec4(phiNext, debug, 0.0));
}
