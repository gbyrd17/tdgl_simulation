#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord; // Must be declared to match your VBO

out vec2 fragCoord;

void main() {
    // Standard full-screen quad positioning
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    
    // Pass aPos directly to the fragment shader. 
    // This ensures every pixel gets a unique coordinate from -1 to 1.
    fragCoord = aPos; 
}
