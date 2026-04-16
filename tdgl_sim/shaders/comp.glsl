#version 430 core
layout(local_size_x=32,local_size_y=32) in;

layout(rgba32f, binding=0) uniform image2D img_in;
layout(rgba32f, binding=1) uniform image2D img_out;
layout(r32f   , binding=2) uniform image2D img_mask;
uniform sampler2D img_sampler;
uniform sampler2D img_neighbors;  // stores neighbor connectivity
uniform sampler2D img_edgeWeights;  // stores normalized edge distances (up, down, left, right)

uniform float uDt, uH, uL, uBField, uQ, uXi, uAlpha, uLambda, uGamma, uRelax, uEpsilon;
uniform bool uUseNoise;
uniform float uTime;
uniform ivec2 uPartitionOffset;  // offset for current partition (startX, startY)
uniform bool uComputeInterior;   // true: compute interior only, false: compute boundaries only
uniform ivec2 uInteriorStart;    // interior region start (for boundary mode)
uniform ivec2 uInteriorEnd;      // interior region end (for boundary mode)

vec2 cMult (vec2 a, vec2 b) {
  return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x);
}

vec2 cConj (vec2 a) {
  return vec2(a.x, -a.y);
}

void main() {
  // Compute actual cell coordinates with partition offset
  ivec2 local = ivec2(gl_GlobalInvocationID.xy);
  ivec2 curr = local + uPartitionOffset;
  ivec2 dim = imageSize(img_in);
  float mask = imageLoad(img_mask, curr).r;
  if (curr.x >= dim.x || curr.y >= dim.y) {return;}
  
  // Filter cells based on interior vs boundary computation mode
  bool isInterior = curr.x >= uInteriorStart.x && curr.x < uInteriorEnd.x &&
                    curr.y >= uInteriorStart.y && curr.y < uInteriorEnd.y;
  
  if (uComputeInterior && !isInterior) {return;}  // skip boundaries in interior phase
  if (!uComputeInterior && isInterior) {return;}  // skip interior in boundary phase

  // check for vacuum
  if (mask < 0.5) {
    imageStore(img_out, curr, vec4(0, 0, 0, 1));
    return;
  }

  vec2 phi = imageLoad(img_in, curr).rg; 

  // Get neighbor indices from connectivity texture (for generalized mesh support)
  vec2 texCoord = (vec2(curr) + 0.5f) / vec2(dim);
  vec4 neighborIndices = texture(img_neighbors, texCoord);
  
  // Decode neighbor cell IDs (stored as normalized indices)
  float totalCells = float(dim.x * dim.y);
  int upIdx    = int(round(neighborIndices.r * totalCells));
  int downIdx  = int(round(neighborIndices.g * totalCells));
  int leftIdx  = int(round(neighborIndices.b * totalCells));
  int rightIdx = int(round(neighborIndices.a * totalCells));
  
  // Convert linear indices back to 2D coordinates
  ivec2 upCoord    = ivec2(upIdx % dim.x, upIdx / dim.x);
  ivec2 downCoord  = ivec2(downIdx % dim.x, downIdx / dim.x);
  ivec2 leftCoord  = ivec2(leftIdx % dim.x, leftIdx / dim.x);
  ivec2 rightCoord = ivec2(rightIdx % dim.x, rightIdx / dim.x);

  // Fetch neighbors using the mesh-aware connectivity
  vec2 phiUp    = imageLoad(img_in, upCoord).rg;
  vec2 phiDown  = imageLoad(img_in, downCoord).rg;
  vec2 phiRight = imageLoad(img_in, rightCoord).rg;
  vec2 phiLeft  = imageLoad(img_in, leftCoord).rg;

  // Get edge weights from texture (normalized distances)
  vec4 edgeWeights = texture(img_edgeWeights, texCoord);
  float w_up    = edgeWeights.r;
  float w_down  = edgeWeights.g;
  float w_left  = edgeWeights.b;
  float w_right = edgeWeights.a;

  // calc link variables in landau gauge with B in z dir ==> A = (-B*y, 0, 0)
  // particle moving in x dir gets phase shift proportional to B*y*dx
  float yDist   = (float(curr.y) - 0.5f * float(dim.y - 1.0f)) * uH;     // y position relative to center
  float xPhase  = uQ * uBField * yDist * uH;                     // phase shift for x-links: q*B*y*dx
  
  // Wrap phase to [-π, π] for numerical stability in trig functions
  const float PI = 3.14159265359f;
  xPhase = mod(xPhase + PI, 2.0f*PI) - PI;  // Stable phase wrapping
  
  vec2 xU       = vec2(cos(xPhase), sin(xPhase));                 // x-direction link variable
  vec2 yU       = vec2(1.0, 0.0);                                 // y-direction link variable (no B field)

  // eq1 in pytdgl paper tells us that
  //      (u / sqrt(1 + gamma^2 |phi^2|)) * (d phi / dt + i*mu + (gamma^2 / 2) * (d|phi|^2 / dt)phi)...
  //            ... = epsilon*phi - |phi|^2 * phi + laplacian

  // covariant laplacian with distance weighting: weighted sum of U_ij * phi_j - phi_i
  // Each term is divided by (edge_length)^2 for proper discretization
  vec2 lapRight = (cMult(xU, phiRight)      - phi) / (w_right * w_right);
  vec2 lapLeft  = (cMult(cConj(xU), phiLeft) - phi) / (w_left * w_left);
  vec2 lapUp    = (cMult(yU, phiUp)         - phi) / (w_up * w_up);
  vec2 lapDown  = (cMult(cConj(yU), phiDown) - phi) / (w_down * w_down);
  
  vec2 laplacian = lapRight + lapLeft + lapUp + lapDown;
  laplacian     /= (uH * uH);       // normalize by grid spacing squared
  float lapScale = 1.0f; // gradient energy scaling; must be 1.0 in properly normalized TDGL units

  // for now we set mu = 0 since we havent implemented terminals yet, so no current flowing
  // calculate the euler step variable w using temporal link var tU
  float mu      = 0.0f; // electric scalar potential felt at point, not implemented yet ####
  vec2 tU       = vec2(cos(- mu * uDt), sin(-mu * uDt));
  float phi2 = dot(phi, phi);

  // TDGL implicit Euler time-stepping (following pyTDGL Eq. 16-17)
  // ψ^{n+1} + z^n |ψ^{n+1}|^2 = w^n
  
  // Nonlinear coefficient z^n = (γ^2 / 2) * ψ^n
  // Note: rescale z to improve conditioning of quadratic equation
  // Original: z = 0.5*gamma^2*phi, but this is O(0.005) when gamma=0.1, phi~1
  // Instead use normalized form to keep z O(1)
  float zScale = 0.5f * uGamma * uGamma;  // Accumulate scale factor
  vec2 z = zScale * phi;
  
  // Explicit RHS w^n = ψ^n + (Δt/u) * [GL terms] / sqrt(1 + γ^2 |ψ|^2)
  float denomFactor = sqrt(1.0f + uGamma * uGamma * phi2);
  float prefactor = uDt / (uRelax * denomFactor);
  vec2 glTerms = (uEpsilon - phi2) * phi + lapScale * laplacian;
  vec2 w = phi + prefactor * glTerms;

  // quadratic solve for |ψ^{n+1}|^2 (pyTDGL Eq. 18-20)
  // Equation: ψ + z|ψ|^2 = w, solved as quadratic in |ψ|^2
  float w2 = dot(w, w);
  float z2 = dot(z, z);
  float c  = dot(z, w);  // Re(z* w)

  // Discriminant of the quadratic
  float discriminant = (2.0f*c + 1.0f)*(2.0f*c + 1.0f) - 4.0f*z2*w2;
  
  // Initialize phiNext at function scope
  vec2 phiNext;
  
  // Numerically stable quadratic solver
  // Use alternative formulation to avoid cancellation errors
  if (discriminant < 0.0f || z2 < 1e-6f) {
    // Degenerate case: use explicit Euler fallback
    phiNext = w;
  } else {
    float sqrtDisc = sqrt(discriminant);
    float denom = (2.0f*c + 1.0f) + sqrt(discriminant);
    
    // Avoid division by zero or near-singular denominator
    if (abs(denom) > 1e-6f) {
      // Stable formulation: choose the branch that avoids cancellation
      float s = (2.0f*w2) / denom;
      phiNext = w - z * s;
    } else {
      // Use alternative formulation if denominator is small
      float altDenom = (2.0f*c + 1.0f) - sqrt(discriminant);
      if (abs(altDenom) > 1e-6f) {
        float s = discriminant / ((2.0f*c + 1.0f) - sqrt(discriminant));
        phiNext = w - z * s;
      } else {
        // Both branches dangerous; use explicit step
        phiNext = w;
      }
    }
  }


  int debugState = 0; float debug = 0.0f;
  if        (debugState == 1) {
    debug = phi2;  // Current |ψ|^2
  } else if (debugState == 2) {
    debug = dot(laplacian, laplacian);  // Laplacian magnitude
  } else if (debugState == 3) {
    debug = length(phiNext - phi);  // Change in ψ per step
  } else if (debugState == 4) {
    debug = discriminant;  // Quadratic discriminant
  } else if (debugState == 5) {
    debug = float(discriminant < 0.0f);  // Quadratic failure flag
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
